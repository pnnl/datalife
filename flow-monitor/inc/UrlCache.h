#ifndef URLCACHE_H
#define URLCACHE_H

#ifdef USE_CURL

#include "UnboundedCache.h"
#include "UrlDownload.h"
#include <atomic>
#include <future>
#include <map>
#include <unordered_map>
#include <unordered_set>

#define URLCACHENAME "urlcache"

class UrlCache : public UnboundedCache {
  public:
    UrlCache(std::string cacheName, CacheType type, uint64_t blockSize);
    ~UrlCache();

    virtual void addFile(unsigned int index, std::string filename, uint64_t blockSize, uint64_t fileSize);
    void removeFile(uint32_t index);
    static Cache *addNewUrlCache(std::string cacheName, CacheType type, uint64_t blockSize);

  private:
    virtual bool blockAvailable(unsigned int index, unsigned int fileIndex, bool arg = false);
    virtual bool blockWritable(unsigned int index, unsigned int fileIndex, bool arg = false);
    virtual uint8_t *getBlockData(unsigned int blockIndex, unsigned int fileIndex);
    
    virtual void setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size, unsigned int fileIndex);
    virtual bool blockSet(unsigned int index, unsigned int fileIndex,  uint8_t byte);
    virtual bool blockReserve(unsigned int index, unsigned int fileIndex);
    
    virtual void cleanUpBlockData(uint8_t *data);
    std::unordered_map<uint32_t, UrlDownload*> _urlMap;
};

#endif
#endif /* URLCACHE_H */
