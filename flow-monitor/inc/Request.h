#ifndef REQUEST_H
#define REQUEST_H
#include "Timer.h"
#include "CacheTypes.h"
#include "Loggable.h"

#include <sstream>
#include <string.h>
#include <unordered_map>
#include <thread>

class Cache;

struct RequestTrace{
    RequestTrace(std::ostream *o, bool trigger): _o(o), _trigger(trigger){ }

    template <class T>
    RequestTrace &operator<<(const T &val) {
        if (_trigger){
            *_o << val;
        }
        return *this;
    }

    RequestTrace &operator<<(std::ostream &(*f)(std::ostream &)) {
        if (_trigger){
            *_o << f;
        }
        return *this;
    }

    RequestTrace &operator<<(std::ostream &(*f)(std::ios &)) {
        if (_trigger){
            *_o << f;
        }
        return *this;
    }

    RequestTrace &operator<<(std::ostream &(*f)(std::ios_base &)) {
        if (_trigger){
            *_o << f;
        }
        return *this;
    }
    private:
    std::ostream *_o;
    bool _trigger;

};

struct Request {
    uint8_t *data;
    Cache *originating;
    uint32_t blkIndex;
    uint32_t fileIndex;
    uint64_t offset;
    uint64_t size;
    uint64_t time;
    uint64_t retryTime;
    uint64_t deliveryTime;
    std::unordered_map<Cache *, uint8_t> reservedMap;
    bool ready;
    bool printTrace;
    bool globalTrigger;
    bool skipWrite;
    CacheType waitingCache;
    std::unordered_map<Cache *, uint64_t> indexMap;
    std::unordered_map<Cache *, uint64_t> blkIndexMap;
    std::unordered_map<Cache *, uint64_t> fileIndexMap;
    std::unordered_map<Cache *, uint64_t> statusMap;
    uint64_t id;
    std::thread::id threadId;
    std::stringstream ss;

    Request() : data(NULL),originating(NULL),blkIndex(0),fileIndex(0), offset(0), size(0), deliveryTime(0), globalTrigger(false), skipWrite(false), threadId(std::this_thread::get_id()){ }
    Request(uint32_t blk, uint32_t fileIndex, uint64_t size, uint64_t offset, Cache *orig, uint8_t *data) : data(data), originating(orig), blkIndex(blk), fileIndex(fileIndex), 
                                                                                           offset(offset), size(size), time(Timer::getCurrentTime()), retryTime(0), deliveryTime(0), ready(false), 
                                                                                           printTrace(false), globalTrigger(false), skipWrite(false),
                                                                                           waitingCache(CacheType::empty),id(Request::ID_CNT.fetch_add(1)), threadId(std::this_thread::get_id()) {

    }
    ~Request();
    std::string str();
    void flushTrace();


    RequestTrace trace(std::string tag = "");
    RequestTrace trace(bool trigger = true, std::string tag = "");

    private:
        
        static std::atomic<uint64_t> ID_CNT;
        static std::atomic<uint64_t> RET_ID_CNT;
};



#endif //REQUEST_H