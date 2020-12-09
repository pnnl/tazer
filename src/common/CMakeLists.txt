set(COMMON_HEADERS
    ${TAZER_INC_DIR}/Config.h
    ${TAZER_INC_DIR}/AtomicHelper.h
    ${TAZER_INC_DIR}/UnixIO.h
    ${TAZER_INC_DIR}/Loggable.h
    ${TAZER_INC_DIR}/Trackable.h
    ${TAZER_INC_DIR}/Connection.h
    ${TAZER_INC_DIR}/Message.h
    ${TAZER_INC_DIR}/ThreadPool.h
    ${TAZER_INC_DIR}/PriorityThreadPool.h
#    ${TAZER_INC_DIR}/PriorityPool.h
    ${TAZER_INC_DIR}/ReaderWriterLock.h    
    ${TAZER_INC_DIR}/lz4.h
    ${TAZER_INC_DIR}/lz4hc.h
    ${TAZER_INC_DIR}/lz4opt.h
    ${TAZER_INC_DIR}/xxhash.h
    ${TAZER_INC_DIR}/Timer.h
    ${TAZER_INC_DIR}/FileCacheRegister.h
    ${TAZER_INC_DIR}/RSocketAdapter.h
    ${TAZER_INC_DIR}/ConnectionPool.h
    ${TAZER_INC_DIR}/Request.h
)

set(COMMON_FILES
    Connection.cpp
    Message.cpp
    ThreadPool.cpp
    PriorityThreadPool.cpp
#    PriorityPool.cpp
    ReaderWriterLock.cpp
    
    lz4.c
    lz4hc.c
    xxhash.c
    Timer.cpp
    FileCacheRegister.cpp
    RSocketAdapter.cpp
    ConnectionPool.cpp
    Loggable.cpp
    Request.cpp
)

add_library(common OBJECT ${COMMON_HEADERS} ${COMMON_FILES})
add_library(threadPool SHARED ThreadPool.cpp PriorityThreadPool.cpp Timer.cpp ReaderWriterLock.cpp Loggable.cpp Request.cpp)
target_include_directories(common PUBLIC ${TAZER_INC_DIR} ${TAZER_INC_DIR}/caches)
target_include_directories(threadPool PUBLIC  ${TAZER_INC_DIR} ${TAZER_INC_DIR}/caches)
#target_link_libraries(threadPool ${RDMACM_LIB} ${RT_LIB} ${CURL_LIB} ${SSL_LIB} stdc++fs)
