#ifndef CONFIG_H
#define CONFIG_H
#include <string>
#include <cstdint>

namespace Config {

#ifndef TEST_SERVER_PORT
const int serverPort = 6023;
#else
const int serverPort = TEST_SERVER_PORT;
#endif

//This will break for rsockets...
#ifndef TEST_SERVER_IP
const std::string serverIpString("127.0.0.1");
#else
const std::string serverIpString(TEST_SERVER_IP);
#endif

#ifndef INPUTS_DIR
const std::string InputsDir(".");
#else
const std::string InputsDir(INPUTS_DIR);
#endif

const int maxSystemFd = 19459526;

#define NUMTHREADS 1

//Thread Pools
const unsigned int numServerThreads = 16;
const unsigned int numServerCompThreads = 1;
const unsigned int numClientTransThreads = NUMTHREADS;
const unsigned int numClientDecompThreads = 1;
const unsigned int numWriteBufferThreads = 1;
const unsigned int numPrefetchThreads = 1;

//Connection Parameters
const unsigned int defaultBufferSize = 1024;
const unsigned int maxConRetry = 10;
const unsigned int messageRetry = (unsigned int)100; //Simulates infinite retry...
const unsigned int socketsPerConnection =  getenv("MONITOR_SOCKETS_PER_CONN") ? atoi(getenv("MONITOR_SOCKETS_PER_CONN")) : 1;
const unsigned int socketStep = 1024;
const unsigned int socketRetry = 1;

//Input file Parameters
const unsigned int fileOpenRetry = 1;

//Serve file Parameters
const unsigned int numCompressTask = 0;
const unsigned int removeOutput = 0;

static int64_t referenceTime = getenv("MONITOR_REF_TIME") ? atol(getenv("MONITOR_REF_TIME")) : 0;

//architecure Parameters
const bool enableSharedMem = getenv("MONITOR_ENABLE_SHARED_MEMORY") ? atoi(getenv("MONITOR_ENABLE_SHARED_MEMORY")) : 1;

//----------------------cache parameters-----------------------------------

const std::string monitor_id(getenv("USER") ? "monitor" + std::string(getenv("USER")) : "monitor");

const uint64_t maxBlockSize = getenv("MONITOR_BLOCKSIZE") ? atol(getenv("MONITOR_BLOCKSIZE")) : 1 * 1024UL * 1024UL;
#define BOUNDEDCACHENAME "boundedcache"
#define NETWORKCACHENAME "network"

//Memory Cache Parameters
const bool useMemoryCache = true;
static uint64_t memoryCacheSize = getenv("MONITOR_PRIVATE_MEM_CACHE_SIZE") ? atol(getenv("MONITOR_PRIVATE_MEM_CACHE_SIZE")) : 60 * 1024 * 1024UL;
const uint32_t memoryCacheAssociativity = 16UL;
const uint64_t memoryCacheBlocksize = maxBlockSize;

//Scalable Cache Parameters
const bool useScalableCache = getenv("MONITOR_SCALABLE_CACHE") ? atoi(getenv("MONITOR_SCALABLE_CACHE")) : 1;
const uint32_t scalableCacheNumBlocks = getenv("MONITOR_SCALABLE_CACHE_NUM_BLOCKS") ? atoi(getenv("MONITOR_SCALABLE_CACHE_NUM_BLOCKS")) : 60;
//0:addAdaptiveAllocator, 1:addStealingAllocator (LRU), 2:addRandomStealingAllocator (Random File, Oldest Block), 3:addRandomStealingAllocator (Random File, Random Block), 4:LargestStealingAllocator, 5:FirstTouchAllocator, 6:addSimpleAllocator
const uint32_t scalableCacheAllocator = getenv("MONITOR_SCALABLE_CACHE_ALLOCATOR") ? atoi(getenv("MONITOR_SCALABLE_CACHE_ALLOCATOR")) : 0;


//Shared Memory Cache Parameters
const bool useSharedMemoryCache = getenv("MONITOR_SHARED_MEM_CACHE") ? atoi(getenv("MONITOR_SHARED_MEM_CACHE")) : 0;
static uint64_t sharedMemoryCacheSize = getenv("MONITOR_SHARED_MEM_CACHE_SIZE") ? atol(getenv("MONITOR_SHARED_MEM_CACHE_SIZE")) : 1 * 1024 * 1024 * 1024UL;
const uint32_t sharedMemoryCacheAssociativity = 16UL;
const uint64_t sharedMemoryCacheBlocksize = maxBlockSize;

//BurstBuffer Cache Parameters
const bool useBurstBufferCache = getenv("MONITOR_BB_CACHE") ? atoi(getenv("MONITOR_BB_CACHE")) : 0;
static uint64_t burstBufferCacheSize = getenv("MONITOR_BB_CACHE_SIZE") ? atol(getenv("MONITOR_BB_CACHE_SIZE")) : 1 * 1024 * 1024 * 1024UL;
const uint32_t burstBufferCacheAssociativity = 16UL;
const uint64_t burstBufferCacheBlocksize = maxBlockSize;
const std::string burstBufferCacheFilePath("/tmp/" + monitor_id + "/monitor_cache/bbc"); // TODO: have option to pass from environment variable

//File Cache Parameters
const bool useFileCache = getenv("MONITOR_FILE_CACHE") ? atoi(getenv("MONITOR_FILE_CACHE")) : 0;
static uint64_t fileCacheSize = getenv("MONITOR_FILE_CACHE_SIZE") ? atol(getenv("MONITOR_FILE_CACHE_SIZE")) : 1 * 1024 * 1024 * 1024UL;
const uint32_t fileCacheAssociativity = 16UL;
const uint64_t fileCacheBlocksize = maxBlockSize;
const std::string fileCacheFilePath("/tmp/" + monitor_id + "/monitor_cache/fc"); // TODO: have option to pass from environment variable

//Bounded Filelock Cache Parameters
const bool useBoundedFilelockCache = getenv("MONITOR_BOUNDED_FILELOCK_CACHE") ? atoi(getenv("MONITOR_BOUNDED_FILELOCK_CACHE")) : 0;
static uint64_t boundedFilelockCacheSize = getenv("MONITOR_BOUNDED_FILELOCK_CACHE_SIZE") ? atol(getenv("MONITOR_BOUNDED_FILELOCK_CACHE_SIZE")) : 1 * 1024 * 1024 * 1024UL;
const uint32_t boundedFilelockCacheAssociativity = 16UL;
const uint64_t boundedFilelockCacheBlocksize = maxBlockSize;
const std::string boundedFilelockCacheFilePath = getenv("MONITOR_BOUNDED_FILELOCK_CACHE_PATH") ? std::string(getenv("MONITOR_BOUNDED_FILELOCK_CACHE_PATH")) : std::string("/tmp/" + monitor_id + "/monitor_cache/gc"); // TODO: have option to pass from environment variable

//Filelock Cache Parameters
const bool useFilelockCache = getenv("MONITOR_FILELOCK_CACHE") ? atoi(getenv("MONITOR_FILELOCK_CACHE")) : 0;
const uint64_t filelockCacheBlocksize = maxBlockSize;
const std::string filelockCacheFilePath = getenv("MONITOR_FILELOCK_CACHE_PATH") ? std::string(getenv("MONITOR_FILELOCK_CACHE_PATH")) : std::string("/tmp/" + monitor_id + "/monitor_cache/gc");

//Network Cache Parameters
const bool useNetworkCache = getenv("MONITOR_NETWORK_CACHE") ? atoi(getenv("MONITOR_NETWORK_CACHE")) : 1;
const uint64_t networkBlockSize = maxBlockSize;

//LocalFile Cache Parameters
const bool useLocalFileCache = getenv("MONITOR_LOCAL_FILE_CACHE") ? atoi(getenv("MONITOR_LOCAL_FILE_CACHE")) : 0;
const uint64_t localFileBlockSize = maxBlockSize;

//--------------------------------------------------------------------------------

const bool timerOn = true;
const bool printThreadMetric = false;
const bool printNodeMetric = true;
const bool printHits = true;
const int printStats = getenv("MONITOR_PRINT_STATS") ? atoi(getenv("MONITOR_PRINT_STATS")) : 1;

//-----------------------------------------------------

// server parameters
const bool useServerNetworkCache = getenv("MONITOR_NETWORK_CACHE") ? atoi(getenv("MONITOR_NETWORK_CACHE")) : 0;
const uint64_t serverCacheSize = getenv("MONITOR_SERVER_CACHE_SIZE") ? atol(getenv("MONITOR_SERVER_CACHE_SIZE")) : 20UL * 1024 * 1024 * 1024;
const uint64_t serverCacheBlocksize = maxBlockSize;
const uint32_t serverCacheAssociativity = 16UL;
const std::string ServerConnectionsPath(getenv("MONITOR_SERVER_CONNECTIONS") ? getenv("MONITOR_SERVER_CONNECTIONS") : "");

const bool prefetchEvict = getenv("MONITOR_PREFETCH_EVICT") ? atoi(getenv("MONITOR_PREFETCH_EVICT")) : 0; //When evicting a block, choose prefetched blocks first.

//-----------------------------------------------------
// Prefetching config
const unsigned int prefetcherType = getenv("MONITOR_PREFETCHER_TYPE") ? atoi(getenv("MONITOR_PREFETCHER_TYPE")) : 0;
const unsigned int numPrefetchBlks = getenv("MONITOR_PREFETCH_NUM_BLKS") ? atoi(getenv("MONITOR_PREFETCH_NUM_BLKS")) : 1;
const int prefetchDelta = getenv("MONITOR_PREFETCH_DELTA") ? atoi(getenv("MONITOR_PREFETCH_DELTA")) : 1;
const std::string prefetchFileDir = getenv("MONITOR_PREFETCH_FILEDIR") ? getenv("MONITOR_PREFETCH_FILEDIR") : "./";

//const bool prefetchGlobal = getenv("MONITOR_PREFETCH_GLOBAL") ? atoi(getenv("MONITOR_PREFETCH_GLOBAL")) : 1;
//const unsigned int prefetchGap = getenv("MONITOR_PREFETCH_GAP") ? atoi(getenv("MONITOR_PREFETCH_GAP")) : 0;
//const bool prefetchAllBlks = getenv("MONITOR_PREFETCH_ALLBLKS") ? atoi(getenv("MONITOR_PREFETCH_ALLBLKS")) : 0;
//const bool prefetchNextBlks = getenv("MONITOR_PREFETCH") ? atoi(getenv("MONITOR_PREFETCH")) : 1;

//-----------------------------------------------------
//Curl parameters
const unsigned int numCurlHandles = getenv("MONITOR_CURL_HANDLES") ? atoi(getenv("MONITOR_CURL_HANDLES")) : 10;
const long UrlTimeOut = getenv("MONITOR_URL_TIMEOUT") ? atol(getenv("MONITOR_URL_TIMEOUT")) : 1; //Seconds
const std::string DownloadPath(getenv("MONITOR_DOWNLOAD_PATH") ? getenv("MONITOR_DOWNLOAD_PATH") : ".");
const bool deleteDownloads = getenv("MONITOR_DELETE_DOWNLOADS") ? atoi(getenv("MONITOR_DELETE_DOWNLOADS")) : 1;
const bool downloadForSize = getenv("MONITOR_DOWNLOAD_FOR_SIZE") ? atoi(getenv("MONITOR_DOWNLOAD_FOR_SIZE")) : 1;
const bool curlOnStartup = getenv("MONITOR_CURL_ON_START") ? atoi(getenv("MONITOR_CURL_ON_START")) : 0;
const bool urlFileCacheOn = getenv("MONITOR_URL_FILE_CACHE_ON") ? atoi(getenv("MONITOR_URL_FILE_CACHE_ON")) : 0;
//-----------------------------------------------------

const uint64_t outputFileBufferSize = 16UL * 1024 * 1024;

const bool reuseFileCache = true; //josh, what is the point of a reusable cache if you dont reuse it!?!?! ;)
const bool bufferFileCacheWrites = true;

const unsigned int ReservationTimeOut = -1;

const std::string sharedMemName("/" + monitor_id + "ioCache");




const bool ThreadStats = getenv("MONITOR_THREAD_STATS") ? atoi(getenv("MONITOR_THREAD_STATS")) : 0;
const bool TrackBlockStats = false;
const bool TrackReads = false;
const unsigned int FdsPerLocalFile = 10;

const bool WriteFileLog = false;
const bool MonitorFileLog = false;
const bool CacheLog = false;
const bool ServerConLog = false;
const bool ClientConLog = false;
const bool ServeFileLog = false;
const bool CacheFileLog = false;
const bool FileCacheLog = false;
const bool PrefetcherLog = false;

//BM: turn on for Histogram trace
const bool TraceHistogram = getenv("MONITOR_TRACE_HISTOGRAM") ? atoi(getenv("MONITOR_TRACE_HISTOGRAM")) : 0;
//threshold percentage for UMB retention / eviction policy. given as a percentage
const double UMBThreshold = getenv("MONITOR_UMB_THRESHOLD") ? atoi(getenv("MONITOR_UMB_THRESHOLD")) : 0;

const uint64_t blockSizeForStat = 4096UL;
const uint64_t hashtableSizeForStat = 1000;
#define FILE_CACHE_WRITE_THROUGH 1

//#define PRINTF(...) fprintf(stderr, __VA_ARGS__)
#define PRINTF(...)

// Flow Monitoring Parameter
const std::string dataLifeOutputPath = getenv("DATALIFE_OUTPUT_PATH") ? std::string(getenv("DATALIFE_OUTPUT_PATH")) : "";
const std::string passin_patterns = getenv("DATALIFE_FILE_PATTERNS") ? std::string(getenv("DATALIFE_FILE_PATTERNS")) : "*.datalifetest";
const bool enableJsonOutput = getenv("DATALIFE_JSON_OUTPUT") ? atoi(getenv("DATALIFE_JSON_OUTPUT")) : 0;
const std::string task_name_env = getenv("DATALIFE_TASK_NAME") ? std::string(getenv("DATALIFE_TASK_NAME")) : "task_name";


} // namespace Config

#endif /* CONFIG_H */
