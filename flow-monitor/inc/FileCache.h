#ifndef FileCache_H
#define FileCache_H
#include "BoundedCache.h"
#include <atomic>
#include <future>
#include <map>
#include <unordered_map>
#include <unordered_set>

#define FILECACHENAME "fileCache"

class FileCache : public BoundedCache<MultiReaderWriterLock> {
  public:
    FileCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string cachePath);
    ~FileCache();
    static Cache *addFileCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string cachePath);

  private:
    struct MemBlockEntry : BlockEntry {
        std::atomic<uint32_t> activeCnt;
        void init(BoundedCache* c,uint32_t entryId){
          //BlockEntry::init(c,entryId);
                    //update
          BlockEntry::init(c);
	  std::atomic_init(&activeCnt, (uint32_t)0);
        }
    };

    void readFromFile(int fd, uint64_t size, uint8_t *buff);
    void writeToFile(int fd, uint64_t size, uint8_t *buff);

    void preadFromFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset);
    void pwriteToFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset);
    
    virtual void blockSet(BlockEntry* blk,  uint32_t fileIndex, uint32_t blockIndex, uint8_t byte, CacheType type, int32_t prefetch, int activeUpdate,Request* req);
    virtual bool blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs = false, uint32_t cnt = 0, CacheType *origCache = NULL);

    virtual uint8_t *getBlockData(unsigned int blockIndex);
    virtual void setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size);
    virtual BlockEntry* getBlockEntry(uint32_t blockIndex,  Request* req);
    //virtual std::vector<BlockEntry*> readBin(uint32_t binIndex);
    //update
    std::vector<std::shared_ptr<BlockEntry>> readBin(uint32_t binIndex);

    virtual std::string blockEntryStr(BlockEntry *entry);

    virtual int incBlkCnt(BlockEntry * entry, Request* req);
    virtual int decBlkCnt(BlockEntry * entry, Request* req);
    virtual bool anyUsers(BlockEntry * entry, Request* req);

    virtual void cleanUpBlockData(uint8_t *data);
    

    MemBlockEntry *_blkIndex; //keep the entry meta info in shared memory, but the actual data on disk.

    uint32_t _pid;
    std::string _cachePath;
    std::string _indexPath;
    // std::atomic_uint *_fullFlag;
    int _blocksfd; //the raw data file
    // int _blkIndexfd; //

    unixopen_t _open;
    unixclose_t _close;
    unixread_t _read;
    unixwrite_t _write;
    unixlseek_t _lseek;
    unixfsync_t _fsync;
};

#endif /* BUSTBUFFERCACHE_H */
