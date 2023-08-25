#ifndef UnboundedCache_H
#define UnboundedCache_H
#include "Cache.h"
#include "Loggable.h"
#include "ReaderWriterLock.h"
#include "Trackable.h"
#include "UnixIO.h"
#include <future>

#define UBC_BLK_EMPTY 0
#define UBC_BLK_PRE 1
#define UBC_BLK_RES 2
#define UBC_BLK_WR 3
#define UBC_BLK_AVAIL 4

class UnboundedCache : public Cache {
  public:
    UnboundedCache(std::string cacheName, CacheType type, uint64_t blockSize);
    virtual ~UnboundedCache();

    virtual bool writeBlock(Request* req);

    virtual void readBlock(Request* req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request*>>> &reads, uint64_t priority);

    virtual void addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize);

  protected:
    virtual bool blockSet(uint32_t index, uint32_t fileIndex, uint8_t byte) = 0;
    virtual bool blockReserve(unsigned int index, unsigned int fileIndex) = 0;
    // virtual bool blockReserve(uint32_t index, uint32_t fileIndex, bool prefetch = false)=0;
    virtual void cleanReservation();
    virtual void cleanUpBlockData(uint8_t *data) = 0;
    virtual bool blockAvailable(uint32_t index, uint32_t fileIndex, bool arg = false) = 0;
    virtual bool blockWritable(uint32_t index, uint32_t fileIndex, bool arg = false) = 0;
    virtual uint8_t *getBlockData(uint32_t blockIndex, uint32_t fileIndex) = 0;
    virtual void setBlockData(uint8_t *data, uint32_t blockIndex, uint64_t size, uint32_t fileIndex) = 0;

    uint64_t _blockSize;
    std::atomic_uint _outstanding;
    std::unordered_map<uint32_t, std::atomic<uint8_t> *> _blkIndex;
    ReaderWriterLock *_lock;
};

#endif /* UnboundedCache_H */
