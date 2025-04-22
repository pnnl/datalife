#include "Config.h"
#include "ConnectionPool.h"
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <linux/limits.h>
#include <mutex>
#include <sstream>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fnmatch.h>
#include <unordered_map>
#include <map>
#include <atomic>
#include <string>
#include <vector>
#include <unordered_set>
//#include "ErrorTester.h"
//#include "InputFile.h"
//#include "OutputFile.h"
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


#define ADD_THROW __THROW

#define DPRINTF(...)
// #ifdef LIBDEBUG
// #define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
// #else
// #define DPRINTF(...)
// #endif
// #define MYPRINTF(...) fprintf(stderr, __VA_ARGS__)

#define TRACKFILECHANGES 1

// std::vector<std::string> patterns = {
//     "*.fits", "*.vcf", "*.lht", "*.fna",
//     "*.*.bt2", "*.fastq", "*.fasta.amb", "*.fasta.sa", "*.fasta.bwt",
//     "*.fasta.pac", "*.fasta.ann", "*.fasta", "*.stf",
//     "*.out", "*.dot", "*.gz", "*.tar.gz", "*.dcd", "*.pt", "*.h5", "*.nc", 
//     "SAS", "EAS", "GBR", "AMR", "AFR", "EUR", "ALL",
//     "*.datalifetest",
//     //"*.txt", //"*.pdb",
// };


/* Functions to parse input file extension strings*/
// Helper function to trim whitespace from both ends of a string
std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r";
    const auto start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    const auto end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

// Split function using stringstream
std::vector<std::string> split_patterns(const std::string& input, char delimiter = ',') {
    std::vector<std::string> patterns;
    std::stringstream ss(input);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        std::string trimmed = trim(token);
        if (!trimmed.empty()) {
            patterns.push_back(trimmed);
        }
    }
    // Always include default pattern
    patterns.push_back("*.datalife");
    
    return patterns;
}

std::vector<std::string> patterns = split_patterns(Config::passin_patterns);

static Timer* timer;

std::once_flag log_flag;
bool init = false;
ReaderWriterLock vLock;

std::unordered_set<std::string>* track_files = NULL; //this is a pointer cause we access in ((attribute)) constructor and initialization isnt guaranteed
static std::unordered_set<int> track_fd;
static std::unordered_set<FILE *> track_fp;
static std::unordered_set<int> ignore_fd;
static std::unordered_set<FILE *> ignore_fp;

// // The following map stores the filename, as well as the file descriptors, mode. We 
// // Can extend this easily to save more info as part of the tuple. 
// std::map<std::string, std::multimap<int, std::tuple<int>>> file_info;

// std::map<int, std::string> file_info; // to reverse-lookup filename from fd
// The following map stores the block information/ access freqency for each file
std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_r_stat;
std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_r_stat_size;
std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_w_stat;
std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_w_stat_size;
//std::map<std::string, std::map<int, std::tuple<std::atomic<int64_t>, std::atomic<int64_t> > > > track_file_blk_w_stat;

// For tracing
std::map<std::string, std::vector<int> > trace_read_blk_seq;
std::map<std::string, std::vector<int> > trace_write_blk_seq;

unixopen_t unixopen = NULL;
unixopen_t unixopen64 = NULL;
unixopenat_t unixopenat = NULL;
unixclose_t unixclose = NULL;
unixread_t unixread = NULL;
unixwrite_t unixwrite = NULL;
unixlseek_t unixlseek = NULL;
unixlseek64_t unixlseek64 = NULL;
// unixlstat_t unixlstat = NULL;
unixfstat_t unixfstat = NULL;
unixxstat_t unixxstat = NULL;
unixxstat64_t unixxstat64 = NULL;
unixxstat_t unixlxstat = NULL;
unixxstat64_t unixlxstat64 = NULL;
unixfsync_t unixfsync = NULL;
unixfopen_t unixfopen = NULL;
unixfopen_t unixfopen64 = NULL;
unixfclose_t unixfclose = NULL;
unixfread_t unixfread = NULL;
unixfwrite_t unixfwrite = NULL;
unixftell_t unixftell = NULL;
unixfseek_t unixfseek = NULL;
unixrewind_t unixrewind = NULL;
unixfgetc_t unixfgetc = NULL;
unixfgets_t unixfgets = NULL;
unixfputc_t unixfputc = NULL;
unixfputs_t unixfputs = NULL;
unixflockfile_t unixflockfile = NULL;
unixftrylockfile_t unixftrylockfile = NULL;
unixfunlockfile_t unixfunlockfile = NULL;
unixfflush_t unixfflush = NULL;
unixfeof_t unixfeof = NULL;
unixreadv_t unixreadv = NULL;
unixwritev_t unixwritev = NULL;
unixexit_t unixexit = NULL;
unix_exit_t unix_exit = NULL;
unix_Exit_t unix_Exit = NULL;
unix_exit_group_t unix_exit_group = NULL;
unix_vfprintf_t unix_vfprintf = NULL;
mmap_t unixmmap = NULL;
// For POSIX
pread_t unixpread = NULL;
pwrite_t unixpwrite = NULL;
pread64_t unixpread64 = NULL;
pwrite64_t unixpwrite64 = NULL;
std::unordered_map<int, std::string> fdToFileMap;
std::mutex fdToFileMapMutex;


bool write_printf = false;
bool open_printf = false;

int removeStr(char *s, const char *r);

/*Templating*************************************************************************************************/

inline bool checkMeta(const char *pathname, std::string &path, std::string &file, MonitorFile::Type &type) {


  DPRINTF("Checkmeta calling open on file %s\n", pathname);
    int fd = (*unixopen)(pathname, O_RDONLY);

    if(fd >= 0)
    {
        const std::string monitorVersion("MONITOR0.1");
        //std::string types[3] = {"input", "output", "local"};
        //MonitorFile::Type tokType[3] = {MonitorFile::Input, MonitorFile::Output, MonitorFile::Local};
        std::string types = "TrackLocal";
	MonitorFile::Type tokType = MonitorFile::TrackLocal;
	int bufferSize = (monitorVersion.length() + 13); //need space for monitorVersion + \n + type= + (the type) + \0
        char *meta = new char[bufferSize+1];

        int ret = (*unixread)(fd, (void *)meta, bufferSize);
	// MYPRINTF("Will be calling close on file %s\n", pathname);
        (*unixclose)(fd);
        if (ret <= 0) {
            delete[] meta;
            return false;
        }
        meta[bufferSize] = '\0';

        std::stringstream ss(meta);
        std::string curLine;

        std::getline(ss, curLine);
        if(curLine.compare(0, monitorVersion.length(), monitorVersion) != 0) {
            delete[] meta;
	    return false;
        }

        std::getline(ss, curLine);
        if(curLine.compare(0, 5, "type=") != 0) {
            delete[] meta;
            return false;
        }

        std::string typeStr = curLine.substr(5, curLine.length() - 5);
        //for(int i = 0; i < 3; i++) {
            if(typeStr.compare(types) == 0) {
                path = pathname;
                file = pathname;
                type = tokType;
                // DPRINTF("Path: %s File: %s\n", path.c_str(), file.c_str());
                delete[] meta;
                return true;
            }
        //}
        delete[] meta;
    }
    return false;
}

inline bool trackFile(int fd) { return init ? track_fd.count(fd) : false; }
inline bool trackFile(FILE *fp) { return init ? track_fp.count(fp) : false; }
inline bool trackFile(const char *name) { return init ? track_files->count(name) : false; }

inline bool ignoreFile(uint64_t fd) { return init ? ignore_fd.count(fd) : false; }
inline bool ignoreFile(FILE *fp) { return init ? ignore_fp.count(fp) : false; }
inline bool ignoreFile(std::string pathname) {
    if (init) {
        if (pathname.find(Config::filelockCacheFilePath) != std::string::npos) {
            return true;
        }
        if (pathname.find(Config::fileCacheFilePath) != std::string::npos) {
            return true;
        }
        if (pathname.find(Config::burstBufferCacheFilePath) != std::string::npos) {
            return true;
        }
    }
    return false;
}

template <typename T>
inline void removeFileStream(T posixFun, FILE *fp) {}
inline void removeFileStream(unixfclose_t posixFun, FILE *fp) {
    if (posixFun == unixfclose)
        MonitorFileStream::removeStream(fp);
}

template <typename Func, typename FuncLocal, typename... Args>
inline auto innerWrapper(int fd, bool &isMonitorFile, Func monitorFun, FuncLocal localFun, Args... args) {
    DPRINTF("[MONITOR] in innerwrapper fd: %ld\n", fd);

    MonitorFile *file = NULL;
    unsigned int fp = 0;

    DPRINTF("[MONITOR] in innerwrapper 3 init val: %d , fd val %d \n", init, fd);

    
    if (init && MonitorFileDescriptor::lookupMonitorFileDescriptor(fd, file, fp)) {
      DPRINTF("Found a file with fd %d\n", fd);
      isMonitorFile = true;
      DPRINTF("calling Monitor function\n");	
      return monitorFun(file, fp, args...);
    }
    // else if (not internal write) {
    // do track
    
    //}
    DPRINTF("[MONITOR] in innerwrapper 3 for write calling localfun\n");

    return localFun(args...);
}

template <typename Func, typename FuncLocal, typename... Args>
inline auto innerWrapper(FILE *fp, bool &isMonitorFile, Func monitorFun, FuncLocal localFun, Args... args) {
  // if (write_printf == true) {
  //   DPRINTF("[MONITOR] in innerwrapper 2 for write\n");
  // }
  DPRINTF("[MONITOR] in innerwrapper fp: %ld\n", fp);

  if (init) {
        ReaderWriterLock *lock = NULL;
        int fd = MonitorFileStream::lookupStream(fp, lock);
        if (fd != -1) {
            isMonitorFile = true;
            lock->writerLock();
            MonitorFile *file = NULL;
            unsigned int pos = 0;
            MonitorFileDescriptor::lookupMonitorFileDescriptor(fd, file, pos);
	    // if (write_printf == true) {
	    //   DPRINTF("[MONITOR] in innerwrapper 2 for write calling Monitorfun\n");
	    // }
            auto ret = monitorFun(file, pos, fd, args...);
            lock->writerUnlock();
            removeFileStream(localFun, fp);
            return ret;
        }
    }
  return localFun(args...);
}

template <typename Func, typename FuncPosix, typename... Args>
inline auto innerWrapper(const char *pathname, bool &isMonitorFile, Func monitorFun, FuncPosix posixFun, Args... args) {
  std::string path;
  std::string file;
  MonitorFile::Type type;
  
  std::string test_tty(pathname);
  if(test_tty.find("tty") != std::string::npos) {
    return posixFun(args...);
  }

  for (auto pattern: patterns) {
    auto ret_val = fnmatch(pattern.c_str(), pathname, 0);
    if (ret_val == 0) {
        DPRINTF("PATTERN: %s PATHNAME: %s \n", pattern.c_str(), pathname);
        isMonitorFile = true;
        std::string filename(pathname);
        type = MonitorFile::TrackLocal;
        file = filename;
        path = filename;
        DPRINTF("Calling HDF/FITS open: \n");
        return monitorFun(file, path, type, args...);
    }
  }

  if (init && checkMeta(pathname, path, file, type)) {
    isMonitorFile = true;
    // DPRINTF("monitorfun With file %s\n", pathname);
    return monitorFun(file, path, type, args...);
  }
  // DPRINTF("[MONITOR] in innerwrapper calling posix\n");
  

  return posixFun(args...);
}

template <typename T1, typename T2>
inline void addToSet(std::unordered_set<int> &set, T1 value, T2 posixFun) {}
inline void addToSet(std::unordered_set<int> &set, int value, unixopen_t posixFun) {
    if (posixFun == unixopen || posixFun == unixopen64)
        set.emplace(value);
}
inline void addToSet(std::unordered_set<FILE *> &set, FILE *value, unixfopen_t posixFun) {
    if (posixFun == unixfopen)
        set.emplace(value);
}

template <typename T1, typename T2>
inline void removeFromSet(std::unordered_set<int> &set, T1 value, T2 posixFun) {}
inline void removeFromSet(std::unordered_set<int> &set, int value, unixclose_t posixFun) {
    if (posixFun == unixclose)
        set.erase(value);
}
inline void removeFromSet(std::unordered_set<FILE *> &set, FILE *value, unixfclose_t posixFun) {
    if (posixFun == unixfclose)
        set.erase(value);
}

template <typename FileId, typename Func, typename FuncPosix, typename... Args>
auto outerWrapper(const char *name, FileId fileId, Timer::Metric metric, Func monitorFun, FuncPosix posixFun, Args... args) {
  // DPRINTF("command %s\n", name);

  if (!init) {
      posixFun = (FuncPosix)dlsym(RTLD_NEXT, name);
      return posixFun(args...);
    }

    timer->start();

    //Check if this is a special file to track (from environment variable)
    bool track = trackFile(fileId);

    //Check for files internal to monitor
    bool ignore = ignoreFile(fileId);

    //Check if a monitor meta-file
    bool isMonitorFile = false;

    auto retValue = innerWrapper(fileId, isMonitorFile, monitorFun, posixFun, args...);
    if (ignore) {
        //Maintain the ignore_fd set
        addToSet(ignore_fd, retValue, posixFun);
        removeFromSet(ignore_fd, retValue, posixFun);
        timer->end(Timer::MetricType::local, Timer::Metric::dummy); //to offset the call to start()
    }
    else { //End Timers!
        if (track) {
            //Maintain the track_fd set
            addToSet(track_fd, retValue, posixFun);
            removeFromSet(track_fd, retValue, posixFun);
            if (std::string("read").compare(std::string(name)) == 0 ||
            std::string("write").compare(std::string(name)) == 0){
                ssize_t ret = *reinterpret_cast<ssize_t*> (&retValue);
                if (ret != -1) {
                    timer->addAmt(Timer::MetricType::local, metric, ret);
                }
            }
            timer->end(Timer::MetricType::local, metric);
        }
        else if (isMonitorFile){
            timer->end(Timer::MetricType::monitor, metric);
        }
        else{
            if (std::string("read").compare(std::string(name)) == 0 ||
            std::string("write").compare(std::string(name)) == 0){
                ssize_t ret = *reinterpret_cast<ssize_t*> (&retValue);
                if (ret != -1) {
                    timer->addAmt(Timer::MetricType::system, metric, ret);
                }
            }
            timer->end(Timer::MetricType::system, metric);
        }
    }
    return retValue;
}

/*Posix******************************************************************************************************/

int monitorOpen(std::string name, std::string metaName, MonitorFile::Type type, const char *pathname, int flags, int mode);
int open(const char *pathname, int flags, ...);
int open64(const char *pathname, int flags, ...);
int openat(int dirfd, const char *pathname, int flags, ...);

int monitorClose(MonitorFile *file, unsigned int fp, int fd);
int close(int fd);

ssize_t monitorRead(MonitorFile *file, unsigned int fp, int fd, void *buf, size_t count);
ssize_t read(int fd, void *buf, size_t count);

ssize_t monitorWrite(MonitorFile *file, unsigned int fp, int fd, const void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

template <typename T>
T monitorLseek(MonitorFile *file, unsigned int fp, int fd, T offset, int whence);
off_t lseek(int fd, off_t offset, int whence) ADD_THROW;
off64_t lseek64(int fd, off64_t offset, int whence) ADD_THROW;

int innerStat(int version, const char *filename, struct stat *buf);
int innerStat(int version, const char *filename, struct stat64 *buf);
template <typename T>
int monitorStat(std::string name, std::string metaName, MonitorFile::Type type, int version, const char *filename, T *buf);
int __xstat(int version, const char *filename, struct stat *buf) ADD_THROW;
int __xstat64(int version, const char *filename, struct stat64 *buf) ADD_THROW;
int __lxstat(int version, const char *filename, struct stat *buf) ADD_THROW;
int __lxstat64(int version, const char *filename, struct stat64 *buf) ADD_THROW;

int monitorFsync(MonitorFile *file, unsigned int fp, int fd);
int fsync(int fd);

template <typename Func, typename FuncLocal>
ssize_t monitorVector(const char *name, Timer::Metric metric, Func monitorFun, FuncLocal localFun, int fd, const struct iovec *iov, int iovcnt);
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
/*Streaming**************************************************************************************************/

FILE *monitorFopen(std::string name, std::string metaName, MonitorFile::Type type, const char *__restrict fileName, const char *__restrict modes);
FILE *fopen(const char *__restrict fileName, const char *__restrict modes);
FILE *fopen64(const char *__restrict fileName, const char *__restrict modes);

int monitorFclose(MonitorFile *file, unsigned int pos, int fd, FILE *fp);
int fclose(FILE *fp);

size_t monitorFread(MonitorFile *file, unsigned int pos, int fd, void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp);
size_t fread(void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp);

size_t monitorFwrite(MonitorFile *file, unsigned int pos, int fd, const void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp);
size_t fwrite(const void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp);

long int monitorFtell(MonitorFile *file, unsigned int pos, int fd, FILE *fp);
long int ftell(FILE *fp);

int monitorFseek(MonitorFile *file, unsigned int pos, int fd, FILE *fp, long int off, int whence);
int fseek(FILE *fp, long int off, int whence);

int monitorFgetc(MonitorFile *file, unsigned int pos, int fd, FILE *fp);
int fgetc(FILE *fp);

char *monitorFgets(MonitorFile *file, unsigned int pos, int fd, char *__restrict s, int n, FILE *__restrict fp);
char *fgets(char *__restrict s, int n, FILE *__restrict fp);

int monitorFputc(MonitorFile *file, unsigned int pos, int fd, int c, FILE *fp);
int fputc(int c, FILE *fp);

int monitorFputs(MonitorFile *file, unsigned int pos, int fd, const char *__restrict s, FILE *__restrict fp);
int fputs(const char *__restrict s, FILE *__restrict fp);

int monitorFeof(MonitorFile *file, unsigned int pos, int fd, FILE *fp);
int feof(FILE *fp) ADD_THROW;

off_t monitorRewind(MonitorFile *file, unsigned int pos, int fd, int fd2, off_t offset, int whence);
void rewind(FILE *fp);


int trackFileOpen(std::string name, std::string metaName, MonitorFile::Type type, const char *pathname, int flags, int mode);
int monitorOpenat(std::string name, std::string metaName, MonitorFile::Type type, 
		int dirfd, const char *pathname, int flags, int mode);
int trackFileOpenat(std::string name, std::string metaName, MonitorFile::Type type, 
		    int dirfd, const char *pathname, int flags, int mode);
