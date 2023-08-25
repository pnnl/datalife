#ifndef SCALABLE_ALLOCATOR_H
#define SCALABLE_ALLOCATOR_H
#include "ScalableMetaData.h"
#include <vector>
#include <atomic>

// #define PPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

class MonitorAllocator : public Trackable<std::string, MonitorAllocator *>
{
    protected:
        //JS: This hook will turn on and off code path to perform LRU across all blocks in ScalableCache::set_block
        const bool _canFail;
        uint64_t _blockSize;
        uint64_t _maxSize;

        std::atomic<uint64_t> _availBlocks;
    
        MonitorAllocator(uint64_t blockSize, uint64_t maxSize, bool canFail):
            _canFail(canFail),
            _blockSize(blockSize),
            _maxSize(maxSize),
            _availBlocks(maxSize/blockSize) { }

        ~MonitorAllocator() { }

        template<class T>
        static MonitorAllocator * addAllocator(std::string name, uint64_t blockSize, uint64_t maxSize) {
            bool created = false;
            auto ret = Trackable<std::string, MonitorAllocator *>::AddTrackable(
                name, [&]() -> MonitorAllocator * {
                    MonitorAllocator *temp = new T(blockSize, maxSize);
                    return temp;
                }, created);
            return ret;
        }

    public:
        virtual uint8_t * allocateBlock(uint32_t allocateForFileIndex, bool must = false) = 0;
        virtual void closeFile(ScalableMetaData * meta) { }
        virtual void openFile(ScalableMetaData * meta) { }
        bool canReturnEmpty() { return _canFail; }
};

//JS: This is an example of the simplest allocator I can think of
class SimpleAllocator : public MonitorAllocator
{
    public:
        SimpleAllocator(uint64_t blockSize, uint64_t maxSize):
            MonitorAllocator(blockSize, maxSize, false) { }

        uint8_t * allocateBlock(uint32_t allocateForFileIndex, bool must = false) {
            return new uint8_t[_blockSize];
        }

        static MonitorAllocator * addSimpleAllocator(uint64_t blockSize, uint64_t maxSize) {
            return addAllocator<SimpleAllocator>(std::string("SimpleAllocator"), blockSize, maxSize);
        }
};

//JS: This is an allocator that can run out of space
class FirstTouchAllocator : public MonitorAllocator
{
    private:
        std::atomic<uint64_t> _numBlocks;
        uint64_t _maxBlocks;
    
    public:
        FirstTouchAllocator(uint64_t blockSize, uint64_t maxSize):
            //JS: Notice we are setting the canFail to true!
            MonitorAllocator(blockSize, maxSize, true),
            _numBlocks(0),
            _maxBlocks(maxSize / blockSize) { 
                // PPRINTF("NUMBER OF BLOCKS: %lu\n", _maxBlocks);
            }

        uint8_t * allocateBlock(uint32_t allocateForFileIndex, bool must = false) {
            uint64_t temp = _numBlocks.fetch_add(1);
            if(temp < _maxBlocks)
                return new uint8_t[_blockSize];
            _numBlocks.fetch_sub(1);
            return NULL;
        }

        static MonitorAllocator * addFirstTouchAllocator(uint64_t blockSize, uint64_t maxSize) {
            return addAllocator<FirstTouchAllocator>(std::string("FirstTouchAllocator"), blockSize, maxSize);
        }
};

#endif /* SCALABLE_ALLOCATOR_H */
