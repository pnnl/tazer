set(COMMON_HEADERS
    ${CMAKE_SOURCE_DIR}/inc/Config.h
    ${CMAKE_SOURCE_DIR}/inc/AtomicHelper.h
    ${CMAKE_SOURCE_DIR}/inc/UnixIO.h
    ${CMAKE_SOURCE_DIR}/inc/Loggable.h
    ${CMAKE_SOURCE_DIR}/inc/Trackable.h
    ${CMAKE_SOURCE_DIR}/inc/Connection.h
    ${CMAKE_SOURCE_DIR}/inc/Message.h
    ${CMAKE_SOURCE_DIR}/inc/ThreadPool.h
    ${CMAKE_SOURCE_DIR}/inc/PriorityThreadPool.h
#    ${CMAKE_SOURCE_DIR}/inc/PriorityPool.h
    ${CMAKE_SOURCE_DIR}/inc/ReaderWriterLock.h
    ${CMAKE_SOURCE_DIR}/inc/FSReaderWriterLock.h
    ${CMAKE_SOURCE_DIR}/inc/FcntlReaderWriterLock.h
    ${CMAKE_SOURCE_DIR}/inc/lz4.h
    ${CMAKE_SOURCE_DIR}/inc/lz4hc.h
    ${CMAKE_SOURCE_DIR}/inc/lz4opt.h
    ${CMAKE_SOURCE_DIR}/inc/xxhash.h
    ${CMAKE_SOURCE_DIR}/inc/Timer.h
#    ${CMAKE_SOURCE_DIR}/inc/ErrorTester.h
    ${CMAKE_SOURCE_DIR}/inc/FileCacheRegister.h
    ${CMAKE_SOURCE_DIR}/inc/RSocketAdapter.h
    ${CMAKE_SOURCE_DIR}/inc/ConnectionPool.h
    ${CMAKE_SOURCE_DIR}/inc/Request.h
    ${CMAKE_SOURCE_DIR}/inc/BlockCache.h
    ${CMAKE_SOURCE_DIR}/inc/Cache.h
    ${CMAKE_SOURCE_DIR}/inc/BoundedCache.h
    ${CMAKE_SOURCE_DIR}/inc/UnboundedCache.h
    ${CMAKE_SOURCE_DIR}/inc/FileCache.h
    ${CMAKE_SOURCE_DIR}/inc/FilelockCache.h
    ${CMAKE_SOURCE_DIR}/inc/BoundedFilelockCache.h
    ${CMAKE_SOURCE_DIR}/inc/FcntlCache.h
    ${CMAKE_SOURCE_DIR}/inc/MemoryCache.h
    ${CMAKE_SOURCE_DIR}/inc/SharedMemoryCache.h
    ${CMAKE_SOURCE_DIR}/inc/NetworkCache.h
    ${CMAKE_SOURCE_DIR}/inc/LocalFileCache.h
    #${CMAKE_SOURCE_DIR}/inc/BlockSizeTranslationCache.h
    ${CMAKE_SOURCE_DIR}/inc/CacheStats.h
    ${CMAKE_SOURCE_DIR}/inc/CacheTypes.h
    ${CMAKE_SOURCE_DIR}/inc/Prefetcher.h
    ${CMAKE_SOURCE_DIR}/inc/DeltaPrefetcher.h
    ${CMAKE_SOURCE_DIR}/inc/PerfectPrefetcher.h
    ${CMAKE_SOURCE_DIR}/inc/Request.h
    ${CMAKE_SOURCE_DIR}/inc/FileLinkReaderWriterLock.h
    ${CMAKE_SOURCE_DIR}/inc/NewBoundedCache.h
    ${CMAKE_SOURCE_DIR}/inc/NewMemoryCache.h
    ${CMAKE_SOURCE_DIR}/inc/NewSharedMemoryCache.h
    ${CMAKE_SOURCE_DIR}/inc/NewBoundedFilelockCache.h
    ${CMAKE_SOURCE_DIR}/inc/NewBoundedLinkfileCache.h

)

set(COMMON_FILES
    Connection.cpp
    Message.cpp
    ThreadPool.cpp
    PriorityThreadPool.cpp
#    PriorityPool.cpp
    ReaderWriterLock.cpp
    FSReaderWriterLock.cpp
    FcntlReaderWriterLock.cpp
#    ippd_comp.c
#    ippd_decomp.c
    lz4.c
    lz4hc.c
    xxhash.c
    Timer.cpp
#    ErrorTester.cpp
    FileCacheRegister.cpp
    RSocketAdapter.cpp
    ConnectionPool.cpp
    BlockCache.cpp
    Cache.cpp
    BoundedCache.cpp
    UnboundedCache.cpp
    FileCache.cpp
    FilelockCache.cpp
    BoundedFilelockCache.cpp
    FcntlCache.cpp
    MemoryCache.cpp
    SharedMemoryCache.cpp
    NetworkCache.cpp
    LocalFileCache.cpp
#    BlockSizeTranslationCache.cpp
    CacheStats.cpp
    Loggable.cpp
    Prefetcher.cpp
    DeltaPrefetcher.cpp
    PerfectPrefetcher.cpp
    Request.cpp
    FileLinkReaderWriterLock.cpp
    NewBoundedCache.cpp
    NewMemoryCache.cpp
    NewSharedMemoryCache.cpp
    NewBoundedFilelockCache.cpp
    NewBoundedLinkfileCache.cpp
)

if(CURL_LIB)
    set(COMMON_HEADERS ${COMMON_HEADERS} ${CMAKE_SOURCE_DIR}/inc/UrlDownload.h 
                                         ${CMAKE_SOURCE_DIR}/inc/UrlFileCache.h
                                         ${CMAKE_SOURCE_DIR}/inc/UrlCache.h)
    set(COMMON_FILES ${COMMON_FILES} UrlDownload.cpp UrlFileCache.cpp UrlCache.cpp)
endif()

add_library(common OBJECT ${COMMON_HEADERS} ${COMMON_FILES})
add_library(threadPool SHARED ThreadPool.cpp PriorityThreadPool.cpp Timer.cpp ReaderWriterLock.cpp FSReaderWriterLock.cpp Loggable.cpp Request.cpp)
#target_link_libraries(threadPool ${RDMACM_LIB} ${RT_LIB} ${CURL_LIB} ${SSL_LIB} stdc++fs)
