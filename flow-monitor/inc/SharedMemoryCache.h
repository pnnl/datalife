#ifndef SharedMemoryCache_H
#define SharedMemoryCache_H
#include "BoundedCache.h"

#define SHAREDMEMORYCACHENAME "sharedmemory"

class SharedMemoryCache : public BoundedCache<MultiReaderWriterLock> {
  public:

    SharedMemoryCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, Cache * scalableCache);
    ~SharedMemoryCache();

    static Cache *addSharedMemoryCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, Cache * scalableCache);
    
  protected:
    struct MemBlockEntry : BlockEntry {
        std::atomic<uint32_t> activeCnt;
        void init(BoundedCache* c,uint32_t entryId){
          //BlockEntry::init(c,entryId);
          //update
	  BlockEntry::init(c);
	  std::atomic_init(&activeCnt, (uint32_t)0);
        }
    };
    
    virtual void blockSet(BlockEntry* blk,  uint32_t fileIndex, uint32_t blockIndex, uint8_t byte, CacheType type, int32_t prefetch, int activeUpdate,Request* req);
    virtual bool blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs = false, uint32_t cnt = 0, CacheType *origCache = NULL);
   
    virtual void cleanUpBlockData(uint8_t *data);
    virtual uint8_t *getBlockData(unsigned int blockIndex);
    virtual void setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size);
    virtual BlockEntry* getBlockEntry(uint32_t blockIndex, Request* req);
    //virtual std::vector<BlockEntry*> readBin(uint32_t binIndex);
    //update
    std::vector<std::shared_ptr<BlockEntry>> readBin(uint32_t binIndex);

    virtual std::string blockEntryStr(BlockEntry *entry);

    virtual int incBlkCnt(BlockEntry * entry, Request* req);
    virtual int decBlkCnt(BlockEntry * entry, Request* req);
    virtual bool anyUsers(BlockEntry * entry, Request* req);

    //JS: Metric piggybacking
    virtual double getLastUMB(uint32_t fileIndex);
    virtual void setLastUMB(std::vector<std::tuple<uint32_t, double>> &UMBList);

  private:
    MemBlockEntry *_blkIndex;
    uint8_t *_blocks;

    //JS: This is for scalable cache metrics
    ReaderWriterLock *_UMBLock;
    double * _UMB; //Unit Marginal Benifit
    unsigned int * _UMBC;
    

  // JS: This is a hack.  I need to get the size of a MemBlockEntry to create a 
  // shared memory resetter.
  public:
    static unsigned int getSizeOfMemBlockEntry() { return sizeof(MemBlockEntry); }
};

#endif /* SharedMemoryCache_H */
