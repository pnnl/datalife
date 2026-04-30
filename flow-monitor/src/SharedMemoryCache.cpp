#include "SharedMemoryCache.h"
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "Message.h"
#include "ReaderWriterLock.h"
#include "Timer.h"
#include "lz4.h"

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <future>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)
//#define PPRINTF(...) fprintf(stdout, __VA_ARGS__); fflush(stdout)
#define PPRINTF(...)

#define SCALEABLE_METRIC_FILE_MAX 1000


SharedMemoryCache::SharedMemoryCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, Cache * scalableCache) : BoundedCache(cacheName, type, cacheSize, blockSize, associativity, scalableCache),
                                                                                                                                                                           _UMBLock(NULL),
                                                                                                                                                                           _UMB(NULL),
                                                                                                                                                                           _UMBC(NULL){
    // std::cout<<"[MONITOR] " << "Constructing " << _name << " in shared memory cache" << std::endl;
    stats.start(false, CacheStats::Metric::constructor);
    std::string filePath("/" + Config::monitor_id + "_" + _name + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity));
    uint64_t memSize = sizeof(uint32_t) + _cacheSize + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins) + sizeof(ReaderWriterLock) + (sizeof(double) + sizeof(unsigned int)) * SCALEABLE_METRIC_FILE_MAX;

    bool created = true;
    //std::this_thread::sleep_for(std::chrono::seconds(1));
    int fd = shm_open(filePath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666); //try to create file (O_EXCL)
    PPRINTF("Trying to open %s %d\n", filePath.c_str(), fd);
    if (fd == -1) {
        log(this) << "Reusing shared memory" << std::endl;
        //std::this_thread::sleep_for(std::chrono::seconds(2));
        fd = shm_open(filePath.c_str(), O_RDWR, 0666);
        PPRINTF("2nd try to open %s %d\n", filePath.c_str(), fd);
        created = false;
    }
    
    if(fd != -1){
        ftruncate(fd, memSize);
        void *ptr = mmap(NULL, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        uint32_t *init = (uint32_t *)ptr;
        log(this) << "start init: " << *init << std::endl;

        _blocks = (uint8_t *)init + sizeof(uint32_t);
        _blkIndex = (MemBlockEntry *)((uint8_t *)_blocks + _cacheSize);
        uint8_t * binLockDataAddr = (uint8_t *)_blkIndex + _numBlocks * sizeof(MemBlockEntry);
        
        //JS: This is the addition for the scalable metric
        uint8_t * endPtr = (uint8_t*)ptr + memSize;
        uint8_t * lockAddr = binLockDataAddr + MultiReaderWriterLock::getDataSize(_numBins);
        uint8_t * UMBAddr = lockAddr + sizeof(ReaderWriterLock);
        uint8_t * UMBCAddr = UMBAddr + sizeof(double) * SCALEABLE_METRIC_FILE_MAX;
        if(UMBCAddr + sizeof(uint32_t) * SCALEABLE_METRIC_FILE_MAX != endPtr) {
            PRINTF("JS: REUSE: ERROR ON SHARED MEMORY POINTER ARITHMETIC!!! %s\n", filePath.c_str());
            PRINTF("JS: REUSE: %p - %p = %u\n", endPtr, UMBCAddr + sizeof(uint32_t) * SCALEABLE_METRIC_FILE_MAX, endPtr - (UMBCAddr + sizeof(uint32_t) * SCALEABLE_METRIC_FILE_MAX));
        }
        _UMB = (double*)UMBAddr;
        _UMBC = (unsigned int*) UMBCAddr;

        if(created || *init==2) {
            PPRINTF("RESETING %s %d\n", filePath.c_str(), fd);
            _binLock = new MultiReaderWriterLock(_numBins, binLockDataAddr, true);
            _binLock->writerLock(0);
            memset(_blocks, 0, _cacheSize);
            for (uint32_t i=0;i<_numBlocks;i++){
                _blkIndex[i].init(this,i);
            }
            _binLock->writerUnlock(0);

            _UMBLock = new (lockAddr) ReaderWriterLock();
            for(unsigned int i=0; i<SCALEABLE_METRIC_FILE_MAX; i++) {
                _UMB[i] = 0;
                _UMBC[i] = 0;
            }

            *init = 1;
        }
        else {
            while (*init != 1) {
                sched_yield();
            }
            _binLock = new MultiReaderWriterLock(_numBins, binLockDataAddr);
            _UMBLock = (ReaderWriterLock*)lockAddr;
        }

        log(this) << "end init: " << (uint32_t)*init << std::endl;
    }
    else {
        std::cerr << "[MONITOR] "
                    << "Error opening shared memory " << strerror(errno) << std::endl;
    }
    log(this) << (void *)_blkIndex << " " << (void *)_binLock << std::endl;
    debug()<<"blk start: "<<(void*)_blkIndex<<" blk end: "<<(void*)&_blkIndex[_numBlocks]<<std::endl;
    // for (uint32_t i=0;i<10; i++){
    //     std::cout <<"[MONITOR]" << _name << " " << blockEntryStr((BlockEntry*)&_blkIndex[i]) << std::endl;
    // }

    _shared = true;
    stats.end(false, CacheStats::Metric::constructor);
}

SharedMemoryCache::~SharedMemoryCache() {
    //std::cout<<"[MONITOR] " << "deleting " << _name << " in shared memory cache, collisions: " << _collisions << std::endl;
    //std::cout<<"[MONITOR] " << "numBlks: " << _numBlocks << " numBins: " << _numBins << " cacheSize: " << _cacheSize << std::endl;
    stats.start(false, CacheStats::Metric::destructor);
    if (false) {
        //code from FileCacheRegister...
    }
    uint32_t numEmpty = 0;
    for (uint32_t i = 0; i < _numBlocks; i++) {
        if (_blkIndex[i].activeCnt > 0) {
            std::cerr << "[MONITOR] " << _name << " " << i << " " << _numBlocks << " " << _blkIndex[i].activeCnt << " " << _blkIndex[i].fileIndex - 1 << " " << _blkIndex[i].blockIndex - 1 << " "
                      << "prefetched" << _blkIndex[i].prefetched << std::endl;
        }
        if(_blkIndex[i].status == BLK_EMPTY){
            numEmpty+=1;
        }
    }
    std::cout<<_name<<" number of empty blocks: "<<numEmpty<<std::endl;
    stats.end(false, CacheStats::Metric::destructor);
    stats.print(_name);
    std::cout << std::endl;

    delete _binLock;
}

void SharedMemoryCache::setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size) {
    memcpy(&_blocks[blockIndex * _blockSize], data, size);
}
uint8_t *SharedMemoryCache::getBlockData(unsigned int blockIndex) {
    uint8_t *temp = (uint8_t *)&_blocks[blockIndex * _blockSize];
    return temp;
}

void SharedMemoryCache::cleanUpBlockData(uint8_t *data) {
    // debug()<<_name<<" (not)delete data"<<std::endl;
}

void SharedMemoryCache::blockSet(BlockEntry* blk, uint32_t fileIndex, uint32_t blockIndex, uint8_t status, CacheType type, int32_t prefetched, int activeUpdate, Request* req) {
    req->trace(_name)<<"setting block: "<<blockEntryStr(blk)<<std::endl;
    blk->fileIndex = fileIndex;
    blk->blockIndex = blockIndex;
    blk->timeStamp = Timer::getCurrentTime();
    blk->origCache.store(type);
    blk->status = status;
    if (prefetched >= 0) {
        blk->prefetched = prefetched;
    }
    if (activeUpdate >0){
        incBlkCnt(blk,req);
    }
    else if (activeUpdate < 0){
        decBlkCnt(blk,req);
    }
    req->trace(_name)<<"blockset: "<<blockEntryStr(blk)<<std::endl;
}

typename BoundedCache<MultiReaderWriterLock>::BlockEntry* SharedMemoryCache::getBlockEntry(uint32_t blockIndex, Request* req){
    return (BlockEntry*)&_blkIndex[blockIndex];
}


// blockAvailable calls are not protected by locks
// we know that when calling blockAvailable the actual data cannot change during the read (due to refernce counting protections)
// what can change is the meta data associated with the last access of the data (and which cache it originated from)
// in certain cases, we want to capture the originating cache and return it for performance reasons
// there is a potential race between when a block is available and when we capture the originating cache
// having origCache be atomic ensures we always get a valid ID (even though it may not always be 100% acurate)
// we are willing to accept some small error in attribution of stats
bool SharedMemoryCache::blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs, uint32_t cnt, CacheType *origCache) {
    bool avail = _blkIndex[index].status == BLK_AVAIL;
    if (origCache && avail) {
        *origCache = CacheType::empty;
        *origCache = _blkIndex[index].origCache.load();
        // for better or for worse we allow unlocked access to check if a block avail, 
        // there is potential for a race here when some over thread updates the block (not changing the data, just the metadata)
        while(*origCache == CacheType::empty){ 
            *origCache = _blkIndex[index].origCache.load();
        }
        return true;
    }
    return avail;
}


std::vector<BoundedCache<MultiReaderWriterLock>::BlockEntry*> SharedMemoryCache::readBin(uint32_t binIndex) {
    std::vector<BlockEntry*> entries;
    int startIndex = binIndex * _associativity;
    for (uint32_t i = 0; i < _associativity; i++) {
        entries.push_back((BlockEntry *)&_blkIndex[i + startIndex]);
    }
    return entries;
}

std::string  SharedMemoryCache::blockEntryStr(BlockEntry *entry){
    return entry->str() + " ac: "+ std::to_string(((MemBlockEntry*)entry)->activeCnt);
}

int SharedMemoryCache::incBlkCnt(BlockEntry * entry, Request* req) {
    req->trace(_name)<<"incrementing"<<std::endl;
    return ((MemBlockEntry*)entry)->activeCnt.fetch_add(1);
}
int SharedMemoryCache::decBlkCnt(BlockEntry * entry, Request* req) {
    req->trace(_name)<<"decrementing"<<std::endl;
    return ((MemBlockEntry*)entry)->activeCnt.fetch_sub(1);
}

bool SharedMemoryCache::anyUsers(BlockEntry * entry, Request* req) {
    return ((MemBlockEntry*)entry)->activeCnt;
}

Cache *SharedMemoryCache::addSharedMemoryCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, Cache * scalableCache) {
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [&]() -> Cache * {
            Cache *temp = new SharedMemoryCache(cacheName, type, cacheSize, blockSize, associativity,scalableCache);
            return temp;
        });
}

void SharedMemoryCache::setLastUMB(std::vector<std::tuple<uint32_t, double>> &UMBList) {
    PPRINTF("SM-------------setLastUMB\n");
    _UMBLock->writerLock();
    for(const auto &UMB : UMBList) {
        uint32_t index = std::get<0>(UMB);
        double umb = std::get<1>(UMB);
        if(index < SCALEABLE_METRIC_FILE_MAX) {
            _UMB[index] += umb;
            _UMBC[index]++;
        }
    }
    _UMBLock->writerUnlock();
}

double SharedMemoryCache::getLastUMB(uint32_t fileIndex) {
    double ret = std::numeric_limits<double>::min();
    if(fileIndex < SCALEABLE_METRIC_FILE_MAX) {
        _UMBLock->readerLock();
        if(_UMBC[fileIndex] && !isnan(_UMB[fileIndex]) && !isinf(_UMB[fileIndex])) {
            ret = _UMB[fileIndex] / _UMBC[fileIndex];
            PPRINTF("SM-------------getLastUMB %lf %u\n", _UMB[fileIndex], _UMBC[fileIndex]);
        }
        _UMBLock->readerUnlock();
    }
    else{
        //THIS IS NOT EXPECTED BEHAVIOR. WE DON'T SUPPORT #OF FILES MORE THAN SCALEABLE_METRIC_FILE_MAX
        std::cerr << "[MONITOR] We do not currently support number of files more than "<< SCALEABLE_METRIC_FILE_MAX << std::endl;
        raise(SIGSEGV);
    }
    return ret;
}
