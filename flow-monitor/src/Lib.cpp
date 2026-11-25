#include "Config.h"
#include "ConnectionPool.h"
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <mutex>
#include <new>
#include <sstream>
#include <stdarg.h>
#include <stdint.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fnmatch.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <tuple>
#include <string>
#include <fstream>
#include <iostream>
#include <string>
//#include "ErrorTester.h"
//#include "InputFile.h"
//#include "OutputFile.h"
//#include "LocalFile.h"
#include "RSocketAdapter.h"
#include "Request.h"
#include "ReaderWriterLock.h"
#include "MonitorFile.h"
#include "MonitorFileDescriptor.h"
#include "MonitorPathPolicy.h"
#include "Timer.h"
#include "Trackable.h"
#include "UnixIO.h"
#include "ThreadPool.h"
#include "PriorityThreadPool.h"
#include "Lib.h"
#include "UrlDownload.h"
#include "TrackFile.h"
#include <cassert>
#include <errno.h>

// #define printf(...) fprintf(stderr, __VA_ARGS__)
#ifdef LIBDEBUG
#define printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define printf(...)
#endif
#define MONITOR_ID "MONITOR"
#define MONITOR_ID_LEN 5 
#define MONITOR_VERSION "0.1"
#define MONITOR_VERSION_LEN 3 //5+3

#define TRACKFILECHANGES 1 // tmp-ly added

// pthread_atfork handlers. A forked child has only the forking thread, so a
// lock another thread held at fork() stays held in the child, and the child's
// next intercepted call that needs it hangs.
//
// Prepare takes the fd map, registry and trace-stat locks, outermost first as
// the I/O paths nest them (mmap takes a registry under the fd map lock, open
// takes the trace-stat lock under a registry lock). Each is held only for an
// in-memory update, so fork() waits briefly and the child gets them unlocked,
// with the maps they guard intact. vLock is held across the real I/O call, which
// can take long, so fork() does not wait for it; the child resets it instead.
//
// fork() called from a signal handler can interrupt this thread while it holds
// or is taking a DataLife lock, which only the interrupted code can release.
// Prepare then takes no locks and the child resets nothing, as if these
// handlers were not installed.
//
// Handlers registered before these run inside them: their prepare after ours,
// their parent and child handlers before ours. Their intercepted I/O counts as
// DataLife's own, so it bypasses monitoring and the locks held here.
//
// fork() calls in progress on this thread whose prepare took no locks (more
// than one if a signal handler forks during another fork).
static thread_local unsigned int skippedForks = 0;

static void monitorForkPrepare(void) {
    enterMonitorInternalIO();
    if (heldLocks() != 0) {
        ++skippedForks;
        return;
    }
    fdToFileMapMutex.lock();
    Trackable<std::string, MonitorFile *>::registryLock().writerLock();
    Trackable<int, MonitorFileDescriptor *>::registryLock().writerLock();
    trackFileStatLock().lock();
}

static void monitorForkRelease(void) {
    trackFileStatLock().unlock();
    Trackable<int, MonitorFileDescriptor *>::registryLock().writerUnlock();
    Trackable<std::string, MonitorFile *>::registryLock().writerUnlock();
    fdToFileMapMutex.unlock();
}

static void monitorForkParent(void) {
    if (skippedForks != 0) {
        --skippedForks;
    } else {
        monitorForkRelease();
    }
    leaveMonitorInternalIO();
}

// Allocates and frees nothing: a fork() from a signal handler can interrupt
// malloc, leaving this child's heap mid-update. Locks are reinitialized in place
// and the parent's data abandoned rather than freed.
static void monitorForkChild(void) {
    monitorPid = getpid();
    if (skippedForks != 0) {
        --skippedForks;
    } else {
        monitorForkRelease();
        vLock.resetAfterFork();
        new (Loggable::mtx_cout) std::mutex(); // the old one may be held
        if (timer) {
            timer->resetAfterFork();
        }
        trackFileForkChildReset();
    }
    leaveMonitorInternalIO();
}

void __attribute__((constructor)) monitorInit(void) {
    // // std::cout << "Lip.cpp: " << "monitorInit(void)" << std::endl;
    DPRINTF("Lip.cpp: monitorInit(void)\n");
    // std::cout << "Lib.cpp: monitorInit(void) start" << std::endl;

    std::call_once(log_flag, []() {
        timer = new Timer();
        timer->start();
        Loggable::mtx_cout = new std::mutex();
        // Created before other threads exist, so no fork() can happen while another
        // thread is creating them.
        trackFileInit();
        //InputFile::_time_of_last_read = new std::chrono::time_point<std::chrono::high_resolution_clock>();
        //InputFile::_cache = new Cache(BASECACHENAME, CacheType::base);
        //InputFile::_transferPool = new PriorityThreadPool<std::packaged_task<std::shared_future<Request *>()>>(1,"infile tx pool") ;
        //InputFile::_decompressionPool = new PriorityThreadPool<std::packaged_task<Request*()>>(Config::numClientDecompThreads,"infile comp pool");
        //OutputFile::_transferPool = new PriorityThreadPool<std::function<void()>>(1,"outfile tx pool");
        //OutputFile::_decompressionPool = new ThreadPool<std::function<void()>>(Config::numClientDecompThreads,"outfile comp pool");

        //LocalFile::_cache = new Cache(BASECACHENAME, CacheType::base);
        ConnectionPool::useCnt = new std::unordered_map<std::string, uint64_t>();
        ConnectionPool::consecCnt = new std::unordered_map<std::string, uint64_t>();
        ConnectionPool::stats = new std::unordered_map<std::string, std::pair<double, double>>();

        track_files = new std::unordered_set<std::string>();
        char *temp = getenv("MONITOR_LOCAL_FILES");
        if (temp) {
            std::stringstream files(temp);
            while (!files.eof()) {
                std::string f;
                getline(files, f, ' ');
                DPRINTF("Lib.cpp: %s\n",f.c_str());
                track_files->insert(f);
            }
        }
        
        curlInit;

        unixopen = (unixopen_t)dlsym(RTLD_NEXT, "open");
        unixopen64 = (unixopen_t)dlsym(RTLD_NEXT, "open64");
        unixopenat = (unixopenat_t)dlsym(RTLD_NEXT, "openat");
        unixclose = (unixclose_t)dlsym(RTLD_NEXT, "close");
        unixread = (unixread_t)dlsym(RTLD_NEXT, "read");
        unixwrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");
        unixlseek = (unixlseek_t)dlsym(RTLD_NEXT, "lseek");
        unixlseek64 = (unixlseek64_t)dlsym(RTLD_NEXT, "lseek64");
        // unixlstat = (unixlstat_t)dlsym(RTLD_NEXT, "lstat");
        unixxstat = (unixxstat_t)dlsym(RTLD_NEXT, "__xstat");
        unixxstat64 = (unixxstat64_t)dlsym(RTLD_NEXT, "__xstat64");
        unixlxstat = (unixxstat_t)dlsym(RTLD_NEXT, "__lxstat");
        unixlxstat64 = (unixxstat64_t)dlsym(RTLD_NEXT, "__lxstat64");
        unixfsync = (unixfsync_t)dlsym(RTLD_NEXT, "fsync");
        unixrename = (unixrename_t)dlsym(RTLD_NEXT, "rename");
        unixrenameat = (unixrenameat_t)dlsym(RTLD_NEXT, "renameat");
        unixrenameat2 = (unixrenameat2_t)dlsym(RTLD_NEXT, "renameat2");
        unixfopen = (unixfopen_t)dlsym(RTLD_NEXT, "fopen");
        unixfopen64 = (unixfopen_t)dlsym(RTLD_NEXT, "fopen64");
        unixfdopen = (unixfdopen_t)dlsym(RTLD_NEXT, "fdopen");
        unixfclose = (unixfclose_t)dlsym(RTLD_NEXT, "fclose");
        unixfread = (unixfread_t)dlsym(RTLD_NEXT, "fread");
        unixfwrite = (unixfwrite_t)dlsym(RTLD_NEXT, "fwrite");
        unixftell = (unixftell_t)dlsym(RTLD_NEXT, "ftell");
        // unixfstat = (unixfstat_t)dlsym(RTLD_NEXT, "fstat");
        unixfseek = (unixfseek_t)dlsym(RTLD_NEXT, "fseek");
        unixfseeko = (unixfseeko_t)dlsym(RTLD_NEXT, "fseeko");
        unixfseeko64 = (unixfseeko64_t)dlsym(RTLD_NEXT, "fseeko64");
        unixrewind = (unixrewind_t)dlsym(RTLD_NEXT, "rewind");
        unixfgetc = (unixfgetc_t)dlsym(RTLD_NEXT, "fgetc");
        unixfgets = (unixfgets_t)dlsym(RTLD_NEXT, "fgets");
        unixfread_unlocked = (unixfread_t)dlsym(RTLD_NEXT, "fread_unlocked");
        unixfgets_unlocked = (unixfgets_t)dlsym(RTLD_NEXT, "fgets_unlocked");
        unix__fgets_chk = (unixfgets_chk_t)dlsym(RTLD_NEXT, "__fgets_chk");
        unix__fgets_unlocked_chk = (unixfgets_chk_t)dlsym(RTLD_NEXT, "__fgets_unlocked_chk");
        unixfputc = (unixfputc_t)dlsym(RTLD_NEXT, "fputc");
        unixfputs = (unixfputs_t)dlsym(RTLD_NEXT, "fputs");
        unixflockfile = (unixflockfile_t)dlsym(RTLD_NEXT, "flockfile");
        unixftrylockfile = (unixftrylockfile_t)dlsym(RTLD_NEXT, "ftrylockfile");
        unixfunlockfile = (unixfunlockfile_t)dlsym(RTLD_NEXT, "funlockfile");
        unixfflush = (unixfflush_t)dlsym(RTLD_NEXT, "fflush");
        unixfeof = (unixfeof_t)dlsym(RTLD_NEXT, "feof");
        unix__uflow = (unixuflow_t)dlsym(RTLD_NEXT, "__uflow");
        unix__underflow = (unixuflow_t)dlsym(RTLD_NEXT, "__underflow");
        unix__overflow = (unixoverflow_t)dlsym(RTLD_NEXT, "__overflow");
        unixreadv = (unixreadv_t)dlsym(RTLD_NEXT, "readv");
        unixwritev = (unixwritev_t)dlsym(RTLD_NEXT, "writev");
        unixexit = (unixexit_t)dlsym(RTLD_NEXT, "exit");
        unix_exit = (unix_exit_t)dlsym(RTLD_NEXT, "_exit");
        unix_Exit = (unix_Exit_t)dlsym(RTLD_NEXT, "_Exit");
        unix_exit_group = (unix_exit_group_t)dlsym(RTLD_NEXT, "exit_group");
        unix_vfprintf = (unix_vfprintf_t)dlsym(RTLD_NEXT, "vfprintf");
        unixmmap = (mmap_t)dlsym(RTLD_NEXT, "mmap");
        unixmmap64 = (mmap64_t)dlsym(RTLD_NEXT, "mmap64");
        // // Below added for HDF5 POSIX I/O
        unixpread = (pread_t)dlsym(RTLD_NEXT, "pread");
        unixpwrite = (pwrite_t)dlsym(RTLD_NEXT, "pwrite");
        unixpread64 = (pread64_t)dlsym(RTLD_NEXT, "pread64");
        unixpwrite64 = (pwrite64_t)dlsym(RTLD_NEXT, "pwrite64");

        // Keep a forked child from hanging on a lock another thread held at
        // fork(), and from reporting the parent's I/O as its own. Registered
        // after the pointers above: I/O from other fork handlers takes the
        // internal-I/O bypasses, and some of them call these pointers.
        pthread_atfork(monitorForkPrepare, monitorForkParent, monitorForkChild);

        //enable if running into issues with an application that launches child shells
        bool unsetLib = getenv("MONITOR_UNSET_LIB") ? atoi(getenv("MONITOR_UNSET_LIB")) : 0;
        if (unsetLib){
            unsetenv("LD_PRELOAD");
        }


        timer->end(Timer::MetricType::monitor, Timer::Metric::constructor);
        //*InputFile::_time_of_last_read = std::chrono::high_resolution_clock::now();
    });
    monitorPid = getpid();
    init = true;
}

static void recordStandardStreams(void);

void __attribute__((destructor)) monitorCleanup(void) {
    // Removed the manual destructor call.

    DPRINTF("Lib.cpp: monitorCleanup(void)\n");
    recordStandardStreams();

    timer->start();
    init = false; //set to false because we can't ensure our static members have not already been deleted.

    curlEnd(Config::curlOnStartup);
    curlDestroy;

    if (Config::printStats) {
        std::cerr << "[MONITOR] " << "Exiting Client" << std::endl;
        if (ConnectionPool::useCnt->size() > 0) {
            for (auto conUse : *ConnectionPool::useCnt) {
                std::cerr << "[MONITOR] connection: " << conUse.first << " num_tx: " << conUse.second << " amount: " << (*ConnectionPool::stats)[conUse.first].first << " B time: " << (*ConnectionPool::stats)[conUse.first].second << " s avg BW: " << ((*ConnectionPool::stats)[conUse.first].first / (*ConnectionPool::stats)[conUse.first].second) / 1000000 << "MB/s" << std::endl;
            }
        }
        delete track_files;
        delete ConnectionPool::useCnt;
        delete ConnectionPool::consecCnt;
        delete ConnectionPool::stats;
    }

    timer->end(Timer::MetricType::monitor, Timer::Metric::destructor);

    timer->start();
    FileCacheRegister::closeFileCacheRegister();
    ConnectionPool::removeAllConnectionPools();
    Connection::closeAllConnections();
    timer->end(Timer::MetricType::monitor, Timer::Metric::destructor);

    delete timer;
}

std::string getFileNameFromFd(int fd) {
    std::lock_guard<CountedMutex> lock(fdToFileMapMutex);
    auto it = fdToFileMap.find(fd);
    if (it != fdToFileMap.end()) {
        return it->second;
    }
    return "";
}



int trackFileOpen(std::string name, std::string metaName, MonitorFile::Type type, const char *pathname, int flags, int mode) {
    DPRINTF("Lib.cpp: trackfileOpen: %s %s %u\n", name.c_str(), metaName.c_str(), type);


  auto fd = (*unixopen64)(name.c_str(), flags, mode);
  if (fd > 0) {

    MonitorFile *file = MonitorFile::addNewMonitorFile(type, name, name, fd, true);
    if (file) {
        bool descriptorAdded = MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex(fd));
        if (descriptorAdded) {
            file->open();
            DPRINTF("Lib.cpp: trackFileOpen() add new file success: %s , fd = %d\n", pathname, fd);
        } else {
            DPRINTF("Lib.cpp: trackFileOpen() Failed to add monitor file descriptor for file: %s, fd = %d\n", pathname, fd);
            // // Optionally clean up the `MonitorFile` object if the descriptor addition fails
            // MonitorFile::removeMonitorFile(file);
            // close(fd);
            // fd = -1; // Indicate failure
        }
    } 

  } else {
    DPRINTF("Lib.cpp: fd value %d\n", fd);
    DPRINTF("Lib.cpp: Error opening file: %s flag: %d mode: %d \n", strerror(errno), flags, mode);
  }
  return fd;
}

/*Posix******************************************************************************************************/

int monitorOpen(std::string name, std::string metaName, MonitorFile::Type type, const char *pathname, int flags, int mode) {
  auto fd = (*unixopen64)(metaName.c_str(), O_RDONLY, 0);
  MonitorFile *file = MonitorFile::addNewMonitorFile(type, name, metaName, fd);
  if (file) {
    // MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex(fd));
    bool descriptorAdded = MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex(fd));
    if (descriptorAdded) {
        file->open();
        DPRINTF("Lib.cpp: monitorOpen add new file success: %s , fd = %d\n", pathname, fd);
    } else {
        DPRINTF("Lib.cpp: monitorOpen() Failed to add monitor file descriptor for file: %s, fd = %d\n", pathname, fd);
        // Optionally clean up the `MonitorFile` object if the descriptor addition fails
        MonitorFile::removeMonitorFile(file);
        close(fd);
        fd = -1; // Indicate failure
    }
  } else if(fd != -1) {
    DPRINTF("Lib.cpp: monitorOpen add new monitor file failed: %s %s %d\n", name.c_str(), metaName.c_str(), fd);
    (*unixclose)(fd);
    fd = -1;
  }
  return fd;
}

// Whether open() and its variants take a mode argument with these flags.
static inline bool openNeedsMode(int flags) {
    return (flags & O_CREAT)
#ifdef O_TMPFILE
        || ((flags & O_TMPFILE) == O_TMPFILE)
#endif
    ;
}

int open(const char *pathname, int flags, ...) {
    DPRINTF("Lib.cpp: Open %s: \n", pathname);

    int mode = 0;
    if (openNeedsMode(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
    }

    if (leavesRecordsAlone() || isMonitorOwnedPath(pathname, Config::dataLifeOutputPath)) {
        return (*unixopen)(pathname, flags, mode);
    }

    Timer::Metric metric = (flags & O_WRONLY || flags & O_RDWR) ? Timer::Metric::out_open : Timer::Metric::in_open;

    // Check if the file matches any pattern
    for (auto pattern : patterns) {
        if (fnmatch(pattern.c_str(), get_basename(pathname), 0) == 0) {
            DPRINTF("Lib.cpp: open() Firing off trackFileOpen for %s\n", pathname);
            int fd = outerWrapper("open", pathname, metric, trackFileOpen, unixopen, pathname, flags, mode);


            if (fd >= 0 && init) {
                // Track the fd and pathname
                std::lock_guard<CountedMutex> lock(fdToFileMapMutex);
                fdToFileMap[fd] = std::string(pathname);
            } else {
                DPRINTF("Lib.cpp: trackFileOpen failed for %s\n", pathname);
            }
            return fd;
        }
    }

    // If no pattern matched, fallback to monitorOpen
    DPRINTF("Lib.cpp: open() Firing off monitorOpen for %s\n", pathname);
    int fd = outerWrapper("open", pathname, metric, monitorOpen, unixopen, pathname, flags, mode);

    if (fd >= 0 && init) {
        // Track the fd and pathname
        std::lock_guard<CountedMutex> lock(fdToFileMapMutex);
        fdToFileMap[fd] = std::string(pathname);
    } else {
        DPRINTF("Lib.cpp: monitorOpen failed for %s\n", pathname);
    }

    return fd;
}

int open64(const char *pathname, int flags, ...) {
    DPRINTF("Lib.cpp: Open64 %s: \n", pathname);

    int mode = 0;
    if (openNeedsMode(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
    }

    if (leavesRecordsAlone() || isMonitorOwnedPath(pathname, Config::dataLifeOutputPath)) {
        return (*unixopen64)(pathname, flags, mode);
    }

    Timer::Metric metric = (flags & O_WRONLY || flags & O_RDWR) ? Timer::Metric::out_open : Timer::Metric::in_open;

    // Check if the file matches any pattern
    for (auto pattern : patterns) {
        if (fnmatch(pattern.c_str(), get_basename(pathname), 0) == 0) {
            int fd = outerWrapper("open64", pathname, metric, trackFileOpen, unixopen64, pathname, flags, mode);

            if (fd >= 0 && init) {
                // Track the fd and pathname
                std::lock_guard<CountedMutex> lock(fdToFileMapMutex);
                fdToFileMap[fd] = std::string(pathname);
            } else {
                DPRINTF("Lib.cpp: trackFileOpen failed for %s\n", pathname);
            }
            return fd;
        }
    }

    // If no pattern matched, fallback to monitorOpen
    DPRINTF("Lib.cpp: open64() Firing off monitorOpen for %s\n", pathname);
    int fd = outerWrapper("open64", pathname, metric, monitorOpen, unixopen64, pathname, flags, mode);

    if (fd >= 0 && init) {
        // Track the fd and pathname
        std::lock_guard<CountedMutex> lock(fdToFileMapMutex);
        fdToFileMap[fd] = std::string(pathname);
    } else {
        DPRINTF("Lib.cpp: monitorOpen failed for %s\n", pathname);
    }

    return fd;
}


int monitorOpenat(std::string name, std::string metaName, MonitorFile::Type type,
		int dirfd, const char *pathname, int flags, int mode) {
  return (*unixopenat)(dirfd, name.c_str(), flags, mode);
}

int trackFileOpenat(std::string name, std::string metaName, MonitorFile::Type type, 
		    int dirfd, const char *pathname, int flags, int mode) {
  DPRINTF("Lib.cpp: trackfileOpenat: %s %s %u\n", name.c_str(), metaName.c_str(), type);
  auto fd = (*unixopenat)(dirfd, name.c_str(), flags, mode);
  if (fd > 0) {  
    MonitorFile *file = MonitorFile::addNewMonitorFile(type, name, name, fd, true);
    if (file) {
      MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex(fd));
      DPRINTF("Lib.cpp: trackFileOpen add new  file success: %s , fd = %d\n", pathname, fd);
    } 
  } else {
    DPRINTF("Lib.cpp: fd value %d\n", fd);
  }
  return fd;
}

static int openatImpl(int dirfd, const char *pathname, int flags, int mode) {
  if (leavesRecordsAlone() || isMonitorOwnedPathAt(dirfd, pathname, Config::dataLifeOutputPath)) {
    return (*unixopenat)(dirfd, pathname, flags, mode);
  }
  
  Timer::Metric metric = (flags & O_WRONLY || flags & O_RDWR) ? 
    Timer::Metric::out_open : Timer::Metric::in_open;

  DPRINTF("Lib.cpp: Openat %s: \n", pathname);
  for (auto pattern: patterns) {
    auto ret_val = fnmatch(pattern.c_str(), get_basename(pathname), 0);
    if (ret_val == 0) {
      DPRINTF("Lib.cpp: Firing off trackfileopen for %s \n ", pathname);

      return outerWrapper("openat", pathname, metric, trackFileOpenat, unixopenat, 
			  dirfd, pathname, flags, mode);
    }
  }

  return outerWrapper("openat", pathname, metric, monitorOpenat, unixopenat, dirfd, 
		      pathname, flags, mode);
}

int openat(int dirfd, const char *pathname, int flags, ...) {
  int mode = 0;
  if (openNeedsMode(flags)) {
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);
  }
  return openatImpl(dirfd, pathname, flags, mode);
}

// What programs built with _FILE_OFFSET_BITS=64 call for openat(). The real one
// is openat() with O_LARGEFILE, which is 0 or implied wherever off_t has 64 bits.
int openat64(int dirfd, const char *pathname, int flags, ...) {
  int mode = 0;
  if (openNeedsMode(flags)) {
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);
  }
  return openatImpl(dirfd, pathname, flags | O_LARGEFILE, mode);
}

int monitorClose(MonitorFile *file, unsigned int fp, int fd) {
    DPRINTF("Lib.cpp: In monitor close for fd %d\n", fd);
    MonitorFileDescriptor::removeMonitorFileDescriptor(fd, fd);
    return (*unixclose)(fd);
}

int close(int fd) {
    DPRINTF("Lib.cpp: Trying to close file with fd %d\n", fd);
    // DataLife's own I/O can run inside the fork handlers, which hold the fd
    // map and registry locks.
    if (!init || leavesRecordsAlone()) {
        unixclose_t realClose = unixclose ? unixclose : (unixclose_t)dlsym(RTLD_NEXT, "close");
        return (*realClose)(fd);
    }
    {
        std::lock_guard<CountedMutex> lock(fdToFileMapMutex);
        fdToFileMap.erase(fd);
    }
    return outerWrapper("close", fd, Timer::Metric::close, monitorClose, unixclose, fd);
}


#ifdef TRACKRESOURCE
void exit(int status) {
  DPRINTF("Lib.cpp: Calling exit \n");
  auto iter = Trackable<int, MonitorFileDescriptor *>::begin();
  while (true) {
    auto next_iter = Trackable<int, MonitorFileDescriptor *>::next(iter);
    if (next_iter == Trackable<int, MonitorFileDescriptor *>::end()) {
      break;
    }
    auto fd_ = next_iter->first;
    auto file_desc = next_iter->second;
    auto file = file_desc->getFile();
    file->close(fd_);
    iter = next_iter;
  }
  (*unixexit)(status);
  // return outerWrapper("exit", fd, Timer::Metric::close, monitorClose, unixclose, fd);
}

void _exit(int status) {
  DPRINTF("Lib.cpp: Calling _exit \n");
  (*unix_exit)(status);
}

void _Exit(int status) {
  DPRINTF("Lib.cpp: Calling _Exit \n");
  (*unix_Exit)(status);
}

void exit_group(int status) {
  DPRINTF("Lib.cpp: Calling exit_group \n");
  (*unix_exit_group)(status);
}
#endif

/* Renames ***************************************************************************************************/

// After a rename succeeded. Names are compared as the program spells them, like
// everywhere else: a file opened as "tmp/x" is not found as "./tmp/x".
static void followRename(const char *oldpath, const char *newpath, bool exchange) {
    if (!init || isMonitorInternalIO() || !oldpath || !newpath) {
        return;
    }
    int saved_errno = errno; // the rename succeeded: nothing here may change it
    const std::string from(oldpath);
    const std::string to(newpath);
    if (from == to) {
        errno = saved_errno;
        return;
    }
    // Two files that swap names keep their records: the swap is only noted.
    bool followed = !exchange && trackFileRenamed(from, to);
    if (followed) {
        // mmap() finds the file of a descriptor by its name.
        std::lock_guard<CountedMutex> lock(fdToFileMapMutex);
        for (auto &entry : fdToFileMap) {
            if (entry.second == from) {
                entry.second = to;
            }
        }
    }
    if (followed || MonitorFile::matchesPatterns(from) || MonitorFile::matchesPatterns(to)) {
        Timer::recordRename(from, to, followed);
    }
    errno = saved_errno;
}

extern "C" {

int rename(const char *oldpath, const char *newpath) ADD_THROW {
    unixrename_t real = unixrename ? unixrename : (unixrename_t)dlsym(RTLD_NEXT, "rename");
    int ret = (*real)(oldpath, newpath);
    if (ret == 0) {
        followRename(oldpath, newpath, false);
    }
    return ret;
}

int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) ADD_THROW {
    unixrenameat_t real = unixrenameat ? unixrenameat : (unixrenameat_t)dlsym(RTLD_NEXT, "renameat");
    int ret = (*real)(olddirfd, oldpath, newdirfd, newpath);
    if (ret == 0) {
        followRename(oldpath, newpath, false);
    }
    return ret;
}

// What mv calls. In glibc since 2.28: a program that calls it runs with a C
// library that has it.
int renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, unsigned int flags) ADD_THROW {
    unixrenameat2_t real = unixrenameat2 ? unixrenameat2 : (unixrenameat2_t)dlsym(RTLD_NEXT, "renameat2");
    if (!real) {
        errno = ENOSYS;
        return -1;
    }
    int ret = (*real)(olddirfd, oldpath, newdirfd, newpath, flags);
    if (ret == 0) {
#ifdef RENAME_EXCHANGE
        const bool exchange = (flags & RENAME_EXCHANGE) != 0;
#else
        const bool exchange = (flags & 2) != 0;
#endif
        followRename(oldpath, newpath, exchange);
    }
    return ret;
}

} // extern "C"

// vLock around one call on a monitored file: shared for read and lseek, exclusive
// for write. Regular files only: a read on a FIFO can block until another thread
// writes to it.
class MonitoredIOLock {
  public:
    MonitoredIOLock(MonitorFile *file, bool exclusive) : _exclusive(exclusive), _locked(file->regularFile()) {
        if (!_locked) {
            return;
        }
        if (_exclusive) {
            vLock.writerLock();
        } else {
            vLock.readerLock();
        }
    }

    ~MonitoredIOLock() {
        if (!_locked) {
            return;
        }
        if (_exclusive) {
            vLock.writerUnlock();
        } else {
            vLock.readerUnlock();
        }
    }

    MonitoredIOLock(const MonitoredIOLock &) = delete;
    MonitoredIOLock &operator=(const MonitoredIOLock &) = delete;

  private:
    bool _exclusive;
    bool _locked;
};

ssize_t monitorRead(MonitorFile *file, unsigned int fp, int fd, void *buf, size_t count) {
  DPRINTF("Lib.cpp: In Monitor read\n");
  MonitoredIOLock lock(file, false);
  ssize_t ret = file->read(fd, buf, count, fp);
  if (ret > 0) {
    timer->addAmt(Timer::MetricType::monitor, Timer::Metric::read, ret);
  }
  return ret;
}

ssize_t read(int fd, void *buf, size_t count) {
    DPRINTF("Lib.cpp: Invoking read fd: %d count: %u\n", fd, count);
    return outerWrapper("read", fd, Timer::Metric::read, monitorRead, unixread, fd, buf, count);
}

ssize_t monitorWrite(MonitorFile *file, unsigned int fp, int fd, const void *buf, size_t count) {
    DPRINTF("Lib.cpp: In Monitor write\n");
    MonitoredIOLock lock(file, true);
    // auto ret = file->write(buf, count, fp);
    // Pass -1 for offset to indicate positional write
    auto ret = file->write(fd, buf, count, fp, -1);
    if (ret > 0) {
        timer->addAmt(Timer::MetricType::monitor, Timer::Metric::write, ret);
    }
    DPRINTF("Lib.cpp: Returning from Monitor write\n");
    return ret;
}

ssize_t write(int fd, const void *buf, size_t count) {
    DPRINTF("Lib.cpp: Invoking write fd: %d count: %u\n", fd, count);
    return outerWrapper("write", fd, Timer::Metric::write, monitorWrite, unixwrite, fd, buf, count);
}

template <typename T>
T monitorLseek(MonitorFile *file, unsigned int fp, int fd, T offset, int whence) {
    MonitoredIOLock lock(file, false);
    return (T)file->seek(fd, offset, whence, fp);
}

off_t lseek(int fd, off_t offset, int whence) ADD_THROW {
    return outerWrapper("lseek", fd, Timer::Metric::seek, monitorLseek<off_t>, unixlseek, fd, offset, whence);
}

off64_t lseek64(int fd, off64_t offset, int whence) ADD_THROW {
    return outerWrapper("lseek64", fd, Timer::Metric::seek, monitorLseek<off64_t>, unixlseek64, fd, offset, whence);
}

thread_local unixxstat_t whichStat = NULL;
int innerStat(int version, const char *filename, struct stat *buf) { return whichStat(version, filename, buf); }

thread_local unixxstat64_t whichStat64 = NULL;
int innerStat(int version, const char *filename, struct stat64 *buf) { return whichStat64(version, filename, buf); }

// thread_local unixlstat_t whichLstat = NULL;
// thread_local unixfstat_t whichFstat = NULL;

template <typename T>
int monitorStat(std::string name, std::string metaName, MonitorFile::Type type, int version, const char *filename, T *buf) {
//   auto ret = innerStat(_STAT_VER, metaName.c_str(), buf);
  #ifdef _STAT_VER
    auto ret = innerStat(_STAT_VER, metaName.c_str(), buf);
  #else
    auto ret = innerStat(version, metaName.c_str(), buf);
  #endif

  MonitorFile *file = MonitorFile::lookUpMonitorFile(filename);
  if (file)
    buf->st_size = (off_t)file->fileSize();
  else {
    if(type == MonitorFile::Type::TrackLocal) {
      return ret;
    }

    int fd = (*unixopen)(metaName.c_str(), O_RDONLY, 0);
    /*if(type == MonitorFile::Type::Input) {
      InputFile tempFile(name, metaName, fd, false);
      buf->st_size = tempFile.fileSize();
    }
    else if(type == MonitorFile::Type::Local) {
      int urlSize = supportedUrlType(name) ? sizeUrlPath(name) : -1;
      if(urlSize > -1) {
	if(Config::downloadForSize)
	  buf->st_size = urlSize;
	else if(urlSize == 0)
	  buf->st_size = 1;
      }
      else {
	LocalFile tempFile(name, metaName, fd, false);
	buf->st_size = tempFile.fileSize();
      }
    }*/

    (*unixclose)(fd);
  }
  return ret;
}

int __xstat(int version, const char *filename, struct stat *buf) ADD_THROW {
    whichStat = unixxstat;
    return outerWrapper("__xstat", filename, Timer::Metric::stat, monitorStat<struct stat>, unixxstat, version, filename, buf);
}

int __xstat64(int version, const char *filename, struct stat64 *buf) ADD_THROW {
    whichStat64 = unixxstat64;
    return outerWrapper("__xstat64", filename, Timer::Metric::stat, monitorStat<struct stat64>, unixxstat64, version, filename, buf);
}

int __lxstat(int version, const char *filename, struct stat *buf) ADD_THROW {
    whichStat = unixxstat;
    return outerWrapper("__lxstat", filename, Timer::Metric::stat, monitorStat<struct stat>, unixlxstat, version, filename, buf);
}

int __lxstat64(int version, const char *filename, struct stat64 *buf) ADD_THROW {
    whichStat64 = unixlxstat64;
    return outerWrapper("__lxstat64", filename, Timer::Metric::stat, monitorStat<struct stat64>, unixlxstat64, version, filename, buf);
}

int monitorFsync(MonitorFile *file, unsigned int fp, int fd) {
    // Monitored files are local files: the caller relies on its data being on disk.
    return (*unixfsync)(fd);
}

int fsync(int fd) {
    return outerWrapper("fsync", fd, Timer::Metric::stat, monitorFsync, unixfsync, fd);
}

// readv and writev on a monitored file make the real call, so the transfer stays
// one atomic call that can return short, then record its bytes as one read or
// write ending at the descriptor's new offset.
static void trackVector(MonitorFile *file, unsigned int fp, int fd, ssize_t bytes, bool isRead) {
    off_t pos = (*unixlseek)(fd, 0, SEEK_CUR);
    if (pos < 0) {
        pos = file->filePos(fp) + bytes; // not seekable (a FIFO): advance as read() does
    }
    file->setFilePos(fp, pos);
    if (auto *trackFile = dynamic_cast<TrackFile *>(file)) {
        if (isRead) {
            trackFile->trackRead(bytes, fp, pos);
        } else {
            trackFile->trackWrite(bytes, fp, pos);
        }
    }
}

ssize_t monitorReadv(MonitorFile *file, unsigned int fp, int fd, const struct iovec *iov, int iovcnt) {
    MonitoredIOLock lock(file, false);
    ssize_t ret = (*unixreadv)(fd, iov, iovcnt);
    if (ret > 0) {
        trackVector(file, fp, fd, ret, true);
        timer->addAmt(Timer::MetricType::monitor, Timer::Metric::readv, ret);
    }
    return ret;
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return outerWrapper("readv", fd, Timer::Metric::readv, monitorReadv, unixreadv, fd, iov, iovcnt);
}

ssize_t monitorWritev(MonitorFile *file, unsigned int fp, int fd, const struct iovec *iov, int iovcnt) {
    MonitoredIOLock lock(file, true);
    ssize_t ret = (*unixwritev)(fd, iov, iovcnt);
    if (ret > 0) {
        trackVector(file, fp, fd, ret, false);
        timer->addAmt(Timer::MetricType::monitor, Timer::Metric::writev, ret);
    }
    return ret;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return outerWrapper("writev", fd, Timer::Metric::writev, monitorWritev, unixwritev, fd, iov, iovcnt);
}

/* More calls on descriptors **********************************************************************************/

// The next definition of a symbol, libc's, looked up at the first call and kept
// in `cache`, a static of the wrapper. Not a static that is initialized with
// the lookup: its guard would stop for good a wrapper that a signal handler
// calls during the lookup, or the child of a fork() made during it.
template <typename Function>
static Function nextSymbol(std::atomic<Function> &cache, const char *name) {
    Function function = cache.load(std::memory_order_relaxed);
    if (!function) {
        function = (Function)dlsym(RTLD_NEXT, name);
        cache.store(function, std::memory_order_relaxed);
    }
    return function;
}
#define NEXT_SYMBOL(Function, name)                    \
    ([]() -> Function {                                \
        static std::atomic<Function> cache{nullptr};   \
        return nextSymbol(cache, name);                \
    }())

// Whether the C library has no such function: one older than the function. A
// program can come by the wrapper all the same, with dlsym() or through a weak
// reference, and must find what it would without the library.
template <typename Function>
static bool missing(Function real) {
    if (real) {
        return false;
    }
    errno = ENOSYS;
    return true;
}

// Defines the function that programs call as `name`. In this file it goes by
// another name: the C library's headers may already define `name` inline, make
// it a macro, or have it stand for another symbol (scanf() for __isoc23_scanf()).
#define INTERPOSE(type, name, parameters)                         \
    extern "C" type monitor_##name parameters __asm__(#name);     \
    type monitor_##name parameters

INTERPOSE(int, creat, (const char *pathname, mode_t mode)) {
    return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

INTERPOSE(int, creat64, (const char *pathname, mode_t mode)) {
    return open64(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

// mkstemp() and its variants open a new file under a name they choose and
// write that name over the template. The file is monitored if the name matches
// a pattern, like a file opened by name; a program that renames it to its
// result is followed there (see followRename()). `real` makes the call.
template <typename Real>
static int openTemporary(const char *name, Real real) {
    if (!init || leavesRecordsAlone()) {
        return real();
    }
    timer->start();
    int fd = real();
    bool monitored = false;
    if (fd >= 0 && MonitorFile::matchesPatterns(name) && !isMonitorOwnedPath(name, Config::dataLifeOutputPath)) {
        int saved_errno = errno;
        MonitorFile *file = MonitorFile::addNewMonitorFile(MonitorFile::TrackLocal, name, name, fd);
        if (file) {
            MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex(fd));
            monitored = true;
        }
        {
            // mmap() finds the file of a descriptor by its name.
            std::lock_guard<CountedMutex> lock(fdToFileMapMutex);
            fdToFileMap[fd] = std::string(name);
        }
        errno = saved_errno;
    }
    timer->end(monitored ? Timer::MetricType::monitor : Timer::MetricType::system, Timer::Metric::out_open);
    return fd;
}

#define OPEN_TEMPORARY(name, parameters, arguments)                                          \
    INTERPOSE(int, name, parameters) {                                                       \
        auto real = NEXT_SYMBOL(int (*) parameters, #name);                                  \
        return openTemporary(pattern, [&] { return (*real) arguments; });                    \
    }
OPEN_TEMPORARY(mkstemp, (char *pattern), (pattern))
OPEN_TEMPORARY(mkstemp64, (char *pattern), (pattern))
OPEN_TEMPORARY(mkostemp, (char *pattern, int flags), (pattern, flags))
OPEN_TEMPORARY(mkostemp64, (char *pattern, int flags), (pattern, flags))
OPEN_TEMPORARY(mkstemps, (char *pattern, int suffix), (pattern, suffix))
OPEN_TEMPORARY(mkstemps64, (char *pattern, int suffix), (pattern, suffix))
OPEN_TEMPORARY(mkostemps, (char *pattern, int suffix, int flags), (pattern, suffix, flags))
OPEN_TEMPORARY(mkostemps64, (char *pattern, int suffix, int flags), (pattern, suffix, flags))

typedef int (*unixfcntl_t)(int fd, int command, ...);

// `copy` has become a copy of fd, by dup() or the like: the same open file, with
// the same offset. It is monitored if fd is, and like it, at the same position
// (see MonitorFile::newFilePosIndexSharing()): what goes through the one moves
// the other. Only what a stream on it has recorded is the copy's own. The file
// it was a descriptor of until now, for dup2() and dup3(), is closed. That one's size can
// still be read through `kept`. The records change in one step, after the call,
// so that what other threads do with `copy` meanwhile is recorded for one file
// or the other.
static void copiedDescriptor(int fd, int copy, int kept = -1) {
    if (copy < 0 || copy == fd || !init || leavesRecordsAlone()) {
        return;
    }
    int saved_errno = errno;
    MonitorFile *closed;
    {
        MonitoredFile monitored(fd);
        unsigned int pos = 0;
        if (monitored) {
            monitored.file->incUsers(); // for the copy
            pos = monitored.file->newFilePosIndexSharing(monitored.pos);
        }
        closed = MonitorFileDescriptor::replaceMonitorFileDescriptor(copy, monitored.file, pos);
        // mmap() finds the file of a descriptor by its name.
        std::lock_guard<CountedMutex> lock(fdToFileMapMutex);
        auto name = fdToFileMap.find(fd);
        if (name != fdToFileMap.end()) {
            fdToFileMap[copy] = std::string(name->second);
        } else {
            fdToFileMap.erase(copy);
        }
    }
    MonitorFileDescriptor::closeMonitorFile(closed, kept);
    errno = saved_errno;
}

INTERPOSE(int, dup, (int fd)) {
    int copy = (*NEXT_SYMBOL(int (*)(int), "dup"))(fd);
    copiedDescriptor(fd, copy);
    return copy;
}

// dup2() and dup3() close the file of the descriptor they make the copy on. If
// that file is monitored, a descriptor of it is kept until its traces are
// written: they tell its size. Not one of the first three numbers, which a
// program may be counting on for its next open().
template <typename Real>
static int copyOnto(int fd, int onto, Real real) {
    unixfcntl_t control = NEXT_SYMBOL(unixfcntl_t, "fcntl");
    int kept = -1;
    if (init && fd != onto && MonitorFileDescriptor::mayBeMonitored(onto) && !leavesRecordsAlone()) {
        int saved_errno = errno;
        kept = (*control)(onto, F_DUPFD_CLOEXEC, 3);
        errno = saved_errno;
    }
    int copy = real();
    copiedDescriptor(fd, copy, kept);
    if (kept >= 0) {
        int saved_errno = errno;
        unixclose_t realClose = unixclose ? unixclose : (unixclose_t)dlsym(RTLD_NEXT, "close");
        (*realClose)(kept);
        errno = saved_errno;
    }
    return copy;
}

INTERPOSE(int, dup2, (int fd, int onto)) {
    auto real = NEXT_SYMBOL(int (*)(int, int), "dup2");
    return copyOnto(fd, onto, [&] { return (*real)(fd, onto); });
}

INTERPOSE(int, dup3, (int fd, int onto, int flags)) {
    auto real = NEXT_SYMBOL(int (*)(int, int, int), "dup3");
    return copyOnto(fd, onto, [&] { return (*real)(fd, onto, flags); });
}

// fcntl() copies a descriptor too, and says whether one appends. Its third
// argument is passed on as the C library itself reads it, whatever the command.
static int descriptorControl(unixfcntl_t real, int fd, int command, void *argument) {
    int result = (*real)(fd, command, argument);
    if (command == F_DUPFD || command == F_DUPFD_CLOEXEC) {
        copiedDescriptor(fd, result);
    } else if (command == F_SETFL && result == 0) {
        MonitoredFile monitored(fd);
        if (monitored) {
            monitored.file->setAppends(monitored.pos, ((long)argument & O_APPEND) != 0);
        }
    }
    return result;
}

INTERPOSE(int, fcntl, (int fd, int command, ...)) {
    va_list arguments;
    va_start(arguments, command);
    void *argument = va_arg(arguments, void *);
    va_end(arguments);
    return descriptorControl(NEXT_SYMBOL(unixfcntl_t, "fcntl"), fd, command, argument);
}

INTERPOSE(int, fcntl64, (int fd, int command, ...)) {
    va_list arguments;
    va_start(arguments, command);
    void *argument = va_arg(arguments, void *);
    va_end(arguments);
    unixfcntl_t real = NEXT_SYMBOL(unixfcntl_t, "fcntl64"); // in glibc since 2.28
    return descriptorControl(real ? real : NEXT_SYMBOL(unixfcntl_t, "fcntl"), fd, command, argument);
}

// preadv(), pwritev() and their variants: one call on a monitored descriptor,
// recorded as one read or write, at the offset given or, for the variants that
// take -1 for it, at the descriptor's. A write that appends (the descriptor
// does, or the call says so: `appending`) goes to the end of the file whatever
// the offset given, and leaves the descriptor's where it was.
#ifndef RWF_APPEND
#define RWF_APPEND 0x00000010
#endif

template <typename Real>
static ssize_t vectorAt(int fd, bool isRead, off64_t offset, bool appending, Real real) {
    MonitoredFile monitored(fd);
    if (!monitored) {
        return real();
    }
    const Timer::Metric metric = isRead ? Timer::Metric::readv : Timer::Metric::writev;
    timer->start();
    ssize_t done = real();
    if (done > 0) {
        int saved_errno = errno;
        if (offset < 0) {
            trackVector(monitored.file, monitored.pos, fd, done, isRead);
        } else if (auto *trackFile = dynamic_cast<TrackFile *>(monitored.file)) {
            if (!isRead && (appending || monitored.file->appends(monitored.pos))) {
                const off_t start = TrackFile::appendedAt(fd, done);
                offset = start >= 0 ? start : offset;
            }
            trackFile->trackAt(!isRead, done, offset);
        }
        timer->addAmt(Timer::MetricType::monitor, metric, done);
        errno = saved_errno;
    }
    timer->end(Timer::MetricType::monitor, metric);
    return done;
}

#define VECTOR_AT(name, isRead, offset_t)                                                           \
    INTERPOSE(ssize_t, name, (int fd, const struct iovec *iov, int iovcnt, offset_t offset)) {      \
        auto real = NEXT_SYMBOL(ssize_t (*)(int, const struct iovec *, int, offset_t), #name);      \
        return vectorAt(fd, isRead, offset, false, [&] { return (*real)(fd, iov, iovcnt, offset); });      \
    }
#define VECTOR_AT_FLAGS(name, isRead, offset_t)                                                           \
    INTERPOSE(ssize_t, name, (int fd, const struct iovec *iov, int iovcnt, offset_t offset, int flags)) { \
        auto real = NEXT_SYMBOL(ssize_t (*)(int, const struct iovec *, int, offset_t, int), #name);       \
        if (missing(real)) { /* in glibc since 2.26 */                                                    \
            return -1;                                                                                    \
        }                                                                                                 \
        return vectorAt(fd, isRead, offset, (flags & RWF_APPEND) != 0,                                    \
                        [&] { return (*real)(fd, iov, iovcnt, offset, flags); });     \
    }
VECTOR_AT(preadv, true, off_t)
VECTOR_AT(preadv64, true, off64_t)
VECTOR_AT(pwritev, false, off_t)
VECTOR_AT(pwritev64, false, off64_t)
VECTOR_AT_FLAGS(preadv2, true, off_t)
VECTOR_AT_FLAGS(preadv64v2, true, off64_t)
VECTOR_AT_FLAGS(pwritev2, false, off_t)
VECTOR_AT_FLAGS(pwritev64v2, false, off64_t)

// sendfile() and copy_file_range(): the kernel reads from one descriptor and
// writes to another. Each side that is monitored is recorded, at the offset the
// program gave for it or, with none, at the descriptor's.
template <typename Offset>
static void trackTransferSide(MonitoredFile &side, bool isRead, const Offset *start, ssize_t bytes) {
    if (!side) {
        return;
    }
    timer->addAmt(Timer::MetricType::monitor, isRead ? Timer::Metric::read : Timer::Metric::write, bytes);
    if (!start) {
        trackVector(side.file, side.pos, side.fd, bytes, isRead);
    } else if (auto *trackFile = dynamic_cast<TrackFile *>(side.file)) {
        trackFile->trackAt(!isRead, bytes, *start);
    }
}

template <typename Offset, typename Real>
static ssize_t transfer(int in_fd, Offset *in_offset, int out_fd, Offset *out_offset, Real real) {
    MonitoredFile in(in_fd), out(out_fd);
    if (!in && !out) {
        return real();
    }
    const Offset in_start = in_offset ? *in_offset : 0;
    const Offset out_start = out_offset ? *out_offset : 0;
    timer->start();
    ssize_t done = real();
    if (done > 0) {
        int saved_errno = errno;
        trackTransferSide(in, true, in_offset ? &in_start : NULL, done);
        trackTransferSide(out, false, out_offset ? &out_start : NULL, done);
        errno = saved_errno;
    }
    timer->end(Timer::MetricType::monitor, in ? Timer::Metric::read : Timer::Metric::write);
    return done;
}

INTERPOSE(ssize_t, sendfile, (int out_fd, int in_fd, off_t *offset, size_t count)) {
    auto real = NEXT_SYMBOL(ssize_t (*)(int, int, off_t *, size_t), "sendfile");
    return transfer(in_fd, offset, out_fd, (off_t *)NULL, [&] { return (*real)(out_fd, in_fd, offset, count); });
}

INTERPOSE(ssize_t, sendfile64, (int out_fd, int in_fd, off64_t *offset, size_t count)) {
    auto real = NEXT_SYMBOL(ssize_t (*)(int, int, off64_t *, size_t), "sendfile64");
    return transfer(in_fd, offset, out_fd, (off64_t *)NULL, [&] { return (*real)(out_fd, in_fd, offset, count); });
}

INTERPOSE(ssize_t, copy_file_range,
          (int in_fd, off64_t *in_offset, int out_fd, off64_t *out_offset, size_t length, unsigned int flags)) {
    auto real = NEXT_SYMBOL(ssize_t (*)(int, off64_t *, int, off64_t *, size_t, unsigned int), "copy_file_range");
    if (missing(real)) { // in glibc since 2.27
        return -1;
    }
    return transfer(in_fd, in_offset, out_fd, out_offset,
                    [&] { return (*real)(in_fd, in_offset, out_fd, out_offset, length, flags); });
}

/*Streaming**************************************************************************************************/

FILE *trackFileFopen(std::string name, std::string metaName, MonitorFile::Type type, const char *__restrict fileName, const char *__restrict modes) {
  // DPRINTF("Lib.cpp: trackFileFopen: %s %s %u\n", name.c_str(), metaName.c_str(), type);
  DPRINTF("Lib.cpp: in trackFileFopen\n");
  FILE *fp = (*unixfopen)(name.c_str(), modes);
  if (fp) {
    int fd = fileno(fp);
    MonitorFile *file = MonitorFile::addNewMonitorFile(type, name, metaName, fd);
    if (file) {
      MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex(fd));
      DPRINTF("Lib.cpp: trackFileOpen add new  file success: %s , fd = %d\n", fileName, fd);
    }
  }
  return fp;
}

FILE *monitorFopen(std::string name, std::string metaName, MonitorFile::Type type, const char *__restrict fileName, const char *__restrict modes) {
  DPRINTF("Lib.cpp: monitorFOpen: %s %s %u\n", name.c_str(), metaName.c_str(), type);
    char m = 'r';
    FILE *fp = (*unixfopen)(fileName, &m);
    if (fp) {
        int fd = fileno(fp);
        MonitorFile *file = MonitorFile::addNewMonitorFile(type, name, metaName, fd);
        if (file) {
            MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex(fd));
        }
    }
    return fp;
}

FILE *fopen(const char *__restrict fileName, const char *__restrict modes) {
  DPRINTF("Lib.cpp: Calling fopen on %s \n", fileName);  
  if (leavesRecordsAlone() || isMonitorOwnedPath(fileName, Config::dataLifeOutputPath)) {
    return (*unixfopen)(fileName, modes);
  }
  Timer::Metric metric = (modes[0] == 'r') ? Timer::Metric::in_fopen : Timer::Metric::out_fopen;

  for (auto pattern: patterns) {
    auto ret_val = fnmatch(pattern.c_str(), get_basename(fileName), 0);
    if (ret_val == 0) {

      return outerWrapper("fopen", fileName, metric, trackFileFopen, unixfopen, 
			  fileName, modes);
    }
  }
 
  return outerWrapper("fopen", fileName, metric, monitorFopen, unixfopen, fileName, modes);
}

FILE *fopen64(const char *__restrict fileName, const char *__restrict modes) {
  DPRINTF("Lib.cpp: Calling fopen64 on %s \n", fileName);  
  if (leavesRecordsAlone() || isMonitorOwnedPath(fileName, Config::dataLifeOutputPath)) {
    return (*unixfopen64)(fileName, modes);
  }
  Timer::Metric metric = (modes[0] == 'r') ? Timer::Metric::in_fopen : Timer::Metric::out_fopen;

  for (auto pattern: patterns) {
    auto ret_val = fnmatch(pattern.c_str(), get_basename(fileName), 0);
    if (ret_val == 0 
    // && (strstr(fileName, "_r_stat") || strstr(fileName, "_w_stat") || strstr(fileName, "_trace_stat"))
    ) {
      DPRINTF("Lib.cpp: fopen64() Found pattern [%s] \n", pattern.c_str());
      return outerWrapper("fopen64", fileName, metric, trackFileFopen, unixfopen64, 
			  fileName, modes);
    }
  }  

    return outerWrapper("fopen64", fileName, metric, monitorFopen, unixfopen64, fileName, modes);
}

// A stream on a descriptor the program opened itself, as bowtie2 does for its
// reads: open(), then fdopen(). On a monitored descriptor the stream is monitored
// like one from fopen() (see MonitoredFile), and fclose() closes the file.
FILE *monitorFdopen(MonitorFile *file, unsigned int pos, int fd, const char *modes) {
    FILE *fp = (*unixfdopen)(fd, modes);
    if (fp) {
        // An appending stream starts at the end of the file.
        int saved_errno = errno;
        long start = (*unixftell)(fp);
        errno = saved_errno;
        if (start >= 0) {
            file->setFilePos(pos, start);
        }
    }
    return fp;
}

FILE *fdopen(int fd, const char *modes) {
    Timer::Metric metric = (modes && modes[0] == 'r') ? Timer::Metric::in_fopen : Timer::Metric::out_fopen;
    return outerWrapper("fdopen", fd, metric, monitorFdopen, unixfdopen, fd, modes);
}

// What the program has done in a stream's buffer: the bytes it took since the
// buffer was last filled, or put since it was last flushed. glibc's
// getc_unlocked(), putc_unlocked() and the other unlocked character calls are
// macros that work in the buffer and call libc only to refill or flush it
// (__uflow(), __underflow() and __overflow() below), so no wrapper sees these
// bytes pass. The FILE members read here are the ones those macros use.
struct StreamBuffer {
    bool writing;
    long bytes;

    explicit StreamBuffer(FILE *fp) : writing(__fwriting(fp) != 0) {
        if (writing) {
            bytes = fp->_IO_write_ptr - fp->_IO_write_base;
        } else if (fp->_IO_read_base >= fp->_IO_buf_base && fp->_IO_read_base <= fp->_IO_buf_end) {
            bytes = fp->_IO_read_ptr - fp->_IO_buf_base;
        } else {
            // The stream reads from the area where ungetc() keeps what it could
            // not put back in the buffer. Its place in the buffer is saved, and
            // what is left in that area is still to be read again.
            bytes = (fp->_IO_save_base - fp->_IO_buf_base) - (fp->_IO_read_end - fp->_IO_read_ptr);
        }
    }
    // Bytes the stream has just written.
    explicit StreamBuffer(long written) : writing(true), bytes(written) {}
};

// Records the bytes a monitored stream moved without a wrapper seeing them.
// `buffer` is the stream's buffer and `now` its position at this moment; the
// bytes are those of the buffer that are not recorded yet (see
// streamRecorded()), and they end at `now`. Counting them in the buffer, not by
// how far the position moved, keeps a seek, or a file that another writer
// appends to, from being taken for I/O. This also covers what stdio calls
// without a wrapper (the wide-character ones, getw(), ...) left in the buffer,
// but not what they moved through earlier buffers.
static void trackUnseenStreamIO(MonitorFile *file, unsigned int pos, const StreamBuffer &buffer, long now) {
    long unseen = buffer.bytes - std::max(file->streamRecorded(pos), 0L);
    if (unseen < 0) {
        // Fewer bytes than were recorded. The program went back in the buffer
        // (ungetc()) if the position went back as far: nothing new then.
        // Otherwise the buffer was filled or flushed since, and all are new.
        unseen = (now - (long)file->filePos(pos) == unseen) ? 0 : buffer.bytes;
    }
    unseen = std::min(unseen, now);
    if (unseen > 0) {
        timer->addAmt(Timer::MetricType::monitor, buffer.writing ? Timer::Metric::write : Timer::Metric::read, unseen);
        if (auto *trackFile = dynamic_cast<TrackFile *>(file)) {
            if (buffer.writing) {
                trackFile->trackWrite(unseen, pos, now);
            } else {
                trackFile->trackRead(unseen, pos, now);
            }
        }
    }
    file->setFilePos(pos, now);
}

// Where a stream that is writing is: ftell(), which counts what waits in its
// buffer from the descriptor's offset. Through a descriptor that appends, that
// goes to the end of the file instead, and only a stream opened to append
// knows: one from fdopen(fd, "w") does not.
static long writePosition(MonitorFile *file, unsigned int pos, FILE *fp) {
    if (file->appends(pos)) {
        const off_t end = TrackFile::appendedAt(fp->_fileno, 0);
        if (end >= 0) {
            return (long)end + (fp->_IO_write_ptr - fp->_IO_write_base);
        }
    }
    return (*unixftell)(fp);
}

// After a call on fp that read or wrote was recorded: so is all the program has
// done in the stream's buffer, but for the last `pending` bytes.
static void streamRecorded(MonitorFile *file, unsigned int pos, FILE *fp, long pending = 0) {
    file->setStreamRecorded(pos, std::max(StreamBuffer(fp).bytes - pending, 0L));
}

// Looks at the stream's buffer and does that, before every call on the stream
// that a wrapper makes: the call may empty the buffer or fill it anew. Only on
// a stream already seen reading, writing or seeking: one used with nothing but
// calls that have no wrapper is left alone, rather than reported with the last
// part of its I/O. A call that writes (`writes`) sees the stream from here on,
// and what already waits in its buffer goes to the file with what the call
// adds: characters put with the unlocked macros, or what the program wrote
// before dup2() gave the stream this file. Costs a system call (ftell() on a
// stream that was never positioned) only if the buffer holds bytes that are not
// recorded. A stream that cannot seek, such as one on a FIFO, has no position to
// record them at, so they are not.
static void trackUnseenStreamIO(MonitorFile *file, unsigned int pos, FILE *fp, bool writes = false) {
    long recorded = file->streamRecorded(pos);
    StreamBuffer buffer(fp);
    if (recorded < 0) {
        if (!writes || !buffer.writing) {
            return;
        }
        recorded = 0;
    }
    if (buffer.bytes <= 0 || buffer.bytes == recorded) {
        return;
    }
    int saved_errno = errno;
    long now = buffer.writing ? writePosition(file, pos, fp) : (*unixftell)(fp);
    errno = saved_errno;
    if (now >= 0) {
        trackUnseenStreamIO(file, pos, buffer, now);
        file->setStreamRecorded(pos, buffer.bytes);
    }
}

// Before the file of a monitored stream is closed, by fclose() or freopen():
// records what the program left in the stream's buffer and pushes the buffer to
// the descriptor, so that the size TrackFile::close() reads is the final one.
// Returns whether the stream is monitored. Sets flush_errno if the buffer could
// not be written: the C library has dropped it then, and its own close will not
// report the failure. Gives the name of the stream's file if asked.
static bool finishStream(FILE *fp, int &flush_errno, std::string *name = NULL) {
    if (!mayBeMonitored(fp)) {
        return false;
    }
    StreamLock lock(fp);
    MonitoredFile monitored(fp);
    if (!monitored) {
        return false;
    }
    trackUnseenStreamIO(monitored.file, monitored.pos, fp);
    if ((*unixfflush)(fp) != 0) {
        flush_errno = errno;
    }
    if (name) {
        *name = monitored.file->name();
    }
    return true;
}

int fclose(FILE *fp) {
    DPRINTF("Lib.cpp: Invoking fclose \n");
    if (!init || leavesRecordsAlone()) {
        unixfclose_t realFclose = unixfclose ? unixfclose : (unixfclose_t)dlsym(RTLD_NEXT, "fclose");
        return (*realFclose)(fp);
    }
    const int fd = fp->_fileno;
    {
        // As close() does for the descriptor of a stream from fdopen().
        std::lock_guard<CountedMutex> guard(fdToFileMapMutex);
        fdToFileMap.erase(fd);
    }
    timer->start();
    int flush_errno = 0;
    const bool monitored = finishStream(fp, flush_errno);
    if (monitored) {
        MonitorFileDescriptor::removeMonitorFileDescriptor(fd, fd); // once, however many patterns the name matches
    }
    int ret = (*unixfclose)(fp);
    timer->end(monitored ? Timer::MetricType::monitor : Timer::MetricType::system, Timer::Metric::close);
    if (ret == 0 && flush_errno != 0) {
        errno = flush_errno;
        ret = EOF;
    }
    return ret;
}

// fflush() of a monitored stream empties its buffer: record what the program put
// there unseen first. fflush(NULL) is passed through, so bytes unseen in a
// monitored stream at that moment are not recorded.
// unlocked: the call is one the program makes without the stream's lock (see
// StreamLock).
static int flushStream(unixfflush_t real, FILE *fp, bool unlocked = false) {
    if (!fp || !mayBeMonitored(fp)) {
        return (*real)(fp);
    }
    StreamLock lock(fp, !unlocked);
    if (!lock) {
        return (*real)(fp);
    }
    MonitoredFile monitored(fp);
    if (!monitored) {
        return (*real)(fp);
    }
    trackUnseenStreamIO(monitored.file, monitored.pos, fp);
    int ret = (*real)(fp);
    if (monitored.file->streamRecorded(monitored.pos) >= 0) {
        streamRecorded(monitored.file, monitored.pos, fp);
    }
    return ret;
}

int fflush(FILE *fp) {
    return flushStream(unixfflush ? unixfflush : (unixfflush_t)dlsym(RTLD_NEXT, "fflush"), fp);
}

// A program seldom closes its standard streams. What it took from the buffer of
// one that is on a monitored file, or left in it to be written at the exit,
// since the last call recorded (getchar() and putchar() are recorded a buffer
// at a time) is recorded here, when the library is unloaded, before the timer's
// totals are written. A stream another thread holds at that moment is left
// alone.
static void recordStandardStreams(void) {
    for (FILE *fp : {stdin, stdout, stderr}) {
        if (!mayBeMonitored(fp)) {
            continue;
        }
        StreamLock lock(fp, false);
        if (!lock) {
            continue;
        }
        MonitoredFile monitored(fp);
        if (monitored) {
            trackUnseenStreamIO(monitored.file, monitored.pos, fp, fp != stdin);
        }
    }
}

// After a real fread()-like call read items_read items of size bytes from fp.
static size_t trackFread(size_t items_read, size_t size, MonitorFile *file, unsigned int pos, FILE *fp) {
    auto read_bytes = items_read * size;

    // Update the timer with the number of bytes read
    timer->addAmt(Timer::MetricType::monitor, Timer::Metric::read, read_bytes);

    // Track for JSON tracing (if file is TrackFile)
    if (read_bytes > 0) {
        long new_pos = (*unixftell)(fp);
        if (new_pos >= 0) {
            streamRecorded(file, pos, fp);
            file->setFilePos(pos, new_pos);
            // Call tracking method for trace collection
            if (auto* trackFile = dynamic_cast<TrackFile*>(file)) {
                trackFile->trackRead(read_bytes, pos, new_pos);
            }
        }
    }

    return items_read;
}

size_t monitorFread(MonitorFile *file, unsigned int pos, int fd, void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
  DPRINTF("Lib.cpp: In monitor fread \n");
    trackUnseenStreamIO(file, pos, fp);
    return trackFread((*unixfread)(ptr, size, n, fp), size, file, pos, fp);
}

size_t monitorFreadUnlocked(MonitorFile *file, unsigned int pos, int fd, void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
    trackUnseenStreamIO(file, pos, fp);
    return trackFread((*unixfread_unlocked)(ptr, size, n, fp), size, file, pos, fp);
}

size_t fread(void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
  DPRINTF("Lib.cpp: Invoking fread \n");
  auto ret_val = outerWrapper("fread", fp, Timer::Metric::read, 
			      monitorFread, unixfread, ptr, size, n, fp);
  DPRINTF("Lib.cpp: fread return value %d\n", ret_val);
  return ret_val;
}

size_t fread_unlocked(void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
  return outerWrapper("fread_unlocked", UnlockedStream{fp}, Timer::Metric::read, monitorFreadUnlocked, unixfread_unlocked, ptr, size, n, fp);
}

// After a real call wrote written_bytes through fp.
static void trackFwrite(size_t written_bytes, MonitorFile *file, unsigned int pos, FILE *fp) {
    // Update the timer with the number of bytes written
    timer->addAmt(Timer::MetricType::monitor, Timer::Metric::write, written_bytes);

    // Track for JSON tracing (if file is TrackFile)
    if (written_bytes > 0) {
        int saved_errno = errno;
        long new_pos = writePosition(file, pos, fp);
        errno = saved_errno;
        if (new_pos >= 0) {
            streamRecorded(file, pos, fp);
            file->setFilePos(pos, new_pos);
            // Call tracking method for trace collection
            if (auto* trackFile = dynamic_cast<TrackFile*>(file)) {
                trackFile->trackWrite(written_bytes, pos, new_pos);
            }
        }
    }
}

size_t monitorFwrite(MonitorFile *file, unsigned int pos, int fd, const void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
    DPRINTF("Lib.cpp: In monitor fwrite \n");
    trackUnseenStreamIO(file, pos, fp, true);
    auto items_written = (*unixfwrite)(ptr, size, n, fp);
    trackFwrite(items_written * size, file, pos, fp);
    return items_written;
}

size_t fwrite(const void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
    DPRINTF("Lib.cpp: Invoking fwrite \n");
    return outerWrapper("fwrite", fp, Timer::Metric::write, monitorFwrite, unixfwrite, ptr, size, n, fp);
}

int monitorVfprintf(MonitorFile *file, unsigned int pos, int fd, FILE * stream,
		     const char * format, va_list arg) {
  trackUnseenStreamIO(file, pos, stream, true);
  auto count = unix_vfprintf(stream, format, arg);
  if (count > 0) {
    trackFwrite(count, file, pos, stream);
  }
  return count;
}
int vfprintf(FILE * stream, const char * format, va_list arg ) {
  //DPRINTF("Lib.cpp: Invoking vfprintf\n");
  return outerWrapper("vfprintf", stream, Timer::Metric::write, monitorVfprintf, 
		      unix_vfprintf, stream, format, arg);
}

long int monitorFtell(MonitorFile *file, unsigned int pos, int fd, FILE *fp) {
    // return (long int)lseek(fd, 0, SEEK_CUR);
    return (*unixftell)(fp);
}

long int ftell(FILE *fp) {
    return outerWrapper("ftell", fp, Timer::Metric::ftell, monitorFtell, unixftell, fp);
}

// fseek(), fseeko() and fseeko64() on a monitored stream. bowtie2 positions its
// index files with fseeko64(): the position recorded for the stream must follow,
// or the bytes skipped would later pass for bytes read.
// `reposition` makes the real call.
template <typename Reposition>
static int repositionStream(MonitorFile *file, unsigned int pos, FILE *fp, Reposition reposition) {
    trackUnseenStreamIO(file, pos, fp);
    int result = reposition();
    // The stream as the seek left it: what lies before the new position in the
    // buffer was not read, and what the program takes after it is. From here on
    // the stream counts as seen, though only its position was.
    streamRecorded(file, pos, fp);
    if (result == 0) {
        long new_pos = (*unixftell)(fp);
        if (new_pos >= 0) {
            file->setFilePos(pos, new_pos);
        }
    }
    return result;
}

template <typename Seek, typename Offset>
static int seekStream(Seek realSeek, MonitorFile *file, unsigned int pos, FILE *fp, Offset off, int whence) {
    return repositionStream(file, pos, fp, [&] { return (*realSeek)(fp, off, whence); });
}

int monitorFseek(MonitorFile *file, unsigned int pos, int fd, FILE *fp, long int off, int whence) {
    return seekStream(unixfseek, file, pos, fp, off, whence);
}

int fseek(FILE *fp, long int off, int whence) {
    return outerWrapper("fseek", fp, Timer::Metric::seek, monitorFseek, unixfseek, fp, off, whence);
}

int monitorFseeko(MonitorFile *file, unsigned int pos, int fd, FILE *fp, off_t off, int whence) {
    return seekStream(unixfseeko, file, pos, fp, off, whence);
}

int fseeko(FILE *fp, off_t off, int whence) {
    return outerWrapper("fseeko", fp, Timer::Metric::seek, monitorFseeko, unixfseeko, fp, off, whence);
}

int monitorFseeko64(MonitorFile *file, unsigned int pos, int fd, FILE *fp, off64_t off, int whence) {
    return seekStream(unixfseeko64, file, pos, fp, off, whence);
}

int fseeko64(FILE *fp, off64_t off, int whence) {
    return outerWrapper("fseeko64", fp, Timer::Metric::seek, monitorFseeko64, unixfseeko64, fp, off, whence);
}

int monitorFgetc(MonitorFile *file, unsigned int pos, int fd, FILE *fp) {
    // if (!file->eof(pos)) {
    //     unsigned char buffer;
    //     read(fd, &buffer, 1);
    //     return (int)buffer;
    // }
    // return EOF;

    trackUnseenStreamIO(file, pos, fp);
    int result = (*unixfgetc)(fp);

    // Sync internal position tracking with actual FILE* position
    if (result != EOF) {
        long new_pos = (*unixftell)(fp);
        if (new_pos >= 0) {
            streamRecorded(file, pos, fp);
            file->setFilePos(pos, new_pos);
            timer->addAmt(Timer::MetricType::monitor, Timer::Metric::read, 1);
            // Call tracking method for trace collection
            if (auto* trackFile = dynamic_cast<TrackFile*>(file)) {
                trackFile->trackRead(1, pos, new_pos);
            }
        }
    }

    return result;
}

int fgetc(FILE *fp) {
    return outerWrapper("fgetc", fp, Timer::Metric::fgetc, monitorFgetc, unixfgetc, fp);
}

// After a real call read through fp a number of bytes it does not tell: fgets(),
// whose line is longer than strlen() says if it holds a NUL, and the scanf()
// family. What it read is what lies between the position last recorded and the
// stream's position now.
static void trackReadTo(MonitorFile *file, unsigned int pos, FILE *fp) {
    int saved_errno = errno;
    long new_pos = (*unixftell)(fp);
    errno = saved_errno;
    if (new_pos < 0) {
        return;
    }
    long prev_pos = file->filePos(pos);
    file->setFilePos(pos, new_pos);
    streamRecorded(file, pos, fp);
    if (new_pos > prev_pos) {
        timer->addAmt(Timer::MetricType::monitor, Timer::Metric::read, new_pos - prev_pos);
        // Call tracking method for trace collection
        if (auto* trackFile = dynamic_cast<TrackFile*>(file)) {
            trackFile->trackRead(new_pos - prev_pos, pos, new_pos);
        }
    }
}

// After a real fgets()-like call on fp returned result.
static char *trackFgets(char *result, MonitorFile *file, unsigned int pos, FILE *fp) {
    if (result != NULL) {
        trackReadTo(file, pos, fp);
    }
    return result;
}

char *monitorFgets(MonitorFile *file, unsigned int pos, int fd, char *__restrict s, int n, FILE *__restrict fp) {
    trackUnseenStreamIO(file, pos, fp);
    return trackFgets((*unixfgets)(s, n, fp), file, pos, fp);
}

char *monitorFgetsUnlocked(MonitorFile *file, unsigned int pos, int fd, char *__restrict s, int n, FILE *__restrict fp) {
    trackUnseenStreamIO(file, pos, fp);
    return trackFgets((*unixfgets_unlocked)(s, n, fp), file, pos, fp);
}

char *monitorFgetsChk(MonitorFile *file, unsigned int pos, int fd, char *__restrict s, size_t size, int n, FILE *__restrict fp) {
    trackUnseenStreamIO(file, pos, fp);
    return trackFgets((*unix__fgets_chk)(s, size, n, fp), file, pos, fp);
}

char *monitorFgetsUnlockedChk(MonitorFile *file, unsigned int pos, int fd, char *__restrict s, size_t size, int n, FILE *__restrict fp) {
    trackUnseenStreamIO(file, pos, fp);
    return trackFgets((*unix__fgets_unlocked_chk)(s, size, n, fp), file, pos, fp);
}

char *fgets(char *__restrict s, int n, FILE *__restrict fp) {
    return outerWrapper("fgets", fp, Timer::Metric::fgets, monitorFgets, unixfgets, s, n, fp);
}

char *fgets_unlocked(char *__restrict s, int n, FILE *__restrict fp) {
    return outerWrapper("fgets_unlocked", UnlockedStream{fp}, Timer::Metric::fgets, monitorFgetsUnlocked, unixfgets_unlocked, s, n, fp);
}

int monitorFputc(MonitorFile *file, unsigned int pos, int fd, int c, FILE *fp) {
    // write(fd, (void *)&c, 1);
    // return c;

    trackUnseenStreamIO(file, pos, fp, true);
    int result = (*unixfputc)(c, fp);

    if (result != EOF) {
        long new_pos = writePosition(file, pos, fp);
        if (new_pos >= 0) {
            streamRecorded(file, pos, fp);
            file->setFilePos(pos, new_pos);
            timer->addAmt(Timer::MetricType::monitor, Timer::Metric::write, 1);
            // Call tracking method for trace collection
            if (auto* trackFile = dynamic_cast<TrackFile*>(file)) {
                trackFile->trackWrite(1, pos, new_pos);
            }
        }
    }

    return result;
}

int fputc(int c, FILE *fp) {
    return outerWrapper("fputc", fp, Timer::Metric::fputc, monitorFputc, unixfputc, c, fp);
}

int monitorFputs(MonitorFile *file, unsigned int pos, int fd, const char *__restrict s, FILE *__restrict fp) {
    // unsigned int index = 0;
    // while (1) {
    //     if (s[index] == '\0')
    //         break;
    //     index++;
    // }

    // int res = -1;
    // if (index)
    //     res = write(fd, s, index);

    // if (res == -1)
    //     return EOF;
    // return res;

    trackUnseenStreamIO(file, pos, fp, true);
    int result = (*unixfputs)(s, fp);
    if (result != EOF) {
        trackFwrite(strlen(s), file, pos, fp);
    }
    return result;
}

int fputs(const char *__restrict s, FILE *__restrict fp) {
    return outerWrapper("fputs", fp, Timer::Metric::fputs, monitorFputs, unixfputs, s, fp);
}

int monitorFeof(MonitorFile *file, unsigned int pos, int fd, FILE *fp) {
    // return file->eof(pos);

    return (*unixfeof)(fp);
}

int feof(FILE *fp) ADD_THROW {
    return outerWrapper("feof", fp, Timer::Metric::feof, monitorFeof, unixfeof, fp);
}

/* Inlined character I/O **********************************************************************************/

// glibc's getc_unlocked(), fgetc_unlocked(), getchar_unlocked(), putc_unlocked(),
// fputc_unlocked() and putchar_unlocked() are macros: they take or put the
// character in the stream's buffer and call __uflow() (or, to peek,
// __underflow()) only when it is empty and __overflow() only when it is full.
// These are therefore the only calls a program reading or writing a character at
// a time makes, as bowtie2 does for its FASTQ input. On a monitored stream each
// records what the program did in the buffer it replaces (see StreamBuffer), as
// one read or write; fclose() records the rest. libc's own calls to these three
// functions do not come here.
extern "C" int __underflow(FILE *fp);

// A call that finds a monitored stream's buffer empty and fills it: `take` makes
// it. taken: the call takes the character it returns, as __uflow() does.
template <typename Take>
static int refillStream(MonitorFile *file, unsigned int pos, FILE *fp, bool taken, Take take) {
    timer->start();
    if (file->streamRecorded(pos) < 0) {
        file->setStreamRecorded(pos, 0); // seen reading from here on
    }
    trackUnseenStreamIO(file, pos, fp);
    int result = take();
    // A character taken from the new buffer is recorded with the rest of it.
    streamRecorded(file, pos, fp, (taken && result != EOF) ? 1 : 0);
    timer->end(Timer::MetricType::monitor, Timer::Metric::read);
    return result;
}

// A call that finds a monitored stream's buffer full, puts ch in it and writes
// the buffer: `put` makes it.
template <typename Put>
static int overflowStream(MonitorFile *file, unsigned int pos, FILE *fp, int ch, Put put) {
    timer->start();
    if (file->streamRecorded(pos) < 0) {
        file->setStreamRecorded(pos, 0); // seen writing from here on
    }
    trackUnseenStreamIO(file, pos, fp);
    int result = put();
    // The character now waits in the buffer, to be recorded with the rest of it,
    // unless the stream wrote it at once: an unbuffered stream does, and a
    // line-buffered one given a newline.
    const bool stored = ch != EOF && result != EOF;
    const bool written = stored && fp->_IO_write_ptr == fp->_IO_write_base;
    if (written) {
        int saved_errno = errno;
        long now = writePosition(file, pos, fp);
        errno = saved_errno;
        if (now > 0) {
            file->setStreamRecorded(pos, 0);
            trackUnseenStreamIO(file, pos, StreamBuffer(1), now);
        }
    }
    streamRecorded(file, pos, fp, (stored && !written) ? 1 : 0);
    timer->end(Timer::MetricType::monitor, Timer::Metric::write);
    return result;
}

// Other streams go straight to libc: these are called for every character of an
// unbuffered stream. The stream is not locked here: the program has locked it
// (flockfile()) if other threads use it.
int __uflow(FILE *fp) {
    unixuflow_t real = unix__uflow ? unix__uflow : (unixuflow_t)dlsym(RTLD_NEXT, "__uflow");
    MonitoredFile monitored(fp);
    if (!monitored) {
        return (*real)(fp);
    }
    return refillStream(monitored.file, monitored.pos, fp, true, [&] { return (*real)(fp); });
}

int __underflow(FILE *fp) {
    unixuflow_t real = unix__underflow ? unix__underflow : (unixuflow_t)dlsym(RTLD_NEXT, "__underflow");
    MonitoredFile monitored(fp);
    if (!monitored) {
        return (*real)(fp);
    }
    return refillStream(monitored.file, monitored.pos, fp, false, [&] { return (*real)(fp); });
}

int __overflow(FILE *fp, int ch) {
    unixoverflow_t real = unix__overflow ? unix__overflow : (unixoverflow_t)dlsym(RTLD_NEXT, "__overflow");
    MonitoredFile monitored(fp);
    if (!monitored) {
        return (*real)(fp, ch);
    }
    return overflowStream(monitored.file, monitored.pos, fp, ch, [&] { return (*real)(fp, ch); });
}

/* More calls on streams **********************************************************************************/

INTERPOSE(int, fflush_unlocked, (FILE *fp)) {
    return flushStream(NEXT_SYMBOL(unixfflush_t, "fflush_unlocked"), fp, true);
}

// getc(), putc() and their variants as real calls: what the macros above do, in
// libc. A character taken from the buffer, or put where there is room, is
// recorded with the rest of the buffer; the buffer is recorded when the call
// finds it empty, or full, as __uflow() and __overflow() do it. The stream is
// locked from the look at the buffer to the end of the call, so that no other
// thread empties or fills the buffer in between, unrecorded.
typedef int (*unixgetc_t)(FILE *fp);
typedef int (*unixputc_t)(int ch, FILE *fp);

// unlocked: the call is one the program makes without the stream's lock (see
// StreamLock).
static int takeCharacter(unixgetc_t real, FILE *fp, bool unlocked = false) {
    if (!mayBeMonitored(fp)) {
        return (*real)(fp);
    }
    StreamLock lock(fp, !unlocked);
    if (!lock || fp->_IO_read_ptr < fp->_IO_read_end) {
        return (*real)(fp);
    }
    MonitoredFile monitored(fp);
    if (!monitored) {
        return (*real)(fp);
    }
    return refillStream(monitored.file, monitored.pos, fp, true, [&] { return (*real)(fp); });
}

static int putCharacter(unixputc_t real, int ch, FILE *fp, bool unlocked = false) {
    if (!mayBeMonitored(fp)) {
        return (*real)(ch, fp);
    }
    StreamLock lock(fp, !unlocked);
    if (!lock || fp->_IO_write_ptr < fp->_IO_write_end) {
        return (*real)(ch, fp);
    }
    MonitoredFile monitored(fp);
    if (!monitored) {
        return (*real)(ch, fp);
    }
    return overflowStream(monitored.file, monitored.pos, fp, ch, [&] { return (*real)(ch, fp); });
}

#define TAKE_CHARACTER(name, unlocked)                                          \
    INTERPOSE(int, name, (FILE *fp)) {                                          \
        return takeCharacter(NEXT_SYMBOL(unixgetc_t, #name), fp, unlocked);     \
    }
#define PUT_CHARACTER(name, unlocked)                                           \
    INTERPOSE(int, name, (int ch, FILE *fp)) {                                  \
        return putCharacter(NEXT_SYMBOL(unixputc_t, #name), ch, fp, unlocked);  \
    }
TAKE_CHARACTER(getc, false)
TAKE_CHARACTER(getc_unlocked, true)
TAKE_CHARACTER(fgetc_unlocked, true)
TAKE_CHARACTER(_IO_getc, false) // getc() in programs built with glibc before 2.28
PUT_CHARACTER(putc, false)
PUT_CHARACTER(putc_unlocked, true)
PUT_CHARACTER(fputc_unlocked, true)
PUT_CHARACTER(_IO_putc, false)

// The calls that name no stream are those above on stdin and stdout, which are
// monitored when their descriptors are: after freopen(), or dup2() onto 0 or 1.
INTERPOSE(int, getchar, (void)) {
    return takeCharacter(NEXT_SYMBOL(unixgetc_t, "getc"), stdin);
}

INTERPOSE(int, getchar_unlocked, (void)) {
    return takeCharacter(NEXT_SYMBOL(unixgetc_t, "getc_unlocked"), stdin, true);
}

INTERPOSE(int, putchar, (int ch)) {
    return putCharacter(NEXT_SYMBOL(unixputc_t, "putc"), ch, stdout);
}

INTERPOSE(int, putchar_unlocked, (int ch)) {
    return putCharacter(NEXT_SYMBOL(unixputc_t, "putc_unlocked"), ch, stdout, true);
}

// A call on a stream: `real` makes it. On a monitored stream `monitored` makes
// and records it, timed, with the stream locked (see StreamLock). unlocked: the
// call is one the program makes without that lock.
template <typename Real, typename Monitored>
static auto streamCall(FILE *fp, Timer::Metric metric, Real real, Monitored monitored, bool unlocked = false)
    -> decltype(real()) {
    if (!mayBeMonitored(fp)) {
        return real();
    }
    StreamLock lock(fp, !unlocked);
    if (!lock) {
        return real();
    }
    MonitoredFile stream(fp);
    if (!stream) {
        return real();
    }
    timer->start();
    auto result = monitored(stream.file, stream.pos);
    timer->end(Timer::MetricType::monitor, metric);
    return result;
}

// fsetpos() and fsetpos64() are seeks to a position fgetpos() gave.
INTERPOSE(int, fsetpos, (FILE *fp, const fpos_t *position)) {
    auto real = NEXT_SYMBOL(int (*)(FILE *, const fpos_t *), "fsetpos");
    return streamCall(
        fp, Timer::Metric::seek, [&] { return (*real)(fp, position); },
        [&](MonitorFile *file, unsigned int pos) {
            return repositionStream(file, pos, fp, [&] { return (*real)(fp, position); });
        });
}

INTERPOSE(int, fsetpos64, (FILE *fp, const fpos64_t *position)) {
    auto real = NEXT_SYMBOL(int (*)(FILE *, const fpos64_t *), "fsetpos64");
    return streamCall(
        fp, Timer::Metric::seek, [&] { return (*real)(fp, position); },
        [&](MonitorFile *file, unsigned int pos) {
            return repositionStream(file, pos, fp, [&] { return (*real)(fp, position); });
        });
}

// getline() and getdelim() return the bytes they read.
typedef ssize_t (*unixgetdelim_t)(char **line, size_t *size, int delimiter, FILE *fp);

static ssize_t readDelimited(unixgetdelim_t real, char **line, size_t *size, int delimiter, FILE *fp) {
    return streamCall(
        fp, Timer::Metric::fgets, [&] { return (*real)(line, size, delimiter, fp); },
        [&](MonitorFile *file, unsigned int pos) {
            trackUnseenStreamIO(file, pos, fp);
            ssize_t got = (*real)(line, size, delimiter, fp);
            if (got > 0) {
                trackFread(got, 1, file, pos, fp);
            }
            return got;
        });
}

INTERPOSE(ssize_t, getline, (char **line, size_t *size, FILE *fp)) {
    return readDelimited(NEXT_SYMBOL(unixgetdelim_t, "getdelim"), line, size, '\n', fp);
}

INTERPOSE(ssize_t, getdelim, (char **line, size_t *size, int delimiter, FILE *fp)) {
    return readDelimited(NEXT_SYMBOL(unixgetdelim_t, "getdelim"), line, size, delimiter, fp);
}

// What getline() is in a program compiled with optimization.
INTERPOSE(ssize_t, __getdelim, (char **line, size_t *size, int delimiter, FILE *fp)) {
    return readDelimited(NEXT_SYMBOL(unixgetdelim_t, "__getdelim"), line, size, delimiter, fp);
}

// The scanf() family does not tell how much it read (see trackReadTo()). A
// program calls fscanf() by one of three names, depending on the C library it
// was built with and for which standard; each reads numbers its own way, so each
// goes to its own real function.
typedef int (*unixvfscanf_t)(FILE *fp, const char *format, va_list arguments);

static int scanStream(unixvfscanf_t real, FILE *fp, const char *format, va_list arguments) {
    return streamCall(
        fp, Timer::Metric::fgets, [&] { return (*real)(fp, format, arguments); },
        [&](MonitorFile *file, unsigned int pos) {
            trackUnseenStreamIO(file, pos, fp);
            int items = (*real)(fp, format, arguments);
            trackReadTo(file, pos, fp);
            return items;
        });
}

#define SCAN_FAMILY(prefix)                                                                    \
    INTERPOSE(int, prefix##vfscanf, (FILE *fp, const char *format, va_list arguments)) {       \
        unixvfscanf_t real = NEXT_SYMBOL(unixvfscanf_t, #prefix "vfscanf");                    \
        if (!real) { /* no __isoc23_ names before glibc 2.38 */                                \
            real = NEXT_SYMBOL(unixvfscanf_t, "__isoc99_vfscanf");                             \
        }                                                                                      \
        return scanStream(real, fp, format, arguments);                                        \
    }                                                                                          \
    INTERPOSE(int, prefix##fscanf, (FILE *fp, const char *format, ...)) {                      \
        va_list arguments;                                                                     \
        va_start(arguments, format);                                                           \
        int items = monitor_##prefix##vfscanf(fp, format, arguments);                          \
        va_end(arguments);                                                                     \
        return items;                                                                          \
    }                                                                                          \
    INTERPOSE(int, prefix##vscanf, (const char *format, va_list arguments)) {                  \
        return monitor_##prefix##vfscanf(stdin, format, arguments);                            \
    }                                                                                          \
    INTERPOSE(int, prefix##scanf, (const char *format, ...)) {                                 \
        va_list arguments;                                                                     \
        va_start(arguments, format);                                                           \
        int items = monitor_##prefix##vfscanf(stdin, format, arguments);                       \
        va_end(arguments);                                                                     \
        return items;                                                                          \
    }
SCAN_FAMILY()
SCAN_FAMILY(__isoc99_)
SCAN_FAMILY(__isoc23_)

// The calls that write through a stream: `write` makes the call, and `bytes`
// tells from what it returned how many bytes it wrote.
template <typename Write, typename Bytes>
static auto writeStream(FILE *fp, Write write, Bytes bytes, bool unlocked = false) -> decltype(write()) {
    return streamCall(
        fp, Timer::Metric::write, write,
        [&](MonitorFile *file, unsigned int pos) {
            trackUnseenStreamIO(file, pos, fp, true);
            auto result = write();
            long written = bytes(result);
            if (written > 0) {
                trackFwrite(written, file, pos, fp);
            }
            return result;
        },
        unlocked);
}

// printf() and its variants return the bytes they wrote. The names with _chk
// are what the others are in a program built with _FORTIFY_SOURCE; they check
// the format as they go.
typedef int (*unixvfprintf_chk_t)(FILE *fp, int flag, const char *format, va_list arguments);

static int printStream(FILE *fp, const char *format, va_list arguments) {
    unix_vfprintf_t real = NEXT_SYMBOL(unix_vfprintf_t, "vfprintf");
    return writeStream(
        fp, [&] { return (*real)(fp, format, arguments); }, [](int printed) { return (long)printed; });
}

static int printStreamChecked(FILE *fp, int flag, const char *format, va_list arguments) {
    unixvfprintf_chk_t real = NEXT_SYMBOL(unixvfprintf_chk_t, "__vfprintf_chk");
    return writeStream(
        fp, [&] { return (*real)(fp, flag, format, arguments); }, [](int printed) { return (long)printed; });
}

INTERPOSE(int, fprintf, (FILE *fp, const char *format, ...)) {
    va_list arguments;
    va_start(arguments, format);
    int printed = printStream(fp, format, arguments);
    va_end(arguments);
    return printed;
}

INTERPOSE(int, printf, (const char *format, ...)) {
    va_list arguments;
    va_start(arguments, format);
    int printed = printStream(stdout, format, arguments);
    va_end(arguments);
    return printed;
}

INTERPOSE(int, vprintf, (const char *format, va_list arguments)) {
    return printStream(stdout, format, arguments);
}

INTERPOSE(int, __vfprintf_chk, (FILE *fp, int flag, const char *format, va_list arguments)) {
    return printStreamChecked(fp, flag, format, arguments);
}

INTERPOSE(int, __fprintf_chk, (FILE *fp, int flag, const char *format, ...)) {
    va_list arguments;
    va_start(arguments, format);
    int printed = printStreamChecked(fp, flag, format, arguments);
    va_end(arguments);
    return printed;
}

INTERPOSE(int, __printf_chk, (int flag, const char *format, ...)) {
    va_list arguments;
    va_start(arguments, format);
    int printed = printStreamChecked(stdout, flag, format, arguments);
    va_end(arguments);
    return printed;
}

INTERPOSE(int, __vprintf_chk, (int flag, const char *format, va_list arguments)) {
    return printStreamChecked(stdout, flag, format, arguments);
}

// puts() and fputs_unlocked() write the whole string, puts() a newline too, or
// fail.
INTERPOSE(int, puts, (const char *s)) {
    auto real = NEXT_SYMBOL(int (*)(const char *), "puts");
    return writeStream(
        stdout, [&] { return (*real)(s); }, [&](int result) { return result == EOF ? 0L : (long)strlen(s) + 1; });
}

INTERPOSE(int, fputs_unlocked, (const char *s, FILE *fp)) {
    auto real = NEXT_SYMBOL(int (*)(const char *, FILE *), "fputs_unlocked");
    return writeStream(
        fp, [&] { return (*real)(s, fp); }, [&](int result) { return result == EOF ? 0L : (long)strlen(s); }, true);
}

INTERPOSE(size_t, fwrite_unlocked, (const void *ptr, size_t size, size_t n, FILE *fp)) {
    unixfwrite_t real = NEXT_SYMBOL(unixfwrite_t, "fwrite_unlocked");
    return writeStream(
        fp, [&] { return (*real)(ptr, size, n, fp); }, [&](size_t items) { return (long)(items * size); }, true);
}

// freopen() closes the file of a stream and opens another on it, or the same in
// another mode when it is given no name. What fclose() does for a monitored
// stream is done for the file it leaves, and the stream is monitored again if
// the file it gets would be with fopen(). The stream stays locked from the one
// to the other: a thread that writes to it meanwhile waits, as it does in libc,
// and finds the new file. One that begins a call while the C library has the
// stream between its two descriptors takes it for a stream that is not
// monitored: what that call writes is recorded with the buffer it lands in.
typedef FILE *(*unixfreopen_t)(const char *pathname, const char *modes, FILE *fp);

static FILE *reopenStream(unixfreopen_t real, const char *pathname, const char *modes, FILE *fp) {
    if (!init || leavesRecordsAlone()) {
        return (*real)(pathname, modes, fp);
    }
    timer->start();
    MonitorFile *left = NULL;  // the file the stream leaves, if it is monitored
    MonitorFile *stale = NULL; // one still recorded under the number the stream gets
    int kept = -1;             // a descriptor of the file it leaves, for its size
    bool monitored = false;
    FILE *reopened;
    {
        StreamLock lock(fp);
        const int fd = fp->_fileno;
        {
            std::lock_guard<CountedMutex> guard(fdToFileMapMutex);
            fdToFileMap.erase(fd);
        }
        std::string name;
        int flush_errno = 0; // freopen() ignores a buffer it cannot write
        const bool leaves = finishStream(fp, flush_errno, &name);
        if (leaves) {
            kept = (*NEXT_SYMBOL(unixfcntl_t, "fcntl"))(fd, F_DUPFD_CLOEXEC, 3);
        }

        reopened = (*real)(pathname, modes, fp);
        int saved_errno = errno;
        if (pathname) {
            name = (MonitorFile::matchesPatterns(pathname) && !isMonitorOwnedPath(pathname, Config::dataLifeOutputPath)) ? pathname : "";
        }
        MonitorFile *file = NULL;
        const int new_fd = reopened ? reopened->_fileno : -1;
        if (reopened && !name.empty()) {
            file = MonitorFile::addNewMonitorFile(MonitorFile::TrackLocal, name, name, new_fd);
        }
        monitored = (file != NULL);
        // The C library gives the stream its old number back when it can: the
        // record of that number then goes from the one file to the other.
        if (leaves && new_fd != fd) {
            left = MonitorFileDescriptor::replaceMonitorFileDescriptor(fd, NULL, 0);
        }
        if (file || (leaves && new_fd == fd)) {
            MonitorFile *replaced = MonitorFileDescriptor::replaceMonitorFileDescriptor(
                new_fd, file, file ? file->newFilePosIndex(new_fd) : 0);
            if (leaves && new_fd == fd) {
                left = replaced;
            } else {
                stale = replaced;
            }
        }
        errno = saved_errno;
    }
    int saved_errno = errno;
    MonitorFileDescriptor::closeMonitorFile(stale, -1);
    MonitorFileDescriptor::closeMonitorFile(left, kept);
    if (kept >= 0) {
        (*unixclose)(kept);
    }
    errno = saved_errno;
    const bool reading = modes && modes[0] == 'r';
    timer->end(monitored ? Timer::MetricType::monitor : Timer::MetricType::system,
               reading ? Timer::Metric::in_fopen : Timer::Metric::out_fopen);
    return reopened;
}

INTERPOSE(FILE *, freopen, (const char *pathname, const char *modes, FILE *fp)) {
    return reopenStream(NEXT_SYMBOL(unixfreopen_t, "freopen"), pathname, modes, fp);
}

INTERPOSE(FILE *, freopen64, (const char *pathname, const char *modes, FILE *fp)) {
    return reopenStream(NEXT_SYMBOL(unixfreopen_t, "freopen64"), pathname, modes, fp);
}

off_t monitorRewind(MonitorFile *file, unsigned int pos, int fd, int fd2, off_t offset, int whence) {
    auto ret = (off_t)file->seek(fd, offset, whence, pos);
    return ret;
}

// rewind() is fseek() to the start, with the stream's error indicator cleared.
void rewind(FILE *fp) {
    fseek(fp, 0L, SEEK_SET);
    clearerr(fp);
}

ssize_t monitorMmapRead(MonitorFile *file, void *addr, size_t length, off_t offset) {
    DPRINTF("Lib.cpp: monitorMmapRead: addr=%p, length=%zu, offset=%lld\n",
            addr, length, (long long)offset);

    // Just record the number of bytes into the timer (simulate read)
    timer->addAmt(Timer::MetricType::monitor, Timer::Metric::read, length);

    return length;
}

ssize_t monitorMmapWrite(MonitorFile *file, void *addr, size_t length, off_t offset) {
    DPRINTF("Lib.cpp: monitorMmapWrite: addr=%p, length=%zu, offset=%lld\n",
            addr, length, (long long)offset);

    // Just record the number of bytes into the timer (simulate write)
    timer->addAmt(Timer::MetricType::monitor, Timer::Metric::write, length);

    return length;
}

// After the real mmap() or mmap64() returned: the whole mapping counts as read,
// written or both, as its protection says, when it is mapped.
static void trackMapping(void *result, size_t length, int prot, int fd, off64_t offset) {
    if (!init || isMonitorInternalIO()) {
        return; // as in close(); before the constructor the map of descriptors is not there
    }

    // Look up file path from fd
    std::lock_guard<CountedMutex> lock(fdToFileMapMutex);
    auto it = fdToFileMap.find(fd);
    if (it != fdToFileMap.end()) {
        const std::string &filePath = it->second;
        MonitorFile *file = MonitorFile::lookUpMonitorFile(filePath);
        if (file) {
            if (prot & PROT_READ) {
                monitorMmapRead(file, result, length, offset);
            }
            if (prot & PROT_WRITE) {
                monitorMmapWrite(file, result, length, offset);
            }
        }
    }
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    if (!unixmmap) unixmmap = (mmap_t)dlsym(RTLD_NEXT, "mmap");

    DPRINTF("Lib.cpp: Intercepting mmap: addr=%p, length=%zu, prot=%d, flags=%d, fd=%d, offset=%ld\n",
            addr, length, prot, flags, fd, offset);

    void *result = unixmmap(addr, length, prot, flags, fd, offset);
    trackMapping(result, length, prot, fd, offset);
    return result;
}

// What mmap() is in a program built with _FILE_OFFSET_BITS=64: Python, among
// others. Looked up in the constructor, as for mmap(): a memory allocator that
// stands in for the C library's maps memory in the middle of a lookup.
INTERPOSE(void *, mmap64, (void *addr, size_t length, int prot, int flags, int fd, off64_t offset)) {
    if (!unixmmap64) unixmmap64 = (mmap64_t)dlsym(RTLD_NEXT, "mmap64");
    void *result = unixmmap64(addr, length, prot, flags, fd, offset);
    trackMapping(result, length, prot, fd, offset);
    return result;
}

// void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
//     if (!unixmmap) unixmmap = (mmap_t)dlsym(RTLD_NEXT, "mmap");
//     DPRINTF("Lib.cpp: Intercepting mmap: addr=%p, length=%zu, prot=%d, flags=%d, fd=%d, offset=%ld\n",
//            addr, length, prot, flags, fd, offset);

//     // Call the original `mmap`
//     return unixmmap(addr, length, prot, flags, fd, offset);
// }

/* POSIX I/O */
ssize_t monitorPread(MonitorFile *file, unsigned int pos, int fd, void *buf, size_t count, off_t offset) {
    DPRINTF("Lib.cpp: In monitor pread: fd=%d, count=%zu, offset=%lld\n", fd, count, (long long)offset);

    // Use the file object to perform the read operation
    auto read_bytes = (ssize_t)file->read(fd, buf, count, pos, offset);

    // Update the timer with the number of bytes read
    if (read_bytes > 0) {
        timer->addAmt(Timer::MetricType::monitor, Timer::Metric::read, read_bytes);
    }

    return read_bytes;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    DPRINTF("Lib.cpp: Invoking pread: fd=%d, count=%zu, offset=%lld\n", fd, count, (long long)offset);

    auto ret = outerWrapper("pread", fd, Timer::Metric::read, monitorPread, unixpread, fd, buf, count, offset);
    return ret;
}

ssize_t monitorPwrite(MonitorFile *file, unsigned int pos, int fd, const void *buf, size_t count, off_t offset) {
    DPRINTF("Lib.cpp: In monitor pwrite: fd=%d, count=%zu, offset=%ld\n", fd, count, offset);

    // Pass `offset` to the TrackFile::write method
    auto bytes_written = (ssize_t)file->write(fd, buf, count, pos, offset);

    if (bytes_written > 0) {
        timer->addAmt(Timer::MetricType::monitor, Timer::Metric::write, bytes_written);
    }
    return bytes_written;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    DPRINTF("Lib.cpp: Invoking pwrite: fd=%d, count=%zu, offset=%ld\n", fd, count, offset);

    auto ret = outerWrapper("pwrite", fd, Timer::Metric::write,
                            monitorPwrite, unixpwrite, fd, buf, count, offset);
    return ret;
}

ssize_t monitorPread64(MonitorFile *file, unsigned int pos, int fd, void *buf, size_t count, off64_t offset) {
    DPRINTF("Lib.cpp: In monitor pread64: fd=%d, count=%zu, offset=%lld\n", fd, count, (long long)offset);

    // Use the file object to perform the read operation
    auto read_bytes = (ssize_t)file->read(fd, buf, count, pos, offset);

    // Update the timer with the number of bytes read
    if (read_bytes > 0) {
        timer->addAmt(Timer::MetricType::monitor, Timer::Metric::read, read_bytes);
    }

    return read_bytes;
}

ssize_t pread64(int fd, void *buf, size_t count, off64_t offset) {
    DPRINTF("Lib.cpp: Invoking pread64: fd=%d, count=%zu, offset=%lld\n", fd, count, (long long)offset);
    auto ret = outerWrapper("pread64", fd, Timer::Metric::read, monitorPread64, unixpread64, fd, buf, count, offset);
    return ret;
}



ssize_t monitorPwrite64(MonitorFile *file, unsigned int pos, int fd, const void *buf, size_t count, off64_t offset) {
    DPRINTF("Lib.cpp: In monitor pwrite64: fd=%d, count=%zu, offset=%ld\n", fd, count, offset);
    // Pass the correct `pos` as the `index` to TrackFile::write
    // auto bytes_written = (ssize_t)file->write(buf, count, pos);
    auto bytes_written = (ssize_t)file->write(fd, buf, count, pos, offset);
    if (bytes_written > 0) {
        timer->addAmt(Timer::MetricType::monitor, Timer::Metric::write, bytes_written);
    }
    return bytes_written;
}

ssize_t pwrite64(int fd, const void *buf, size_t count, off64_t offset) {
    DPRINTF("Lib.cpp: Invoking pwrite64: fd=%d, count=%zu, offset=%ld\n", 
        fd, count, offset);
    auto ret = outerWrapper("pwrite64", fd, Timer::Metric::write, 
                            monitorPwrite64, unixpwrite64, fd, buf, count, offset);
    return ret;
}

/* Fortified reads ***************************************************************************************/

// With _FORTIFY_SOURCE, the default when optimizing on most distributions, a
// program calls these instead of read(), pread(), fread(), fgets() and their
// unlocked variants whenever the compiler knows the buffer's size but not the
// request's. They check as glibc's do, then read through the wrappers above.
extern "C" {

void __chk_fail(void) __attribute__((noreturn));

ssize_t __read_chk(int fd, void *buf, size_t nbytes, size_t buflen) {
    if (nbytes > buflen) {
        __chk_fail();
    }
    return read(fd, buf, nbytes);
}

ssize_t __pread_chk(int fd, void *buf, size_t nbytes, off_t offset, size_t buflen) {
    if (nbytes > buflen) {
        __chk_fail();
    }
    return pread(fd, buf, nbytes, offset);
}

ssize_t __pread64_chk(int fd, void *buf, size_t nbytes, off64_t offset, size_t buflen) {
    if (nbytes > buflen) {
        __chk_fail();
    }
    return pread64(fd, buf, nbytes, offset);
}

static void checkFread(size_t ptrlen, size_t size, size_t n) {
    if ((size != 0 && n > SIZE_MAX / size) || size * n > ptrlen) {
        __chk_fail();
    }
}

size_t __fread_chk(void *__restrict ptr, size_t ptrlen, size_t size, size_t n, FILE *__restrict fp) {
    checkFread(ptrlen, size, n);
    return fread(ptr, size, n, fp);
}

size_t __fread_unlocked_chk(void *__restrict ptr, size_t ptrlen, size_t size, size_t n, FILE *__restrict fp) {
    checkFread(ptrlen, size, n);
    return fread_unlocked(ptr, size, n, fp);
}

// glibc's __fgets_chk reads at most size bytes and fails only if the line needs
// more, so the real call does the reading and the checking.
char *__fgets_chk(char *__restrict s, size_t size, int n, FILE *__restrict fp) {
    return outerWrapper("__fgets_chk", fp, Timer::Metric::fgets, monitorFgetsChk, unix__fgets_chk, s, size, n, fp);
}

char *__fgets_unlocked_chk(char *__restrict s, size_t size, int n, FILE *__restrict fp) {
    return outerWrapper("__fgets_unlocked_chk", UnlockedStream{fp}, Timer::Metric::fgets, monitorFgetsUnlockedChk, unix__fgets_unlocked_chk, s, size, n, fp);
}

/* Fortified opens ***************************************************************************************/

// With _FORTIFY_SOURCE, an open() without a mode whose flags are not a
// compile-time constant becomes __open_2() (open64, openat and openat64
// likewise), which glibc aborts in if the flags need a mode. libncbi-vdb in
// distribution builds of the SRA toolkit opens fasterq-dump's output this way.
// The opens below pass a mode so that this file, itself fortified when
// optimized, does not compile them back into calls to these functions.
// Flags that need a mode go to the real function, which reports it and aborts.
typedef int (*unixopen_2_t)(const char *path, int flags);
typedef int (*unixopenat_2_t)(int dirfd, const char *path, int flags);

int __open_2(const char *path, int flags) {
    if (openNeedsMode(flags)) {
        return (*(unixopen_2_t)dlsym(RTLD_NEXT, "__open_2"))(path, flags);
    }
    return open(path, flags, 0);
}

int __open64_2(const char *path, int flags) {
    if (openNeedsMode(flags)) {
        return (*(unixopen_2_t)dlsym(RTLD_NEXT, "__open64_2"))(path, flags);
    }
    return open64(path, flags, 0);
}

int __openat_2(int dirfd, const char *path, int flags) {
    if (openNeedsMode(flags)) {
        return (*(unixopenat_2_t)dlsym(RTLD_NEXT, "__openat_2"))(dirfd, path, flags);
    }
    return openat(dirfd, path, flags, 0);
}

int __openat64_2(int dirfd, const char *path, int flags) {
    if (openNeedsMode(flags)) {
        return (*(unixopenat_2_t)dlsym(RTLD_NEXT, "__openat64_2"))(dirfd, path, flags);
    }
    return openat64(dirfd, path, flags, 0);
}

} // extern "C"