#ifndef URLFILECACHE_H
#define URLFILECACHE_H
#include "Cache.h"
#include "LocalFileCache.h"

#define URLFILECACHENAME "urlfilecache"

class UrlFileCache : public LocalFileCache {
  public:
    UrlFileCache(std::string cacheName, CacheType type);
    virtual ~UrlFileCache();
    void addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize);
    void removeFile(uint32_t index);
    static Cache * addNewUrlFileCache(std::string cacheName, CacheType type);
  private:
    virtual void cleanUpBlockData(uint8_t *data);
    ReaderWriterLock *_lock;
    std::unordered_map<uint32_t, std::string> _urlMap;
};

#endif /* URLFILECACHE_H */
