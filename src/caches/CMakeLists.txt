#add_subdirectory (bounded)
#add_subdirectory (scalable)
#add_subdirectory (unbounded)

set(BOUNDED_INC_DIR ${TAZER_CACHE_INC_DIR}/bounded)
set(UNBOUNDED_INC_DIR ${TAZER_CACHE_INC_DIR}/unbounded)
set(SCALABLE_INC_DIR ${TAZER_CACHE_INC_DIR}/scalable)

set(CACHE_HEADERS
    ${TAZER_CACHE_INC_DIR}/Cache.h
    ${TAZER_CACHE_INC_DIR}/NetworkCache.h
    ${TAZER_CACHE_INC_DIR}/LocalFileCache.h
    ${TAZER_CACHE_INC_DIR}/FcntlReaderWriterLock.h
    ${TAZER_CACHE_INC_DIR}/CacheStats.h
    ${TAZER_CACHE_INC_DIR}/CacheTypes.h

    ${BOUNDED_INC_DIR}/NewBoundedCache.h
    ${BOUNDED_INC_DIR}/NewMemoryCache.h
    ${BOUNDED_INC_DIR}/NewSharedMemoryCache.h
    ${BOUNDED_INC_DIR}/NewBoundedFilelockCache.h
    ${BOUNDED_INC_DIR}/NewBoundedLinkfileCache.h
    ${BOUNDED_INC_DIR}/FileLinkReaderWriterLock.h
    ${BOUNDED_INC_DIR}/deprecated/BoundedCache.h
    ${BOUNDED_INC_DIR}/deprecated/FileCache.h
    ${BOUNDED_INC_DIR}/deprecated/BoundedFilelockCache.h
    ${BOUNDED_INC_DIR}/deprecated/MemoryCache.h
    ${BOUNDED_INC_DIR}/deprecated/SharedMemoryCache.h

    ${UNBOUNDED_INC_DIR}/UnboundedCache.h
    ${UNBOUNDED_INC_DIR}/FilelockCache.h
    ${UNBOUNDED_INC_DIR}/FcntlCache.h

    ${SCALABLE_INC_DIR}/ScalableMetaData.h
    ${SCALABLE_INC_DIR}/ScalableAllocator.h
    ${SCALABLE_INC_DIR}/ScalableCache.h
)

set(CACHE_FILES
    Cache.cpp
    NetworkCache.cpp
    LocalFileCache.cpp
    FcntlReaderWriterLock.cpp
    CacheStats.cpp

    bounded/NewBoundedCache.cpp
    bounded/NewMemoryCache.cpp
    bounded/NewSharedMemoryCache.cpp
    bounded/NewBoundedFilelockCache.cpp
    bounded/NewBoundedLinkfileCache.cpp
    bounded/FileLinkReaderWriterLock.cpp
    bounded/deprecated/BoundedCache.cpp
    bounded/deprecated/FileCache.cpp
    bounded/deprecated/BoundedFilelockCache.cpp
    bounded/deprecated/MemoryCache.cpp
    bounded/deprecated/SharedMemoryCache.cpp

    unbounded/UnboundedCache.cpp
    unbounded/FilelockCache.cpp
    unbounded/FcntlCache.cpp

    scalable/ScalableMetaData.cpp
    scalable/ScalableAllocator.cpp
    scalable/ScalableCache.cpp
)

if(CURL_LIB)
    set(CACHE_HEADERS ${CACHE_HEADERS} 
        ${TAZER_CACHE_INC_DIR}/UrlDownload.h 
        ${TAZER_CACHE_INC_DIR}/UrlFileCache.h
        ${UNBOUNDED_INC_DIR}/UrlCache.h
    )
    set(CACHE_FILES ${CACHE_FILES} 
        UrlDownload.cpp 
        UrlFileCache.cpp
        unbounded/UrlCache.cpp
    )
endif()

add_library(caches OBJECT ${CACHE_HEADERS} ${CACHE_FILES})
target_include_directories(caches PUBLIC ${TAZER_INC_DIR} ${TAZER_CACHE_INC_DIR} ${BOUNDED_INC_DIR} ${BOUNDED_INC_DIR}/deprecated ${UNBOUNDED_INC_DIR} ${SCALABLE_INC_DIR})
