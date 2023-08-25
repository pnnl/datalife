#ifndef PERFECTPREFETCHER_H
#define PERFECTPREFETCHER_H

#include "Prefetcher.h"
#include "Loggable.h"

#include <vector>

//For testing purposes: This prefetcher loads the list of blocks requested by the task
class PerfectPrefetcher : public Prefetcher {

public:
    PerfectPrefetcher(std::string name, std::string fileName);
    ~PerfectPrefetcher();

    std::vector<uint64_t> getBlocks(uint32_t fileIndex, uint64_t startBlk, uint64_t endBlk, uint64_t numBlocks, uint64_t blkSize, uint64_t fileSize);
private:
    void loadAccessTrace(std::string fileName);

    std::vector<std::pair<uint64_t,uint64_t>> _trace; //Trace containing the blocks accessed by the application as <startBlk, endBlk> pairs
    int _lastIndex; //Last index called in trace


};

#endif //PERFECTPREFETCHER_H