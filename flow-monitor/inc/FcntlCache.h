#ifndef FcntlCache_H
#define FcntlCache_H
#include "FcntlReaderWriterLock.h"
#include "UnboundedCache.h"
#include <fcntl.h>

#define FCNTLCACHENAME "fcntllock"

class FcntlCache : public UnboundedCache {
  public:
    FcntlCache(std::string cacheName, CacheType type, uint64_t blockSize, std::string cachePath);
    virtual ~FcntlCache();

    virtual void addFile(unsigned int index, std::string filename, uint64_t blockSize, uint64_t fileSize);
    static Cache *addNewFcntlCache(std::string fileName, CacheType type, uint64_t blockSize, std::string cachePath);

  private:
    virtual bool blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs = false);
    virtual bool blockWritable(unsigned int index, unsigned int fileIndex, bool checkFs = false);
    virtual uint8_t *getBlockData(unsigned int blockIndex, unsigned int fileIndex);
    virtual void setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size, unsigned int fileIndex);
    virtual bool blockSet(unsigned int index, unsigned int fileIndex, uint8_t byte);
    virtual bool blockReserve(unsigned int index, unsigned int fileIndex);
    virtual void cleanUpBlockData(uint8_t *data);

    void readFromFile(int fd, uint64_t size, uint8_t *buff);
    void writeToFile(int fd, uint64_t size, uint8_t *buff);

    int do_mkdir(const char *path, mode_t mode);
    int mkpath(const char *path, mode_t mode);

    unixopen_t _open;
    unixclose_t _close;
    unixread_t _read;
    unixwrite_t _write;
    unixlseek_t _lseek;
    unixfsync_t _fsync;
    unixxstat_t _stat;

    std::unordered_map<uint64_t, int> _fdLock;
    std::unordered_map<uint64_t, int> _fdData;
    std::unordered_map<uint64_t, std::atomic<uint32_t> *> _blkCnts;

    FcntlReaderWriterLock _fcntlLock;
    ReaderWriterLock _fdMutex;

    std::string _cachePath;
    std::atomic<uint32_t> _tcnt;
};

#endif /* FcntlCache_H */
