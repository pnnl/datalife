#include "Config.h"
#include "ConnectionPool.h"
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <mutex>
#include <sstream>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fnmatch.h>
#include <limits.h>
#include <errno.h>
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
#include "MonitorFileStream.h"
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

// #define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#ifdef LIBDEBUG
#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
#define DPRINTF(...)
#endif
#define MONITOR_ID "MONITOR"
#define MONITOR_ID_LEN 5 
#define MONITOR_VERSION "0.1"
#define MONITOR_VERSION_LEN 3 //5+3

#define TRACKFILECHANGES 1 // tmp-ly added

class CleanupTrackFile {
public:
  CleanupTrackFile(){}
  ~CleanupTrackFile() {
  auto next_iter = Trackable<int, MonitorFileDescriptor *>::begin();
  auto itered = Trackable<int, MonitorFileDescriptor *>::end();
  while (next_iter != itered) {
    next_iter = Trackable<int, MonitorFileDescriptor *>::next(next_iter);
    if (next_iter == Trackable<int, MonitorFileDescriptor *>::end()) {
      break;
    }
    auto fd_ = next_iter->first;
    auto file_desc = next_iter->second;
    auto file = file_desc->getFile();
    DPRINTF("Calling CleanupTrackFile() \n");
    file->close();
    MonitorFile::removeMonitorFile(file);
    MonitorFileDescriptor::removeMonitorFileDescriptor(fd_);
  }
  }
};

void __attribute__((constructor)) monitorInit(void) {
    std::call_once(log_flag, []() {
        timer = new Timer();

        timer->start();
        Loggable::mtx_cout = new std::mutex();
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
                DPRINTF("%s\n",f.c_str());
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
        unixfopen = (unixfopen_t)dlsym(RTLD_NEXT, "fopen");
        unixfopen64 = (unixfopen_t)dlsym(RTLD_NEXT, "fopen64");
        unixfclose = (unixfclose_t)dlsym(RTLD_NEXT, "fclose");
        unixfread = (unixfread_t)dlsym(RTLD_NEXT, "fread");
        unixfwrite = (unixfwrite_t)dlsym(RTLD_NEXT, "fwrite");
        unixftell = (unixftell_t)dlsym(RTLD_NEXT, "ftell");
        unixfstat = (unixfstat_t)dlsym(RTLD_NEXT, "fstat");
        unixfseek = (unixfseek_t)dlsym(RTLD_NEXT, "fseek");
        unixrewind = (unixrewind_t)dlsym(RTLD_NEXT, "rewind");
        unixfgetc = (unixfgetc_t)dlsym(RTLD_NEXT, "fgetc");
        unixfgets = (unixfgets_t)dlsym(RTLD_NEXT, "fgets");
        unixfputc = (unixfputc_t)dlsym(RTLD_NEXT, "fputs");
        unixfputs = (unixfputs_t)dlsym(RTLD_NEXT, "fputs");
        unixflockfile = (unixflockfile_t)dlsym(RTLD_NEXT, "flockfile");
        unixftrylockfile = (unixftrylockfile_t)dlsym(RTLD_NEXT, "ftrylockfile");
        unixfunlockfile = (unixfunlockfile_t)dlsym(RTLD_NEXT, "funlockfile");
        unixfflush = (unixfflush_t)dlsym(RTLD_NEXT, "fflush");
        unixfeof = (unixfeof_t)dlsym(RTLD_NEXT, "feof");
        unixreadv = (unixreadv_t)dlsym(RTLD_NEXT, "readv");
        unixwritev = (unixwritev_t)dlsym(RTLD_NEXT, "writev");
        unixexit = (unixexit_t)dlsym(RTLD_NEXT, "exit");
        unix_exit = (unix_exit_t)dlsym(RTLD_NEXT, "_exit");
        unix_Exit = (unix_Exit_t)dlsym(RTLD_NEXT, "_Exit");
        unix_exit_group = (unix_exit_group_t)dlsym(RTLD_NEXT, "exit_group");
        unix_vfprintf = (unix_vfprintf_t)dlsym(RTLD_NEXT, "vfprintf");
        unixmmap = (mmap_t)dlsym(RTLD_NEXT, "mmap");
        // Load the functions dynamically using dlsym
        unixpread = (pread_t)dlsym(RTLD_NEXT, "pread");
        unixpwrite = (pwrite_t)dlsym(RTLD_NEXT, "pwrite");
        unixpread64 = (pread64_t)dlsym(RTLD_NEXT, "pread64");
        unixpwrite64 = (pwrite64_t)dlsym(RTLD_NEXT, "pwrite64");

        //enable if running into issues with an application that launches child shells
        bool unsetLib = getenv("MONITOR_UNSET_LIB") ? atoi(getenv("MONITOR_UNSET_LIB")) : 0;
        if (unsetLib){
            unsetenv("LD_PRELOAD"); 
        }    
    
        timer->end(Timer::MetricType::monitor, Timer::Metric::constructor);
        //*InputFile::_time_of_last_read = std::chrono::high_resolution_clock::now();
    });
    init = true;
}

void __attribute__((destructor)) monitorCleanup(void) {
    // Removed the manual destructor call.
    static CleanupTrackFile cleanup;

    timer->start();
    init = false; //set to false because we can't ensure our static members have not already been deleted.

    curlEnd(Config::curlOnStartup);
    curlDestroy;

    if (Config::printStats) {
        std::cout << "[MONITOR] " << "Exiting Client" << std::endl;
        if (ConnectionPool::useCnt->size() > 0) {
            for (auto conUse : *ConnectionPool::useCnt) {
                std::cout << "[MONITOR] connection: " << conUse.first << " num_tx: " << conUse.second << " amount: " << (*ConnectionPool::stats)[conUse.first].first << " B time: " << (*ConnectionPool::stats)[conUse.first].second << " s avg BW: " << ((*ConnectionPool::stats)[conUse.first].first / (*ConnectionPool::stats)[conUse.first].second) / 1000000 << "MB/s" << std::endl;
            }
        }
        delete track_files;
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
    std::lock_guard<std::mutex> lock(fdToFileMapMutex);
    auto it = fdToFileMap.find(fd);
    if (it != fdToFileMap.end()) {
        return it->second;
    }
    return "";
}

int removeStr(char *s, const char *r) {
    char *ptr = strstr(s, r);
    if (ptr == NULL)
        return -1;
    strcpy(ptr, ptr + strlen(r));
    return 0;
}



int trackFileOpen(std::string name, std::string metaName, MonitorFile::Type type, const char *pathname, int flags, int mode) {
  DPRINTF("trackfileOpen: %s %s %u\n", name.c_str(), metaName.c_str(), type);

  // Add O_CREAT if file creation is happens
  if (flags & O_RDWR && !(flags & O_CREAT)) {
    DPRINTF("Adding O_CREAT flag to open call.\n");
    flags |= O_CREAT;
  }

  auto fd = (*unixopen64)(name.c_str(), flags, mode);
  if (fd > 0) {  
    MonitorFile *file = MonitorFile::addNewMonitorFile(type, name, name, fd, true);
    if (file) {
      bool descriptorAdded = MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex());
      if (descriptorAdded) {
          file->open();
          DPRINTF("trackFileOpen() add new file success: %s , fd = %d\n", pathname, fd);
      } else {
          DPRINTF("trackFileOpen() Failed to add monitor file descriptor for file: %s, fd = %d\n", pathname, fd);
          // Optionally clean up the `MonitorFile` object if the descriptor addition fails
          MonitorFile::removeMonitorFile(file);
          close(fd);
          fd = -1; // Indicate failure
      }
    } 
  } else {
    DPRINTF("fd value %d\n", fd);
    DPRINTF("Error opening file: %s flag: %d mode: %d \n", strerror(errno), flags, mode);
  }
  return fd;
}

/*Posix******************************************************************************************************/

int monitorOpen(std::string name, std::string metaName, MonitorFile::Type type, const char *pathname, int flags, int mode) {
  auto fd = (*unixopen64)(metaName.c_str(), O_RDONLY, 0);
  MonitorFile *file = MonitorFile::addNewMonitorFile(type, name, metaName, fd);
  if (file) {
    // MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex());
    bool descriptorAdded = MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex());
    if (descriptorAdded) {
        file->open();
        DPRINTF("monitorOpen add new file success: %s , fd = %d\n", pathname, fd);
    } else {
        DPRINTF("monitorOpen() Failed to add monitor file descriptor for file: %s, fd = %d\n", pathname, fd);
        // Optionally clean up the `MonitorFile` object if the descriptor addition fails
        MonitorFile::removeMonitorFile(file);
        close(fd);
        fd = -1; // Indicate failure
    }
  } else if(fd != -1) {
    DPRINTF("monitorOpen add new monitor file failed: %s %s %d\n", name.c_str(), metaName.c_str(), fd);
    (*unixclose)(fd);
    fd = -1;
  }
  return fd;
}

int open(const char *pathname, int flags, ...) {
    DPRINTF("Open %s: \n", pathname);
    int mode = 0;
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);

    Timer::Metric metric = (flags & O_WRONLY || flags & O_RDWR) ? Timer::Metric::out_open : Timer::Metric::in_open;

    // Check if the file matches any pattern
    for (auto pattern : patterns) {
        if (fnmatch(pattern.c_str(), pathname, 0) == 0) {
            DPRINTF("open() Firing off trackFileOpen for %s\n", pathname);
            int fd = outerWrapper("open", pathname, metric, trackFileOpen, unixopen, pathname, flags, mode);

#ifdef HDF5_IO
            // Double check if file MinitorFile is added 
            MonitorFile *file = nullptr;
            unsigned int pos = 0;

            // Check if the file descriptor is already associated with a MonitorFile
            if (!MonitorFileDescriptor::lookupMonitorFileDescriptor(fd, file, pos)) {
                DPRINTF("MonitorFile not found for fd %d. Attempting to add...\n", fd);

                // Dynamically add a new MonitorFile
                file = MonitorFile::addNewMonitorFile(MonitorFile::Type::TrackLocal, pathname, pathname, fd, true);
                if (file) {
                    bool descriptorAdded = MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex());
                    if (descriptorAdded) {
                        file->open();
                        DPRINTF("open() MonitorFile successfully added for fd=%d, pathname=%s\n", fd, pathname);
                    } else {
                        DPRINTF("open() Failed to add MonitorFileDescriptor for fd=%d, pathname=%s\n", fd, pathname);
                        // Cleanup if descriptor addition fails
                        MonitorFile::removeMonitorFile(file);
                    }
                } else {
                    DPRINTF("Failed to add MonitorFile for fd=%d, pathname=%s\n", fd, pathname);
                }
            }
#endif

            if (fd >= 0) {
                // Track the fd and pathname
                std::lock_guard<std::mutex> lock(fdToFileMapMutex);
                fdToFileMap[fd] = std::string(pathname);
            } else {
                DPRINTF("trackFileOpen failed for %s\n", pathname);
            }
            return fd;
        }
    }

    // If no pattern matched, fallback to monitorOpen
    DPRINTF("open() Firing off monitorOpen for %s\n", pathname);
    int fd = outerWrapper("open", pathname, metric, monitorOpen, unixopen, pathname, flags, mode);

    if (fd >= 0) {
        // Track the fd and pathname
        std::lock_guard<std::mutex> lock(fdToFileMapMutex);
        fdToFileMap[fd] = std::string(pathname);
    } else {
        DPRINTF("monitorOpen failed for %s\n", pathname);
    }

    return fd;
}

int open64(const char *pathname, int flags, ...) {
  DPRINTF("Open64 %s: \n", pathname);
    int mode = 0;
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);

    Timer::Metric metric = (flags & O_WRONLY || flags & O_RDWR) ? Timer::Metric::out_open : Timer::Metric::in_open;
#ifdef HDF5_IO
            currFileName = pathname;
#endif
    for (auto pattern: patterns) {
        auto ret_val = fnmatch(pattern.c_str(), pathname, 0);
        if (ret_val == 0) {
            DPRINTF("open64() Firing off trackfileopen for %s \n ", pathname);

            return outerWrapper("open64", pathname, metric, trackFileOpen, unixopen64, 
			    pathname, flags, mode);
        }
    }

    DPRINTF("Open64 %s: \n", pathname);
    return outerWrapper("open64", pathname, metric, monitorOpen, unixopen64, pathname, flags, mode);
}

int monitorOpenat(std::string name, std::string metaName, MonitorFile::Type type, 
		int dirfd, const char *pathname, int flags, int mode) {
  return (*unixopenat)(dirfd, name.c_str(), flags);
}

int trackFileOpenat(std::string name, std::string metaName, MonitorFile::Type type, 
		    int dirfd, const char *pathname, int flags, int mode) {
  DPRINTF("trackfileOpenat: %s %s %u\n", name.c_str(), metaName.c_str(), type);
  auto fd = (*unixopenat)(dirfd, name.c_str(), flags);
  if (fd > 0) {  
    MonitorFile *file = MonitorFile::addNewMonitorFile(type, name, name, fd, true);
    if (file) {
      MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex());
      DPRINTF("trackFileOpen add new  file success: %s , fd = %d\n", pathname, fd);
    } 
  } else {
    DPRINTF("fd value %d\n", fd);
  }
  return fd;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
  int mode = 0;
  va_list arg;
  va_start(arg, flags);
  mode = va_arg(arg, int);
  va_end(arg);
  
  Timer::Metric metric = (flags & O_WRONLY || flags & O_RDWR) ? 
    Timer::Metric::out_open : Timer::Metric::in_open;
#ifdef HDF5_IO
      currFileName = pathname;
#endif
  DPRINTF("Openat %s: \n", pathname);
  for (auto pattern: patterns) {
    auto ret_val = fnmatch(pattern.c_str(), pathname, 0);
    if (ret_val == 0) {
      DPRINTF("Firing off trackfileopen for %s \n ", pathname);

      return outerWrapper("openat", pathname, metric, trackFileOpenat, unixopenat, 
			  dirfd, pathname, flags, mode);
    }
  }

  return outerWrapper("openat", pathname, metric, monitorOpenat, unixopenat, dirfd, 
		      pathname, flags, mode);
}

int monitorClose(MonitorFile *file, unsigned int fp, int fd) {
    DPRINTF("In monitor close for fd %d\n", fd);
#ifdef TRACKFILECHANGES
    for (auto pattern : patterns) {
        if (fnmatch(pattern.c_str(), file->name().c_str(), 0) == 0) {
            file->close();
            DPRINTF("Successfully closed a file with fd %d\n", fd);
            break;
        }
    }
#endif
    MonitorFile::removeMonitorFile(file);
    MonitorFileDescriptor::removeMonitorFileDescriptor(fd);
    return (*unixclose)(fd);
}

int close(int fd) {
    DPRINTF("Trying to close file with fd %d\n", fd);
    {
        std::lock_guard<std::mutex> lock(fdToFileMapMutex);
        fdToFileMap.erase(fd);
    }

    MonitorFile *file = nullptr;
    unsigned int pos = 0;

    if (MonitorFileDescriptor::lookupMonitorFileDescriptor(fd, file, pos)) {
        DPRINTF("MonitorFile found for fd %d. Invoking monitorClose.\n", fd);
    } else {
        DPRINTF("No MonitorFile found for fd %d. Invoking unixclose.\n", fd);
    }

    return outerWrapper("close", fd, Timer::Metric::close, monitorClose, unixclose, fd);
}


#ifdef TRACKRESOURCE
void exit(int status) {
  DPRINTF("Calling exit \n");
  auto iter = Trackable<int, MonitorFileDescriptor *>::begin();
  while (true) {
    auto next_iter = Trackable<int, MonitorFileDescriptor *>::next(iter);
    if (next_iter == Trackable<int, MonitorFileDescriptor *>::end()) {
      break;
    }
    auto fd_ = next_iter->first;
    auto file_desc = next_iter->second;
    auto file = file_desc->getFile();
    file->close();
    iter = next_iter;
  }
  (*unixexit)(status);
  // return outerWrapper("exit", fd, Timer::Metric::close, monitorClose, unixclose, fd);
}

void _exit(int status) {
  DPRINTF("Calling _exit \n");
  (*unix_exit)(status);
}

void _Exit(int status) {
  DPRINTF("Calling _Exit \n");
  (*unix_Exit)(status);
}

void exit_group(int status) {
  DPRINTF("Calling exit_group \n");
  (*unix_exit_group)(status);
}
#endif

ssize_t monitorRead(MonitorFile *file, unsigned int fp, int fd, void *buf, size_t count) {
  DPRINTF("In Monitor read\n");
  ssize_t ret = file->read(buf, count, fp);
  timer->addAmt(Timer::MetricType::monitor, Timer::Metric::read, ret);
  return ret;
}

ssize_t read(int fd, void *buf, size_t count) {
    vLock.readerLock();
    DPRINTF("Original read count %u\n", count);
    auto ret = outerWrapper("read", fd, Timer::Metric::read, monitorRead, unixread, fd, buf, count);
    vLock.readerUnlock();
    return ret;
}

ssize_t monitorWrite(MonitorFile *file, unsigned int fp, int fd, const void *buf, size_t count) {
    DPRINTF("In Monitor write\n");
    auto ret = file->write(buf, count, fp);
    timer->addAmt(Timer::MetricType::monitor, Timer::Metric::write, ret);
    DPRINTF("Returning from Monitor write\n");
    return ret;
}

ssize_t write(int fd, const void *buf, size_t count) {
    vLock.writerLock();
    DPRINTF("Printing fd in write %d and count %u\n", fd, count);
    auto ret = outerWrapper("write", fd, Timer::Metric::write, monitorWrite, unixwrite, fd, buf, count);
    vLock.writerUnlock();
    return ret;
}

template <typename T>
T monitorLseek(MonitorFile *file, unsigned int fp, int fd, T offset, int whence) {
    return (T)file->seek(offset, whence, fp);
}

off_t lseek(int fd, off_t offset, int whence) ADD_THROW {
    vLock.readerLock();
    auto ret = outerWrapper("lseek", fd, Timer::Metric::seek, monitorLseek<off_t>, unixlseek, fd, offset, whence);
    vLock.readerUnlock();
    return ret;
}

off64_t lseek64(int fd, off64_t offset, int whence) ADD_THROW {
    vLock.readerLock();
    auto ret = outerWrapper("lseek64", fd, Timer::Metric::seek, monitorLseek<off64_t>, unixlseek64, fd, offset, whence);
    vLock.readerUnlock();
    return ret;
}

thread_local unixxstat_t whichStat = NULL;
int innerStat(int version, const char *filename, struct stat *buf) { return whichStat(version, filename, buf); }

thread_local unixxstat64_t whichStat64 = NULL;
int innerStat(int version, const char *filename, struct stat64 *buf) { return whichStat64(version, filename, buf); }

// thread_local unixlstat_t whichLstat = NULL;
thread_local unixfstat_t whichFstat = NULL;

template <typename T>
int monitorStat(std::string name, std::string metaName, MonitorFile::Type type, int version, const char *filename, T *buf) {
  auto ret = innerStat(_STAT_VER, metaName.c_str(), buf);
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
    return 0;
}

int fsync(int fd) {
    vLock.readerLock();
    auto ret = outerWrapper("fsync", fd, Timer::Metric::stat, monitorFsync, unixfsync, fd);
    vLock.readerUnlock();
    return ret;
}

template <typename Func, typename FuncLocal>
ssize_t monitorVector(const char *name, Timer::Metric metric, Func monitorFun, FuncLocal localFun, int fd, const struct iovec *iov, int iovcnt) {
    ssize_t ret = 0;
    vLock.writerLock();
    for (int i = 0; i < iovcnt && ret < iovcnt; i++) {
        auto temp = outerWrapper(name, fd, metric, monitorFun, localFun, fd, iov[i].iov_base, iov[i].iov_len);
        if (temp == (ssize_t)-1) {
            ret = -1;
            break;
        }
        else
            ret += temp;
    }
    vLock.writerUnlock();
    return ret;
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return monitorVector("read", Timer::Metric::readv, monitorRead, unixread, fd, iov, iovcnt);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return monitorVector("write", Timer::Metric::writev, monitorWrite, unixwrite, fd, iov, iovcnt);
}

/*Streaming**************************************************************************************************/

FILE *trackFileFopen(std::string name, std::string metaName, MonitorFile::Type type, const char *__restrict fileName, const char *__restrict modes) {
  // DPRINTF("trackFileFopen: %s %s %u\n", name.c_str(), metaName.c_str(), type);
  DPRINTF("in trackFileFopen\n");
  FILE *fp = (*unixfopen)(name.c_str(), modes);
  if (fp) {
    int fd = fileno(fp);
    MonitorFile *file = MonitorFile::addNewMonitorFile(type, name, metaName, fd);
    if (file) {
      MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex());
      MonitorFileStream::addStream(fp, fd);
      DPRINTF("trackFileOpen add new  file success: %s , fd = %d\n", fileName, fd);
    }
  }
  return fp;
}

FILE *monitorFopen(std::string name, std::string metaName, MonitorFile::Type type, const char *__restrict fileName, const char *__restrict modes) {
  DPRINTF("monitorFOpen: %s %s %u\n", name.c_str(), metaName.c_str(), type);
    char m = 'r';
    FILE *fp = (*unixfopen)(fileName, &m);
    if (fp) {
        int fd = fileno(fp);
        MonitorFile *file = MonitorFile::addNewMonitorFile(type, name, metaName, fd);
        if (file) {
            MonitorFileDescriptor::addMonitorFileDescriptor(fd, file, file->newFilePosIndex());
            MonitorFileStream::addStream(fp, fd);
        }
    }
    return fp;
}

FILE *fopen(const char *__restrict fileName, const char *__restrict modes) {
  DPRINTF("Calling fopen on %s \n", fileName);  
  Timer::Metric metric = (modes[0] == 'r') ? Timer::Metric::in_fopen : Timer::Metric::out_fopen;
#ifdef HDF5_IO
      currFileName = fileName;
#endif
  for (auto pattern: patterns) {
    auto ret_val = fnmatch(pattern.c_str(), fileName, 0);
    if (ret_val == 0) {

      return outerWrapper("fopen", fileName, metric, trackFileFopen, unixfopen, 
			  fileName, modes);
    }
  }
 
  return outerWrapper("fopen", fileName, metric, monitorFopen, unixfopen, fileName, modes);
}

FILE *fopen64(const char *__restrict fileName, const char *__restrict modes) {
  DPRINTF("Calling fopen64 on %s \n", fileName);  
  Timer::Metric metric = (modes[0] == 'r') ? Timer::Metric::in_fopen : Timer::Metric::out_fopen;

#ifdef HDF5_IO
      currFileName = fileName;
#endif

  for (auto pattern: patterns) {
    auto ret_val = fnmatch(pattern.c_str(), fileName, 0);
    if (ret_val == 0 
    // && (strstr(fileName, "_r_stat") || strstr(fileName, "_w_stat") || strstr(fileName, "_trace_stat"))
    ) {
      DPRINTF("fopen64() Found pattern [%s] \n", pattern.c_str());
      return outerWrapper("fopen64", fileName, metric, trackFileFopen, unixfopen64, 
			  fileName, modes);
    }
  }  

    return outerWrapper("fopen64", fileName, metric, monitorFopen, unixfopen64, fileName, modes);
}

int monitorFclose(MonitorFile *file, unsigned int pos, int fd, FILE *fp) {
  DPRINTF("In monitor fclose \n");
#ifdef TRACKFILECHANGES
  for (auto pattern: patterns) {
    auto ret_val = fnmatch(pattern.c_str(), file->name().c_str(), 0);
    if (ret_val == 0) {
      file->close();
      DPRINTF("Successfully closed a file with fd %d\n", fd);
    }
  }
#endif
    MonitorFile::removeMonitorFile(file);
    MonitorFileDescriptor::removeMonitorFileDescriptor(fd);
    
#ifdef TRACKFILECHANGES
    return 0;
#else
    return (*unixfclose)(fp);
#endif
}

int fclose(FILE *fp) {
    DPRINTF("Invoking fclose \n");
    return outerWrapper("fclose", fp, Timer::Metric::close, monitorFclose, unixfclose, fp);
}

size_t monitorFread(MonitorFile *file, unsigned int pos, int fd, void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
  DPRINTF("In monitor fread \n");
    auto read_bytes = (size_t)file->read(ptr, size * n, pos);
    if (read_bytes >= size){return n;}
    else return (size_t) (size / n) ;
}

size_t fread(void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
  DPRINTF("Invoking fread \n");
  auto ret_val = outerWrapper("fread", fp, Timer::Metric::read, 
			      monitorFread, unixfread, ptr, size, n, fp);
  DPRINTF("fread return value %d\n", ret_val);
  return ret_val;
}

size_t monitorFwrite(MonitorFile *file, unsigned int pos, int fd, const void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
    DPRINTF("In monitor fwrite \n");
    auto written_bytes = (size_t)file->write(ptr, size * n, pos);
    if (written_bytes >= size) return n;
    else return (size_t) (size / n);
}

size_t fwrite(const void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
    DPRINTF("Invoking fwrite \n");
#ifdef HDF5_IO
    int fd = fileno(fp);
    std::string fileName =  getFileNameFromFd(fd);
    DPRINTF("Invoking fwrite on fileName: %s", fileName.c_str());
#endif

    //return outerWrapper("fwrite", fp, Timer::Metric::read, monitorFwrite, unixfwrite, ptr, size, n, fp);
    return outerWrapper("fwrite", fp, Timer::Metric::write, monitorFwrite, unixfwrite, ptr, size, n, fp);
}

int monitorVfprintf(MonitorFile *file, unsigned int pos, int fd, FILE * stream, 
		     const char * format, ...) {
  va_list args;
  va_start(args, format);
  auto count = unix_vfprintf(stream, format, args);
  auto i = file->vfprintf(pos, count);
  return count;
}
int vfprintf(FILE * stream, const char * format, va_list arg ) {
  //DPRINTF("Invoking vfprintf\n");
  return outerWrapper("vfprintf", stream, Timer::Metric::write, monitorVfprintf, 
		      unix_vfprintf, stream, format, arg);
}

long int monitorFtell(MonitorFile *file, unsigned int pos, int fd, FILE *fp) {
    return (long int)lseek(fd, 0, SEEK_CUR);
}

long int ftell(FILE *fp) {
    return outerWrapper("ftell", fp, Timer::Metric::ftell, monitorFtell, unixftell, fp);
}

int monitorFseek(MonitorFile *file, unsigned int pos, int fd, FILE *fp, long int off, int whence) {
    return lseek(fd, off, whence);
}

int fseek(FILE *fp, long int off, int whence) {
    return outerWrapper("fseek", fp, Timer::Metric::seek, monitorFseek, unixfseek, fp, off, whence);
}

int monitorFgetc(MonitorFile *file, unsigned int pos, int fd, FILE *fp) {
    if (!file->eof(pos)) {
        unsigned char buffer;
        read(fd, &buffer, 1);
        return (int)buffer;
    }
    return EOF;
}

int fgetc(FILE *fp) {
    return outerWrapper("fgetc", fp, Timer::Metric::fgetc, monitorFgetc, unixfgetc, fp);
}

char *monitorFgets(MonitorFile *file, unsigned int pos, int fd, char *__restrict s, int n, FILE *__restrict fp) {
    char *ret = NULL;
    if (!file->eof(pos)) {
        int prior = file->filePos(pos);
        size_t res = read(fd, s, n - 1);
        if (res) {
            unsigned int index;
            for (index = 0; index < res; index++) {
                if (s[index] == '\n') {
                    index++;
                    break;
                }
            }

            if (index < res) {
                file->seek(prior + index, SEEK_SET, pos);
            }

            s[index] = '\0';
            ret = s;
        }
    }
    return ret;
}

char *fgets(char *__restrict s, int n, FILE *__restrict fp) {
    return outerWrapper("fgets", fp, Timer::Metric::fgets, monitorFgets, unixfgets, s, n, fp);
}

int monitorFputc(MonitorFile *file, unsigned int pos, int fd, int c, FILE *fp) {
    write(fd, (void *)&c, 1);
    return c;
}

int fputc(int c, FILE *fp) {
    return outerWrapper("fputc", fp, Timer::Metric::fputc, monitorFputc, unixfputc, c, fp);
}

int monitorFputs(MonitorFile *file, unsigned int pos, int fd, const char *__restrict s, FILE *__restrict fp) {
    unsigned int index = 0;
    while (1) {
        if (s[index] == '\0')
            break;
        index++;
    }

    int res = -1;
    if (index)
        res = write(fd, s, index);

    if (res == -1)
        return EOF;
    return res;
}

int fputs(const char *__restrict s, FILE *__restrict fp) {
    return outerWrapper("fputs", fp, Timer::Metric::fputs, monitorFputs, unixfputs, s, fp);
}

int monitorFeof(MonitorFile *file, unsigned int pos, int fd, FILE *fp) {
    return file->eof(pos);
}

int feof(FILE *fp) ADD_THROW {
    return outerWrapper("feof", fp, Timer::Metric::feof, monitorFeof, unixfeof, fp);
}

off_t monitorRewind(MonitorFile *file, unsigned int pos, int fd, int fd2, off_t offset, int whence) {
    auto ret = (off_t)file->seek(offset, whence, pos);
    return ret;
}

void rewind(FILE *fp) {
    // ReaderWriterLock * lock = NULL;
    // int fd = MonitorFileStream::lookupStream(fp, lock);
    // outerWrapper(fp, Timer::Metric::rewind, monitorRewind, unixlseek, fd, 0L, SEEK_SET);
    fseek(fp, 0L, SEEK_SET);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    if (!unixmmap) unixmmap = (mmap_t)dlsym(RTLD_NEXT, "mmap");
    DPRINTF("Intercepting mmap: addr=%p, length=%zu, prot=%d, flags=%d, fd=%d, offset=%ld\n",
           addr, length, prot, flags, fd, offset);

    // Call the original `mmap`
    return unixmmap(addr, length, prot, flags, fd, offset);
}


ssize_t monitorPread(MonitorFile *file, unsigned int pos, int fd, void *buf, size_t count, off_t offset) {
    DPRINTF("In monitor pread: fd=%d, count=%zu, offset=%ld\n", fd, count, offset);
    auto read_bytes = (ssize_t)file->read(buf, count, pos + offset);
    return read_bytes;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    vLock.readerLock();
    DPRINTF("Invoking pread: fd=%d, count=%zu, offset=%ld\n", fd, count, offset);
    auto ret = outerWrapper("pread", fd, Timer::Metric::read, 
                            monitorPread, unixpread, fd, buf, count, offset);
    vLock.readerUnlock();
    return ret;
}

ssize_t monitorPwrite(MonitorFile *file, unsigned int pos, int fd, const void *buf, size_t count, off_t offset) {
    DPRINTF("In monitor pwrite: fd=%d, count=%zu, offset=%ld\n", fd, count, offset);
    auto written_bytes = (ssize_t)file->write(buf, count, pos + offset);
    return written_bytes;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    vLock.writerLock(); // Lock for thread safety

    // Retrieve the file name dynamically
    std::string fileName = getFileNameFromFd(fd);
    DPRINTF("Invoking pwrite: fd=%d, count=%zu, offset=%ld, fileName=%s\n",
           fd, count, offset, fileName.empty() ? "UNKNOWN" : fileName.c_str());

    if (!fileName.empty()) {
#ifdef HDF5_IO

        // Update tracking statistics
        DPRINTF("Tracking track_file_blk_w_stat[%s]\n", fileName.c_str());
        auto pid = std::to_string(getpid());

        // Update block write statistics
        track_file_blk_w_stat[fileName][offset]++;
        track_file_blk_w_stat_size[fileName][offset] += count;

        // Maintain write order
        trace_write_blk_seq[fileName].push_back(offset);

        // // Optionally write block access stats to file
        // std::string traceFileName = fileName + "_" + pid + "_w_trace_stat";
        // std::ofstream traceFile(traceFileName, std::ios::out | std::ios::app);
        // if (!traceFile) {
        //     DPRINTF("Failed to create file for write trace stats: %s\n", traceFileName.c_str());
        // } else {
        //     for (const auto &blk : trace_write_blk_seq[fileName]) {
        //         traceFile << blk << std::endl;
        //     }
        // }
#endif
    } else {
        DPRINTF("Warning: Unable to find file name for fd=%d\n", fd);
    }

    // Call the original pwrite function through outerWrapper
    auto ret = outerWrapper("pwrite", fd, Timer::Metric::write,
                            monitorPwrite, unixpwrite, fd, buf, count, offset);

    vLock.writerUnlock(); // Release the lock
    return ret;
}


ssize_t monitorPread64(MonitorFile *file, unsigned int pos, int fd, void *buf, size_t count, off64_t offset) {
    DPRINTF("In monitor pread64: fd=%d, count=%zu, offset=%lld\n", fd, count, (long long)offset);
    auto read_bytes = (ssize_t)file->read(buf, count, pos + offset);
    return read_bytes;
}

ssize_t pread64(int fd, void *buf, size_t count, off64_t offset) {
    vLock.readerLock();
    DPRINTF("Invoking pread64: fd=%d, count=%zu, offset=%lld\n", fd, count, (long long)offset);
    auto ret = outerWrapper("pread64", fd, Timer::Metric::read, 
                            monitorPread64, unixpread64, fd, buf, count, offset);
    vLock.readerUnlock();
    return ret;
}

/* POSIZ I/O */
ssize_t monitorPwrite64(MonitorFile *file, unsigned int pos, int fd, const void *buf, size_t count, off64_t offset) {
    DPRINTF("In monitor pwrite64: fd=%d, count=%zu, offset=%lld\n", fd, count, (long long)offset);
    auto written_bytes = (ssize_t)file->write(buf, count, pos + offset);
    return written_bytes;
}

ssize_t pwrite64(int fd, const void *buf, size_t count, off64_t offset) {
    vLock.writerLock();
    DPRINTF("Invoking pwrite64: fd=%d, count=%zu, offset=%lld\n", fd, count, (long long)offset);
    auto ret = outerWrapper("pwrite64", fd, Timer::Metric::write, 
                            monitorPwrite64, unixpwrite64, fd, buf, count, offset);
    vLock.writerUnlock();
    return ret;
}

