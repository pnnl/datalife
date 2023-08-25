#ifndef CACHETYPES_H_
#define CACHETYPES_H_
enum CacheType{
    empty,
    base,
    privateMemory,
    sharedMemory,
    burstBuffer,
    nodeFile,
    boundedGlobalFile,
    globalFileLock,
    globalFcntl,
    network,
    local,
    urlCache,
    scalable
};

static inline std::string cacheTypeName(CacheType type){
    switch (type) {
        case CacheType::empty:
            return "empty";
        case CacheType::base:
            return "base";
        case CacheType::privateMemory:
            return "privateMemory";
        case CacheType::sharedMemory:
            return "sharedMemory";
        case CacheType::burstBuffer:
            return "burstBuffer";
        case CacheType::nodeFile:
            return "nodeFile";
        case CacheType::boundedGlobalFile:
            return "boundedGlobalFile";
        case CacheType::globalFileLock:
            return "globalFileLock";
        case CacheType::globalFcntl:
            return "globalFcntl";
        case CacheType::network:
            return "network";
        case CacheType::local:
            return "local";
        case CacheType::urlCache:
            return "urlCache";
        case CacheType::scalable:
            return "scalable";
        default:
            return "unknown cache type";
    }
}

#endif