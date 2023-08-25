#ifndef BOUNDEDCACHE_H
#define BOUNDEDCACHE_H
#include "Cache.h"
#include "Loggable.h"
#include "ReaderWriterLock.h"
#include "Trackable.h"
#include "UnixIO.h"
#include <future>
#include <memory>

#define BLK_EMPTY 0
#define BLK_PRE 1
#define BLK_RES 2
#define BLK_WR 3
#define BLK_AVAIL 4

const uint64_t MAX_CACHE_NAME_LEN = 30;

struct dummy {};
template <class Lock>
class BoundedCache : public Cache {
  public:
    BoundedCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity);
    virtual ~BoundedCache();

    virtual bool writeBlock(Request *req);
    virtual void readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority);
    virtual void addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize);

    //TODO: merge/reimplement from old cache structure...
    virtual void cleanReservation();

  protected:
    struct BlockEntry {
        uint32_t fileIndex;
        uint32_t blockIndex;
        uint32_t timeStamp;
        uint32_t status;
        uint32_t prefetched;
        // char origCache[32];
        std::atomic<CacheType> origCache; 
        // this member is atomic because blockAvailable calls are not protected by locks
        // we know that when calling blockAvailable the actual data cannot change during the read (due to refernce counting protections)
        // what can change is the meta data associated with the last access of the data (and which cache it originated from)
        // in certain cases, we want to capture the originating cache and return it for performance reasons
        // there is a potential race between when a block is available and when we capture the originating cache
        // having origCache be atomic ensures we always get a valid ID (even though it may not always be 100% acurate)
        // we are willing to accept some small error in attribution of stats
        void init(BoundedCache* c){
          // memset(this,0,sizeof(BlockEntry));
          fileIndex =0;
          blockIndex = 0;
          timeStamp = 0;
          status = 0;
          prefetched= 0;
          std::atomic_init(&origCache,CacheType::empty);
        }
        BlockEntry& operator= (const BlockEntry &entry){
          if (this == &entry){
            return *this;
          }
          fileIndex = entry.fileIndex;
          blockIndex = entry.blockIndex;
          timeStamp = entry.timeStamp;
          status = entry.status;
          prefetched= entry.prefetched;
          origCache.store(entry.origCache.load());
          return *this;
        }
        // std::atomic<uint32_t> activeCnt;
    };

    virtual bool blockReserve(uint32_t index, uint32_t fileIndex, bool &found, int &reservedIndex, Request* req, bool prefetch = false);

    // virtual int getBlockIndex(uint32_t index, uint32_t fileIndex);
    virtual int getBlockIndex(uint32_t index, uint32_t fileIndex, BlockEntry *entry = NULL);
    virtual int oldestBlockIndex(uint32_t index, uint32_t fileIndex, bool &found, Request* req);
    virtual uint32_t getBinIndex(uint32_t index, uint32_t fileIndex);
    virtual uint32_t getBinOffset(uint32_t index, uint32_t fileIndex);

    //TODO: eventually when we create a flag for stat keeping we probably dont need to store the cacheName...
    virtual void blockSet(uint32_t index, uint32_t fileIndex, uint32_t blockIndex, uint8_t byte,  CacheType type, int32_t prefetch) = 0;

    virtual bool blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs = false, uint32_t cnt = 0, CacheType *origCache = NULL) = 0;

    // virtual char *blockMiss(uint32_t index, uint64_t &size, uint32_t fileIndex, std::unordered_map<uint64_t,std::future<std::future<Request*>>> &reads);
    virtual uint8_t *getBlockData(uint32_t blockIndex) = 0;
    virtual void setBlockData(uint8_t *data, uint32_t blockIndex, uint64_t size) = 0;
    virtual void readBlockEntry(uint32_t blockIndex, BlockEntry *entry) = 0;
    virtual std::string blockEntryStr(uint32_t blockIndex);
    virtual void writeBlockEntry(uint32_t blockIndex, BlockEntry *entry) = 0;
    virtual void readBin(uint32_t binIndex, BlockEntry *entries) = 0;
    virtual std::vector<std::shared_ptr<BlockEntry>> readBin(uint32_t binIndex) = 0;
    virtual int incBlkCnt(uint32_t blk, Request* req) = 0;
    virtual int decBlkCnt(uint32_t blk, Request* req) = 0;
    virtual bool anyUsers(uint32_t blk, Request* req) = 0;
    // virtual bool anyUsers(BlockEntry * entry, Request* req) = 0;

    virtual void getCompareBlkEntry(uint32_t index, uint32_t fileIndex, BlockEntry *entry);

    virtual std::shared_ptr<BlockEntry> getCompareBlkEntry(uint32_t index, uint32_t fileIndex);

    virtual bool sameBlk(BlockEntry *blk1, BlockEntry *blk2);

    uint64_t _cacheSize;
    uint64_t _blockSize;
    uint32_t _associativity;
    uint32_t _numBlocks;
    uint32_t _numBins;
    std::atomic<uint64_t> _collisions;
    std::atomic<uint64_t> _prefetchCollisions;
    std::atomic_uint _outstanding;

    std::unordered_map<uint64_t, uint64_t> _blkMap;

    // T *_lock;
    Lock *_binLock;
    ReaderWriterLock *_localLock;
};

#endif /* BOUNDEDCACHE_H */
