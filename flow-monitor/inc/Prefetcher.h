#ifndef PREFETCHER_H
#define PREFETCHER_H

#include "Loggable.h"
#include <vector>

#define PERFILE -1
#define NONE 0
#define DELTA 1
#define PERFECT 2

//TODO: Store some stats?
class Prefetcher : public Loggable {
public:
    Prefetcher(std::string name);
    virtual ~Prefetcher();

    virtual std::vector<uint64_t> getBlocks(uint32_t fileIndex, uint64_t startBlk, uint64_t endBlk, uint64_t numBlocks, uint64_t blkSize, uint64_t fileSize) = 0;

protected:
    std::string _name;

    std::string blocks2String(std::vector<uint64_t> v);
};

#endif /* PREFETCHER_H */
