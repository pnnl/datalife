#ifndef LOCALFILECACHE_H
#define LOCALFILECACHE_H
#include "Cache.h"

#define LOCALFILECACHENAME "localfilecache"

class LocalFileCache : public Cache {
  public:
    LocalFileCache(std::string cacheName, CacheType type);
    virtual ~LocalFileCache();

    bool writeBlock(Request *req);

    virtual void readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority);
    void printStats();

    static Cache *addNewLocalFileCache(std::string cacheName, CacheType type);
    void addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize);
    void removeFile(uint32_t index);
    

  private:
    virtual void cleanUpBlockData(uint8_t *data);
    uint8_t *getBlockData(std::ifstream *file, uint32_t blockIndex, uint64_t blockSize,uint64_t fileSize);
    std::unordered_map<uint32_t, std::pair<std::ifstream *, ReaderWriterLock *>> _fstreamMap;
    ReaderWriterLock *_lock;
};

#endif /* LOCALFILECACHE_H */
