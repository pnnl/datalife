#ifndef FilelockCache_H
#define FilelockCache_H
#include "UnboundedCache.h"
#include <fcntl.h>

#define FILELOCKCACHENAME "filelock"

class FilelockCache : public UnboundedCache {
  public:
    FilelockCache(std::string cacheName, CacheType type, uint64_t blockSize, std::string cachePath);
    virtual ~FilelockCache();

    virtual void addFile(unsigned int index, std::string filename, uint64_t blockSize, uint64_t fileSize);
    static Cache *addNewFilelockCache(std::string fileName, CacheType type, uint64_t blockSize, std::string cachePath);

  private:
    virtual bool blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs = false);
    virtual bool blockWritable(unsigned int index, unsigned int fileIndex, bool checkFs = false);
    virtual uint8_t *getBlockData(unsigned int blockIndex, unsigned int fileIndex);
    virtual void setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size, unsigned int fileIndex);
    virtual bool blockSet(unsigned int index, unsigned int fileIndex,  uint8_t byte);
    virtual bool blockReserve(unsigned int index, unsigned int fileIndex);
    virtual void cleanUpBlockData(uint8_t *data);

    void readFromFile(int fd, uint64_t size, uint8_t *buff);
    void writeToFile(int fd, uint64_t size, uint8_t *buff);

    unixopen_t _open;
    unixclose_t _close;
    unixread_t _read;
    unixwrite_t _write;
    unixlseek_t _lseek;
    unixfsync_t _fsync;
    unixxstat_t _stat;

    std::string _cachePath;
    std::string _filePath;
    std::string _blockPath;
    std::string _reservePath;
    std::string _locksPath;
    std::string _txPath;
};

#endif /* FilelockCache_H */
