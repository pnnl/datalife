# -*-Mode: cmake;-*-

#*BeginPNNLCopyright*********************************************************
#
# $HeadURL$
# $Id$
#
#***********************************************************EndPNNLCopyright*

#****************************************************************************
# 
#****************************************************************************

SET(MONITOR_INC_DIR ${MONITOR_INC_DIR})
set(MONITOR_HEADERS
    ${MONITOR_INC_DIR}/TrackFile.h
    ${MONITOR_INC_DIR}/AtomicHelper.h
    ${MONITOR_INC_DIR}/FcntlReaderWriterLock.h
    ${MONITOR_INC_DIR}/ThreadPool.h
    ${MONITOR_INC_DIR}/BoundedCache.h
    ${MONITOR_INC_DIR}/FileCache.h
    ${MONITOR_INC_DIR}/Timer.h
    ${MONITOR_INC_DIR}/Histogram.h
    ${MONITOR_INC_DIR}/ScalableCache.h
    ${MONITOR_INC_DIR}/ScalableMetaData.h
    ${MONITOR_INC_DIR}/ScalableAllocator.h
    ${MONITOR_INC_DIR}/FileCacheRegister.h
    ${MONITOR_INC_DIR}/PerfectPrefetcher.h
    ${MONITOR_INC_DIR}/Trackable.h
    ${MONITOR_INC_DIR}/Cache.h
    ${MONITOR_INC_DIR}/Prefetcher.h
    ${MONITOR_INC_DIR}/UnboundedCache.h
    ${MONITOR_INC_DIR}/CacheStats.h
    ${MONITOR_INC_DIR}/FilelockCache.h
    ${MONITOR_INC_DIR}/UnixIO.h
    ${MONITOR_INC_DIR}/CacheTypes.h
    ${MONITOR_INC_DIR}/Config.h
    ${MONITOR_INC_DIR}/Loggable.h
    ${MONITOR_INC_DIR}/PriorityThreadPool.h
    ${MONITOR_INC_DIR}/Connection.h
    #${MONITOR_INC_DIR}/MemoryCache.h
    ${MONITOR_INC_DIR}/RSocketAdapter.h
    ${MONITOR_INC_DIR}/ConnectionPool.h
    ${MONITOR_INC_DIR}/Message.h
    ${MONITOR_INC_DIR}/ReaderWriterLock.h
    ${MONITOR_INC_DIR}/DeltaPrefetcher.h
    ${MONITOR_INC_DIR}/NetworkCache.h
    ${MONITOR_INC_DIR}/lz4.h
    ${MONITOR_INC_DIR}/ReaderWriterLock.h
    ${MONITOR_INC_DIR}/lz4hc.h
    ${MONITOR_INC_DIR}/FcntlCache.h
    #${MONITOR_INC_DIR}/SharedMemoryCache.h
    ${MONITOR_INC_DIR}/xxhash.h
)

set(MONITOR_FILES
    #InputFile.cpp
    #OutputFileInner.cpp
    #OutputFile.cpp
    #LocalFile.cpp
    TrackFile.cpp
    MonitorFileStream.cpp
    CacheStats.cpp
    Lib.cpp
    PriorityThreadPool.cpp
    RSocketAdapter.cpp
    ThreadPool.cpp
    ReaderWriterLock.cpp
    Timer.cpp
    Connection.cpp
    Loggable.cpp
    Request.cpp
    ConnectionPool.cpp
    MonitorFile.cpp
    FileCacheRegister.cpp
    #MemoryCache.cpp
    #BoundedFilelockCache.cpp
    Cache.cpp
    Prefetcher.cpp
    MonitorFileDescriptor.cpp
)

add_library(monitor SHARED ${MONITOR_HEADERS} ${MONITOR_FILES})
#target_link_libraries(monitor ${RDMACM_LIB} ${RT_LIB} ${CURL_LIB} ${SSL_LIB} stdc++fs) 
target_link_libraries(monitor stdc++fs) 
target_include_directories(monitor PUBLIC ${MONITOR_INC_DIR})
install(TARGETS monitor LIBRARY DESTINATION lib)
 
install(FILES ../datalife-run 
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	DESTINATION bin)
