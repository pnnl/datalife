#ifndef MonitorFile_H_
#define MonitorFile_H_

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <list>
#include <math.h>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Connection.h"
#include "Loggable.h"
#include "Trackable.h"

class MonitorFile : public Loggable, public Trackable<std::string, MonitorFile *> {
  public:
    enum Type { //Input = 0,
                //Output = 1,
                //Local = 2,
		TrackLocal = 3
    };

    MonitorFile(MonitorFile::Type type, std::string name, std::string metaName, int fd);
    virtual ~MonitorFile();

    virtual void open() = 0;
    virtual void close() = 0;
    virtual uint64_t fileSize() = 0;
  //  virtual uint64_t numBlks() = 0;
  
    virtual ssize_t read(void *buf, size_t count, uint32_t filePosIndex = 0) = 0;
    virtual ssize_t write(const void *buf, size_t count, uint32_t filePosIndex = 0) = 0;
    virtual int vfprintf(unsigned int pos, int count) = 0;

    virtual uint32_t newFilePosIndex();
    virtual uint64_t filePos(uint32_t index);
    virtual void setFilePos(uint32_t index, uint64_t pos);
    virtual off_t seek(off_t offset, int whence, uint32_t index = 0) = 0;

    static MonitorFile *addNewMonitorFile(MonitorFile::Type type, std::string fileName, std::string metaName, int fd, bool open = true);
    static bool removeMonitorFile(std::string fileName);
    static bool removeMonitorFile(MonitorFile *file);
    static MonitorFile *lookUpMonitorFile(std::string fileName);

    MonitorFile::Type type();
    std::string name();
    std::string metaName();
    uint64_t blkSize();
    bool compress();
    bool prefetch();
    virtual bool active();
    bool eof(uint32_t index);
  protected:
    std::string findMetaParam(std::string param, std::string server, bool required);
    bool readMetaInfo();
    

    MonitorFile::Type _type;
    std::string _name;
    std::string _metaName;

    std::mutex _fpMutex;
    std::vector<uint64_t> _filePos;
    std::vector<bool> _eof;

    //Properties from meta data file
    bool _compress;
    uint32_t _prefetch; //Adding the option to prefetch or not a particular file
    uint64_t _blkSize;

    uint64_t _initMetaTime;

    std::atomic_bool _active; //if the connections are up
    std::vector<Connection *> _connections;

  private:
    int _fd;
};

#endif /* MonitorFile_H_ */
