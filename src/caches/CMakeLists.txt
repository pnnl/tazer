#add_subdirectory (bounded)
#add_subdirectory (scalable)
#add_subdirectory (unbounded)

set(BOUNDED_INC_DIR ${TAZER_CACHE_INC_DIR}/bounded)
set(UNBOUNDED_INC_DIR ${TAZER_CACHE_INC_DIR}/unbounded)


set(CACHE_HEADERS
    ${TAZER_CACHE_INC_DIR}/Cache.h
    ${TAZER_CACHE_INC_DIR}/NetworkCache.h
    ${TAZER_CACHE_INC_DIR}/LocalFileCache.h
    ${TAZER_CACHE_INC_DIR}/FcntlReaderWriterLock.h
    ${TAZER_CACHE_INC_DIR}/CacheStats.h
    ${TAZER_CACHE_INC_DIR}/CacheTypes.h

    ${BOUNDED_INC_DIR}/BoundedCache.h
    ${BOUNDED_INC_DIR}/MemoryCache.h
    ${BOUNDED_INC_DIR}/SharedMemoryCache.h
    ${BOUNDED_INC_DIR}/FileCache.h
    ${BOUNDED_INC_DIR}/BoundedFilelockCache.h
    ${BOUNDED_INC_DIR}/BoundedLinkfileCache.h
    ${BOUNDED_INC_DIR}/FileLinkReaderWriterLock.h

    ${UNBOUNDED_INC_DIR}/UnboundedCache.h
    ${UNBOUNDED_INC_DIR}/FilelockCache.h
    ${UNBOUNDED_INC_DIR}/FcntlCache.h
)

set(CACHE_FILES
    Cache.cpp
    NetworkCache.cpp
    LocalFileCache.cpp
    FcntlReaderWriterLock.cpp
    CacheStats.cpp

    bounded/BoundedCache.cpp
    bounded/MemoryCache.cpp
    bounded/FileCache.cpp
    bounded/SharedMemoryCache.cpp
    bounded/BoundedFilelockCache.cpp
    bounded/BoundedLinkfileCache.cpp
    bounded/FileLinkReaderWriterLock.cpp

    unbounded/UnboundedCache.cpp
    unbounded/FilelockCache.cpp
    unbounded/FcntlCache.cpp
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
target_include_directories(caches PUBLIC ${TAZER_INC_DIR} ${TAZER_CACHE_INC_DIR} ${BOUNDED_INC_DIR} ${BOUNDED_INC_DIR}/deprecated ${UNBOUNDED_INC_DIR})
