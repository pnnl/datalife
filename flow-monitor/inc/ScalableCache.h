#ifndef SCALABLECACHE_H
#define SCALABLECACHE_H
#include "Cache.h"
#include "Loggable.h"
#include "Trackable.h"
#include "ReaderWriterLock.h"
#include "ScalableMetaData.h"
#include "ScalableAllocator.h"
#include <map>

#define SCALABLECACHENAME "scalable"

class ScalableCache : public Cache {
  public:
    ScalableCache(std::string cacheName, CacheType type, uint64_t blockSize, uint64_t maxCacheSize);
    virtual ~ScalableCache();
    
    static Cache* addScalableCache(std::string cacheName, CacheType type, uint64_t blockSize, uint64_t maxCacheSize);

    virtual bool writeBlock(Request *req);
    virtual void readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority);
    
    virtual void addFile(uint32_t fileIndex, std::string filename, uint64_t blockSize, std::uint64_t fileSize);
    virtual void closeFile(uint32_t fileIndex);

    //JS: Hueristics for replacement
    virtual ScalableMetaData * oldestFile(uint32_t &oldestFileIndex);
    ScalableMetaData * findVictim(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, bool mustSucceed=false);
    ScalableMetaData * randomFile(uint32_t &sourceFileIndex);
    ScalableMetaData * largestFile(uint32_t &largestFileIndex);
    void updateRanks (uint32_t allocateForFileIndex, double& allocateForFileRank); 
    
    //JS: These hueristics are built mainly for scalable cache fallback use.  They walk through all files/blocks
    //looking for blocks.
    uint8_t * findBlockFromCachedUMB(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, uint64_t &sourceBlockIndex, double allocateForFileRank);
    uint8_t * findBlockFromOldestFile(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, uint64_t &sourceBlockIndex);

    //JS: For tracking
    void trackBlockEviction(uint32_t fileIndex, uint64_t blockIndex);
    void trackPattern(uint32_t fileIndex, std::string pattern);
  
    //JS: This is so we can let others piggyback off our metrics.
    //JS: I guess it is not that bad to just return vectors...
    //https://stackoverflow.com/questions/15704565/efficient-way-to-return-a-stdvector-in-c
    std::vector<std::tuple<uint32_t, double>> getLastUMB(Cache * cache);
    void setUMBCache(Cache * cache);

  protected:
    ReaderWriterLock *_cacheLock;
    std::unordered_map<uint32_t, ScalableMetaData*> _metaMap;
    uint64_t _blockSize;
    uint64_t _numBlocks;
    MonitorAllocator * _allocator;
    
    //JS: This is just temp for checking which files had blocks evicted
    Histogram evictHisto;

    //JS: For Nathan
    std::atomic<uint64_t> access;
    std::atomic<uint64_t> misses;
    uint64_t startTimeStamp;

    //JS: This is to make sure there will exist a block that is not requested
    std::atomic<uint64_t> oustandingBlocksRequested;
    std::atomic<uint64_t> maxOutstandingBlocks;
    std::atomic<uint64_t> maxBlocksInUse;


  private:
    uint8_t * getBlockData(uint32_t fileIndex, uint64_t blockIndex, uint64_t fileOffset);
    uint8_t * getBlockDataOrReserve(uint32_t fileIndex, uint64_t blockIndex, uint64_t fileOffset, bool &reserve, bool &full);
    void setBlock(uint32_t fileIndex, uint64_t blockIndex, uint8_t * data, uint64_t dataSize, bool writeOptional);
    void checkMaxBlockInUse(std::string msg, bool die);
    void checkMaxInFlightRequests(uint64_t index);

    //JS: Metric piggybacking
    ReaderWriterLock * _lastVictimFileIndexLock;
    std::vector<std::tuple<uint32_t, double>> _UMBList;
    std::unordered_map<Cache*, bool> _UMBDirty;

    std::once_flag first_miss;
};

#endif // SCALABLECACHE_H
