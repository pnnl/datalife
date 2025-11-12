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

//#define TIMEON(...) __VA_ARGS__
#define TIMEON(...)
// #define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)
#define MYDPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define TRACKFILECHANGES 1

extern int removeStr(char *s, const char *r);
MonitorFile::MonitorFile(MonitorFile::Type type, std::string name, std::string metaName, int fd) : 
    Loggable(Config::MonitorFileLog, "MonitorFile"),
    _type(type),
    _name(name),
    _metaName(metaName),
    _filePos(0),
    _eof(false),
    _compress(false),
    _prefetch(false),
    _blkSize(1),
    _initMetaTime(0),
    _active(false),
    _fd(fd)
     {

#ifdef TRACKFILECHANGES

    bool matched = true;
    for (const auto& pattern : patterns) {
        DPRINTF("Checking file: %s against pattern: %s\n", name.c_str(), pattern.c_str());
        if (fnmatch(pattern.c_str(), name.c_str(), 0) != 0) {
            matched = false;
            break;
        }
    }

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
    char *meta = new char[fileSize + 1];
    int ret = (*unixRead)(_fd, (void *)meta, fileSize);
    if (ret < 0) {
        // std::cout << "ERROR: Failed to read local metafile: " << strerror(errno) << std::endl;
        raise(SIGSEGV);
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
    return _name;
}

std::string MonitorFile::metaName() {
    return _metaName;
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

uint32_t MonitorFile::newFilePosIndex() {
    uint32_t ret;
    std::unique_lock<std::mutex> lock(_fpMutex);
    ret = _filePos.size();
    _filePos.push_back(0);
    _eof.push_back(false);
    lock.unlock();
    return ret;
}

uint64_t MonitorFile::filePos(uint32_t index) {
    //no lock here... we are assuming the fp index is inbounds...
    return _filePos[index];
}

void MonitorFile::setFilePos(uint32_t index, uint64_t pos) {
    //no lock here... we are assuming the fp index is inbounds...
    _filePos[index] = pos;
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

//fileName is the metaFile
bool MonitorFile::removeMonitorFile(std::string fileName) {
  DPRINTF("Removing Monitorfile %s\n", fileName.c_str());  
  if (strstr(fileName.c_str(), ".tmp") != NULL) {
        char temp[1000];
        strcpy(temp, fileName.c_str());
        removeStr(temp, ".tmp");
        // std::cout<<" remove monitor 1: "<<fileName<<std::endl;
        return Trackable<std::string, MonitorFile *>::RemoveTrackable(temp);
    }
    else {
        // std::cout<<" remove monitor 2: "<<fileName<<std::endl;
        return Trackable<std::string, MonitorFile *>::RemoveTrackable(fileName);
    }
}

bool MonitorFile::removeMonitorFile(MonitorFile *file) {
  DPRINTF("Removing Monitorfile %s\n", file->_metaName.c_str());
    // std::cout<<" remove: "<<file->_name<<" "<<file->_metaName<<std::endl;
    return removeMonitorFile(file->_metaName);
}

MonitorFile *MonitorFile::lookUpMonitorFile(std::string fileName) {
    if (strstr(fileName.c_str(), ".tmp") != NULL) {
        char temp[1000];
        strcpy(temp, fileName.c_str());
        removeStr(temp, ".tmp");
        return Trackable<std::string, MonitorFile *>::LookupTrackable(temp);
    }
    else {
        return Trackable<std::string, MonitorFile *>::LookupTrackable(fileName);
    }
}
