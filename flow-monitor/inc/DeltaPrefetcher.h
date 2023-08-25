#ifndef DELTAPREFETCHER_H
#define DELTAPREFETCHER_H

#include "Prefetcher.h"
#include "Loggable.h"

//This class prefetches blocks [startBlock+delta, startBock+delta+numPrefetchBlocks]
class DeltaPrefetcher : public Prefetcher {
public:
    DeltaPrefetcher(std::string name);
    ~DeltaPrefetcher();

    std::vector<uint64_t> getBlocks(uint32_t fileIndex, uint64_t startBlk, uint64_t endBlk,uint64_t numBlocks, uint64_t blkSize, uint64_t fileSize);
private:
    uint32_t _numBlocks;



};

#endif //NEXTPREFETCHER_H