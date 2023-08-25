#ifndef NETWORKCACHE_H
#define NETWORKCACHE_H
#include "Cache.h"
#include "ConnectionPool.h"
#include "PriorityThreadPool.h"

class NetworkCache : public Cache {
  public:
    NetworkCache(std::string cacheName, CacheType type, PriorityThreadPool<std::packaged_task<std::shared_future<Request *>()>> &txPool, PriorityThreadPool<std::packaged_task<Request *()>> &decompPool);
    virtual ~NetworkCache();

    bool writeBlock(Request *req);

    virtual void readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority);

    void setFileCompress(uint32_t index, bool compress);
    void setFileConnectionPool(uint32_t index, ConnectionPool *nmconPool);

    void printStats();

    static Cache *addNewNetworkCache(std::string cacheName, CacheType type, PriorityThreadPool<std::packaged_task<std::shared_future<Request *>()>> &txPool, PriorityThreadPool<std::packaged_task<Request *()>> &decompPool);
    void addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize);

  private:
    virtual void cleanUpBlockData(uint8_t *data);
    Request *decompress(Request *req, char *compBuf, uint32_t compBufSize, uint32_t blkBufSize, uint32_t blk);
    // std::future<Request*> requestBlk(Connection *server, uint32_t blkStart, uint32_t blkEnd, uint32_t fileIndex, uint32_t priority);
    std::future<Request *> requestBlk(Connection *server, Request *req, uint32_t priority, bool &success);
    std::atomic_uint _outstanding;
    std::unordered_map<uint32_t, bool> _compressMap;
    std::unordered_map<uint32_t, ConnectionPool *> _conPoolMap;
    PriorityThreadPool<std::packaged_task<std::shared_future<Request *>()>> &_transferPool;
    PriorityThreadPool<std::packaged_task<Request *()>> &_decompPool;
    ReaderWriterLock *_lock;
};

#endif /* NETWORKCACHE_H */
