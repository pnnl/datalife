#include <atomic>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <fnmatch.h>

#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
//#include "InputFile.h"
#include "MonitorFile.h"
//#include "OutputFile.h"
//#include "LocalFile.h"
#include "TrackFile.h"
#include "Timer.h"
#include "UnixIO.h"
#include "MonitorPathPolicy.h"

//#define TIMEON(...) __VA_ARGS__
#define TIMEON(...)
// #define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)
// Marked as the library's own I/O, like DPRINTF in TrackFile.cpp.
#define MYDPRINTF(...) do { ScopedMonitorInternalIO dprintf_io; fprintf(stderr, __VA_ARGS__); } while (0)
#define TRACKFILECHANGES 1

// Helper function to extract basename from a pathname without modifying the input
inline const char* get_basename(const char* pathname) {
    if (!pathname) return pathname;
    const char* base = strrchr(pathname, '/');
    return base ? base + 1 : pathname;
}

FilePositions::FilePositions() : _size(0) {
    for (auto &chunk : _chunks) {
        chunk.store(nullptr, std::memory_order_relaxed);
    }
}

FilePositions::~FilePositions() {
    for (auto &chunk : _chunks) {
        delete[] chunk.load(std::memory_order_relaxed);
    }
}

// Chunk k starts at index 64 * (2^k - 1).
uint32_t FilePositions::chunkOf(uint32_t index) {
    return 31 - __builtin_clz(index / 64 + 1);
}

uint64_t FilePositions::chunkStart(uint32_t chunk) {
    return 64 * ((uint64_t(1) << chunk) - 1);
}

uint64_t FilePositions::chunkSize(uint32_t chunk) {
    return uint64_t(64) << chunk;
}

std::atomic<uint64_t> &FilePositions::operator[](uint32_t index) {
    uint32_t chunk = chunkOf(index);
    return _chunks[chunk].load(std::memory_order_acquire)[index - chunkStart(chunk)];
}

uint32_t FilePositions::add() {
    uint32_t index = _size;
    uint32_t chunk = chunkOf(index);
    if (!_chunks[chunk].load(std::memory_order_relaxed)) {
        // Value-initialized, so every position starts at 0; indexes are never reused.
        _chunks[chunk].store(new std::atomic<uint64_t>[chunkSize(chunk)](), std::memory_order_release);
    }
    _size = index + 1;
    return index;
}

static bool isRegularFile(int fd) {
    struct stat st;
    return fd >= 0 && fstat(fd, &st) == 0 && S_ISREG(st.st_mode);
}

MonitorFile::MonitorFile(MonitorFile::Type type, std::string name, std::string metaName, int fd) : 
    Loggable(Config::MonitorFileLog, "MonitorFile"),
    _type(type),
    _name(name),
    _metaName(metaName),
    _traced(false),
    _eof(false),
    _compress(false),
    _prefetch(false),
    _blkSize(1),
    _initMetaTime(0),
    _active(false),
    _fd(fd),
    _regularFile(isRegularFile(fd))
     {

#ifdef TRACKFILECHANGES

    bool matched = matchesPatterns(name);


    if (matched) {
        DPRINTF("File %s matched pattern. Reading metadata...\n", name.c_str());
        readMetaInfo();
    } else {
        DPRINTF("File %s did not match any patterns.\n", name.c_str());
    }

#endif

    newFilePosIndex();
}

MonitorFile::~MonitorFile() {
}

//meta file format:
//MONITOR0.1
//type=         (type can be input, output, or local)
//[server]      (the info under [server] can be in any order, but the host, port, and file parts must be provided, multiple servers can be given by using multiple [server] tags)
//host=
//port=
//file=
bool MonitorFile::readMetaInfo() {
    MYDPRINTF("Trying to read meta info for file %s\n", _name.c_str());
    TIMEON(uint64_t t1 = Timer::getCurrentTime());
    auto start = Timer::getCurrentTime();
    unixread_t unixRead = (unixread_t)dlsym(RTLD_NEXT, "read");
    unixlseek_t unixlseek = (unixlseek_t)dlsym(RTLD_NEXT, "lseek");
   
    if (_fd < 0) {
        log(this) << "ERROR: Failed to open local metafile " << _metaName.c_str() << " : " << strerror(errno) << std::endl;
        return 0;
    }

    int64_t fileSize = (*unixlseek)(_fd, 0L, SEEK_END);
    (*unixlseek)(_fd, 0L, SEEK_SET);
    // Not seekable (pipe, socket, tty): never consume bytes from it.
    if (fileSize < 0) {
        return 0;
    }

    // A meta descriptor starts with the MONITOR0.1 magic line (see checkMeta()).
    // Peek at that header only; a tracked data file fails the test here, so it
    // is never slurped into memory, whatever its size. The application's file
    // position is restored either way.
    static const std::string monitorVersion("MONITOR0.1");
    char header[16] = {0};
    int headerBytes = (*unixRead)(_fd, (void *)header, monitorVersion.length());
    (*unixlseek)(_fd, 0L, SEEK_SET);
    if (headerBytes != (int)monitorVersion.length() ||
        monitorVersion.compare(0, monitorVersion.length(), header, monitorVersion.length()) != 0) {
        return 0;
    }

    // Genuine descriptor: a few hundred bytes of key=value text. The size cap is
    // only a guard on the allocation below (MONITOR_MAX_META_SIZE, default 1 MiB).
    if (fileSize > Config::maxMetaFileSize) {
        log(this) << "ERROR: meta descriptor " << _metaName.c_str() << " exceeds MONITOR_MAX_META_SIZE, ignoring it" << std::endl;
        return 0;
    }
    char *meta = new char[fileSize + 1];
    int ret = (*unixRead)(_fd, (void *)meta, fileSize);

    // Restore file position to beginning so subsequent reads work correctly
    (*unixlseek)(_fd, 0L, SEEK_SET);

    if (ret < 0) {
        // std::cout << "ERROR: Failed to read local metafile: " << strerror(errno) << std::endl;
        // raise(SIGSEGV);

        log(this) << "ERROR: Failed to read local metafile: " << strerror(errno) << std::endl;
        delete[] meta;

        return 0;
    }
    meta[fileSize] = '\0';

    typedef enum {
        DEFAULT,
        SERVER,
        //OPTIONS,
        DONE
    } State;

    uint32_t numServers = 0;
    std::string curLine;
    std::string hostAddr;
    std::string fileName;
    int port;
    std::stringstream ss(meta);

    State state = DEFAULT;
    std::getline(ss, curLine);
    while(state != DONE) {
        switch (state)
        {
        case SERVER:
            //found [server]
            hostAddr = "\0";
            port = 0;
            fileName = "\0";
	    
            while(std::getline(ss, curLine)) {
                if(curLine.compare(0, 5, "host=") == 0) {
                    hostAddr = curLine.substr(5, (curLine.length() - 5));
                    log(this) << "hostaddr: " << hostAddr << std::endl;
                }
                else if(curLine.compare(0, 5, "port=") == 0) {
                    port = atoi(curLine.substr(5, (curLine.length() - 5)).c_str());
                    log(this) << "port: " << port << std::endl;
                }
                else if(curLine.compare(0, 5, "file=") == 0) {
                    fileName = curLine.substr(5, (curLine.length() - 5));
                    _name = fileName;
                    log(this) << "fileName: " << fileName << std::endl;
                }
                else if(curLine.compare(0, 13, "compress=true") == 0) {
                    _compress = true;
                    log(this) << "compress: " << _compress << std::endl;
                }
                else if(curLine.compare(0, 14, "compress=false") == 0) {
                    _compress = false;
                    log(this) << "compress: " << _compress << std::endl;
                }
                else if(curLine.compare(0, 13, "prefetch=true") == 0) {
                    _prefetch = true;
                    log(this) << "prefetch: " << _prefetch << std::endl;
                }
                else if(curLine.compare(0, 14, "prefetch=false") == 0) {
                    _prefetch = false;
                    log(this) << "prefetch: " << _prefetch << std::endl;
                }
                else if(curLine.compare(0, 11, "block_size=") == 0) {
                    _blkSize = atoi(curLine.substr(11, (curLine.length() - 11)).c_str());
                    log(this) << "blkSize: " << _blkSize << std::endl;
                }
                else {
                    break;
                }
            }
            //make sure the host port and file name were given
            if(hostAddr == "\0" || port == 0 || fileName == "\0") {
                log(this) << "0:improperly formatted meta file" << std::endl;
		// std::cout << "0:improperly formatted meta file" << std::endl;
                delete[] meta;
                return 0;
            }
            //after collecting info for a server
            if (_type == MonitorFile::TrackLocal) {
                Connection *connection = Connection::addNewClientConnection(hostAddr, port);
                std::cout << hostAddr << " " << port << " " << connection << std::endl;
                if (connection) {
                    if (ConnectionPool::useCnt->count(connection->addrport()) == 0) {
                        ConnectionPool::useCnt->emplace(connection->addrport(), 0);
                        ConnectionPool::consecCnt->emplace(connection->addrport(), 0);
                    }
                    _connections.push_back(connection);
                    numServers++;
                }
            }
	    
            state = DEFAULT;
            break;
        default:
            if(curLine.compare(0, 8, "[server]") == 0) {
                state = SERVER;
            }
            else if(!std::getline(ss, curLine)) {
                state = DONE;
            }
            break;
        }
    }
    
    delete[] meta;
    TIMEON(fprintf(stderr, "Meta Time: %lu\n", Timer::getCurrentTime() - t1));
    _initMetaTime = Timer::getCurrentTime()-start;
    return (numServers > 0);
}


MonitorFile::Type MonitorFile::type() {
    return _type;
}

std::string MonitorFile::name() {
    std::lock_guard<SpinLock> name_guard(trackFileStatLock());
    return _name;
}

std::string MonitorFile::metaName() {
    std::lock_guard<SpinLock> name_guard(trackFileStatLock());
    return _metaName;
}

bool MonitorFile::matchesPatterns(const std::string &path) {
    for (const auto &pattern : patterns) {
        if (fnmatch(pattern.c_str(), get_basename(path.c_str()), 0) == 0) {
            return true;
        }
    }
    return false;
}

bool MonitorFile::traced() {
    std::string current;
    {
        std::lock_guard<SpinLock> name_guard(trackFileStatLock());
        if (_traced) {
            return true;
        }
        current = _name;
    }
    return matchesPatterns(current);
}

uint64_t MonitorFile::blkSize() {
    return _blkSize;
}

bool MonitorFile::compress() {
    return _compress;
}

bool MonitorFile::prefetch() {
    return _prefetch;
}

bool MonitorFile::active() {
    return _active.load();
}

bool MonitorFile::eof(uint32_t index) {
    return _eof[index];
}

bool MonitorFile::regularFile() {
    return _regularFile;
}

uint32_t MonitorFile::newFilePosIndex(int fd) {
    // Asked before the position can be used: nothing has been written through
    // the descriptor yet.
    const int flags = fd >= 0 ? ::fcntl(fd, F_GETFL) : -1;
    uint32_t ret;
    std::unique_lock<std::mutex> lock(_fpMutex);
    ret = _filePos.add();
    _shares.add();
    _shares[ret] = ret;
    _streamRecorded.add();
    _appends.add();
    _eof.push_back(false);
    lock.unlock();
    setAppends(ret, flags != -1 && (flags & O_APPEND));
    return ret;
}

uint32_t MonitorFile::newFilePosIndexSharing(uint32_t index) {
    uint32_t ret;
    std::unique_lock<std::mutex> lock(_fpMutex);
    if (!_freeCopies.empty()) {
        ret = _freeCopies.back();
        _freeCopies.pop_back();
        _streamRecorded[ret] = 0; // no stream seen on it
    } else {
        ret = _filePos.add();
        _shares.add();
        _streamRecorded.add();
        _appends.add();
        _eof.push_back(false);
    }
    _shares[ret] = _shares[index].load(); // the copy of a copy shares with the first
    return ret;
}

void MonitorFile::releaseFilePosIndex(uint32_t index) {
    if (_shares[index].load() != index) {
        std::unique_lock<std::mutex> lock(_fpMutex);
        _freeCopies.push_back(index);
    }
}

bool MonitorFile::appends(uint32_t index) {
    return _appends[(uint32_t)_shares[index].load()].load() != 0;
}

void MonitorFile::setAppends(uint32_t index, bool appends) {
    _appends[(uint32_t)_shares[index].load()] = appends ? 1 : 0;
}

uint64_t MonitorFile::filePos(uint32_t index) {
    //no lock here... we are assuming the fp index is inbounds...
    return offsetAt(index);
}

void MonitorFile::setFilePos(uint32_t index, uint64_t pos) {
    //no lock here... we are assuming the fp index is inbounds...
    offsetAt(index) = pos;
}

long MonitorFile::streamRecorded(uint32_t index) {
    return (long)_streamRecorded[index].load() - 1;
}

void MonitorFile::setStreamRecorded(uint32_t index, long bytes) {
    _streamRecorded[index] = (uint64_t)(bytes + 1);
}

//fileName is the metafile
MonitorFile *MonitorFile::addNewMonitorFile(MonitorFile::Type type, std::string fileName, std::string metaName, int fd, bool open) {
    // // Print debug information
    // DPRINTF("MonitorFile::addNewMonitorFile called with:\n");
    // DPRINTF("  Type: %d\n", type);
    // DPRINTF("  File Name: %s\n", fileName.c_str());
    // DPRINTF("  Meta Name: %s\n", metaName.c_str());
    // DPRINTF("  File Descriptor: %d\n", fd);
    // DPRINTF("  Open: %s\n", open ? "true" : "false");

    // Print debug information
    // std::cout << "MonitorFile::addNewMonitorFile called with:" << std::endl;
    // std::cout << "  Type: " << static_cast<int>(type) << std::endl;  // Convert enum to int for better readability
    // std::cout << "  File Name: " << fileName << std::endl;
    // std::cout << "  Meta Name: " << metaName << std::endl;
    // std::cout << "  File Descriptor: " << fd << std::endl;
    // std::cout << "  Open: " << (open ? "true" : "false") << std::endl;


   /* if (type == MonitorFile::Input) {
        return Trackable<std::string, MonitorFile *>::AddTrackable(
            metaName, [=]() -> MonitorFile * {
                // std::cout << "new input " <<fileName<<" "<<metaName<<" "<<std::endl;
                MonitorFile *temp = new InputFile(fileName, metaName, fd, open);
                if (open && temp && temp->active() == 0) {
                    delete temp;
                    return NULL;
                }
                return temp;
            });
    }
    else if (type == MonitorFile::Output) {
        bool dontcare;
        return Trackable<std::string, MonitorFile *>::AddTrackable(
            metaName, [=]() -> MonitorFile * {
                // std::cout << "new output " <<fileName<<" "<<metaName<<" "<<std::endl;
                // std::cout << "Create new" << std::endl;
                MonitorFile *temp = new OutputFile(fileName, metaName, fd);
                if (temp) {
                    OutputFile* out = dynamic_cast<OutputFile*>(temp);
                    out->setThreadFileDescriptor(fd);
                    if (temp->active() == 0) {
                    delete temp;
                    return NULL;
                    }
                }
                return temp;
            },
            [=](MonitorFile* monitorFile) -> void {
                OutputFile* out = dynamic_cast<OutputFile*>(monitorFile);
                out->setThreadFileDescriptor(fd);
                //// std::cout << "Reuse old" << std::endl;
                out->addFileDescriptor(fd);
            }, dontcare);
    }
    else if (type == MonitorFile::Local) {
        return Trackable<std::string, MonitorFile *>::AddTrackable(
            fileName, [=]() -> MonitorFile * {
                MonitorFile *temp = new LocalFile(fileName, metaName, fd, open);
                if (open && temp && temp->active() == 0) {
                    delete temp;
                    return NULL;
                }
                return temp;
            });
    } else */ 
     
    if (type == MonitorFile::TrackLocal) {
      DPRINTF("Trackfile going to be added to the Trackable \n");
        return Trackable<std::string, MonitorFile *>::AddTrackable(
            fileName, [=]() -> MonitorFile * {
	      DPRINTF("Filename in lambda %s\n", fileName.c_str());
                MonitorFile *temp = new TrackFile(fileName, fd, open);
                if (open && temp && temp->active() == 0) {
                    delete temp;
		    DPRINTF("Can't add a TrackFile with Filename %s fd %d", 
			    fileName.c_str(), fd);
		    return NULL;
                }
		DPRINTF("Adding (filename,Trackfile) to map\n");
                return temp;
            });
    }  
    return NULL;
}

// fileName is the name the file was added under. A name is never altered to find
// its entry: one that held ".tmp" used to be looked up without it, so closing
// "out.tmp" released the entry of "out", still open, or none at all.
bool MonitorFile::removeMonitorFile(std::string fileName) {
    DPRINTF("Removing Monitorfile %s\n", fileName.c_str());
    return Trackable<std::string, MonitorFile *>::RemoveTrackable(fileName);
}

bool MonitorFile::removeMonitorFile(MonitorFile *file) {
  DPRINTF("Removing Monitorfile %s\n", file->_metaName.c_str());
    // std::cout<<" remove: "<<file->_name<<" "<<file->_metaName<<std::endl;
    // The file's name now, read with the registry locked: a rename on another
    // thread changes it under that lock, and the old name may already be that
    // of another file.
    return !file->dropUser() &&
           Trackable<std::string, MonitorFile *>::RemoveTrackableBy([file]() { return file->_metaName; });
}

bool MonitorFile::renameMonitorFile(const std::string &from, const std::string &to, bool &open) {
    return Trackable<std::string, MonitorFile *>::RekeyTrackable(
        from, to, [&](MonitorFile *file) { file->renameTo(to); }, open);
}

MonitorFile *MonitorFile::lookUpMonitorFile(std::string fileName) {
    return Trackable<std::string, MonitorFile *>::LookupTrackable(fileName);
}