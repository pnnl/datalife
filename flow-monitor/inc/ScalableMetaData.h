#ifndef SCALABLE_META_DATA_H
#define SCALABLE_META_DATA_H
#include "ReaderWriterLock.h"
#include "Trackable.h"
#include "Histogram.h"
#include "Cache.h"
#include <atomic>
#include <deque>
#include <vector>
#include <array>

const char* const patternName[] = { "UNKNOWN", "BLOCKSTREAMING" };

struct ScalableMetaData {
    private:
        enum StridePattern {
            UNKNOWN = 0,
            BLOCKSTREAMING
        };

        struct BlockEntry {
            int64_t blockIndex;
            ReaderWriterLock blkLock;
            std::atomic<uint64_t> timeStamp;
            std::atomic<uint8_t*> data;
            BlockEntry(): blockIndex(0), timeStamp(0), data(0) { }
        };

        uint64_t fileSize;
        uint64_t blockSize;
        uint64_t totalBlocks;
        StridePattern pattern;
        ReaderWriterLock metaLock;
        BlockEntry * blocks;

        //JS: For Nathan
        bool recalc;
        uint64_t access;
        uint64_t accessPerInterval;
        uint64_t lastMissTimeStamp;
        double unitBenefit;
        double unitMarginalBenefit;
        double upperLevelMetric;
        double prevUnitBenefit;
        int prevSize;
        //BM: for algorithm calculations.
        double lastDeliveryTime;
        int partitionMissCount;
        double partitionMissCost;

        std::atomic<uint64_t> numBlocks;
        
        //JS: Also for Nathan (intervalTime, fpGrowth, missInverval)
        Histogram missInterval;
        Histogram benefitHistogram;

        std::deque<std::array<uint64_t, 3>> window;
        std::vector<BlockEntry*> currentBlocks;

    public:
        ScalableMetaData(uint64_t bSize, uint64_t fSize):
            fileSize(fSize),
            blockSize(bSize),
            totalBlocks(fSize / bSize + ((fSize % bSize) ? 1 : 0)),
            pattern(UNKNOWN),
            recalc(false),
            access(0),
            accessPerInterval(0),
            lastMissTimeStamp(0),
            unitBenefit(0),
            prevUnitBenefit(0),
            prevSize(0),
            unitMarginalBenefit(0),
            upperLevelMetric(std::numeric_limits<double>::min()),//default value
            lastDeliveryTime(-1.0),
            partitionMissCount(0),
            partitionMissCost(0),
            numBlocks(0),
            //OCEANE: Number in the paranthesis sets the number of buckets for histogram [ missInterval(n) n--> number of buckets]
            missInterval(10),
            benefitHistogram(100,Config::TraceHistogram) {
                blocks = new BlockEntry[totalBlocks];
                for(unsigned int i=0; i<totalBlocks; i++) {
                    blocks[i].data.store(NULL);
                    blocks[i].blockIndex = i;
                }
            }
        
        //JS: These are modifying blocks data, meta data, and usage
        uint8_t * getBlockData(uint64_t blockIndex, uint64_t fileOffset, bool &reserve, bool track);
        void setBlock(uint64_t blockIndex, uint8_t * data);
        void decBlockUsage(uint64_t blockIndex);
        bool backOutOfReservation(uint64_t blockIndex); 

        //JS: These are huristics to support cache/allocators
        bool checkPattern(Cache * cache=NULL, uint32_t fileIndex=0);
        uint8_t * oldestBlock(uint64_t &blockIndex);
        uint8_t * randomBlock(uint64_t &blockIndex);
        uint64_t getLastTimeStamp();
        uint64_t getNumBlocks();
        uint64_t getPattern();

        //JS: From Nathan
        void updateStats(bool miss, uint64_t timestamp);
        double calcRank(uint64_t time, uint64_t misses);
        void updateRank(bool dec);

        //BM: for deliverytime 
        void updateDeliveryTime(double deliveryTime);
        //BM: for plots/tracing
        double getUnitMarginalBenefit();
        double getUnitBenefit();
        double getUpperMetric();
        void printHistLogs(int i){benefitHistogram.printLog(i);}
    private:
        uint64_t trackAccess(uint64_t blockIndex, uint64_t readIndex);
};

#endif //SCALABLE_META_DATA_H

  
