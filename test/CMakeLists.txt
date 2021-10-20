include_directories(${TAZER_INC_DIR}  ${TAZER_CACHE_INC_DIR})
add_library(testLib SHARED $<TARGET_OBJECTS:common> $<TARGET_OBJECTS:caches>)
target_link_libraries(testLib ${RDMACM_LIB} ${RT_LIB} ${CURL_LIB} ${SSL_LIB} stdc++fs)

add_executable(ServerTest ServerTest.cpp)
target_link_libraries(ServerTest testLib)

add_executable(InputFileTest InputFileTest.cpp)
target_link_libraries(InputFileTest client)

add_executable(OutputFileTest OutputFileTest.cpp)
target_link_libraries(OutputFileTest client)

add_executable(ConnectionTest ConnectionTest.cpp)
target_link_libraries(ConnectionTest testLib)

add_executable(CloseServer CloseServer.cpp)
target_link_libraries(CloseServer testLib)

add_executable(PingServer PingServer.cpp)
target_link_libraries(PingServer testLib)

add_executable(LibTest LibTest.cpp)
target_link_libraries(LibTest)

add_executable(OpenTest OpenTest.cpp)
target_link_libraries(OpenTest testLib)

add_executable(FileSizeTest FileSizeTest.cpp)
target_link_libraries(FileSizeTest testLib)

add_executable(SeekTest SeekTest.cpp)
target_link_libraries(SeekTest testLib)

# add_executable(ErrorTest ErrorTest.cpp)
# target_link_libraries(ErrorTest testLib)

add_executable(CopyTest CopyTest.cpp)
target_link_libraries(CopyTest testLib)

add_executable(MapTest MapTest.cpp)
target_link_libraries(MapTest testLib)

add_executable(MapTest2 MapTest2.cpp)
target_link_libraries(MapTest2 testLib)

add_executable(TazerCp TazerCp.cpp )
target_link_libraries(TazerCp threadPool)

configure_file(runTest.pl ${CMAKE_BINARY_DIR}/test/runTest.pl @ONLY)

configure_file(runTest2.pl ${CMAKE_BINARY_DIR}/test/runTest2.pl @ONLY)

add_executable(RSocketServerTest RSocketServerTest.cpp)
target_link_libraries(RSocketServerTest testLib)

add_executable(RSocketClientTest RSocketClientTest.cpp)
target_link_libraries(RSocketClientTest testLib)

#add_executable(FileCacheTest FileCacheTest.cpp)
#target_link_libraries(FileCacheTest testLib)

#add_executable(BurstBufferCacheTest BurstBufferCacheTest.cpp)
#target_link_libraries(BurstBufferCacheTest testLib)

add_executable(FileCacheRegisterTest FileCacheRegisterTest.cpp)
target_link_libraries(FileCacheRegisterTest testLib)

add_executable(StreamTest StreamTest.cpp)
add_executable(BufferIOTest BufferIOTest.cpp)

configure_file(FileIOTest.py ${CMAKE_BINARY_DIR}/test/FileIOTest.py @ONLY)

configure_file(ConvertToRecords.py ${CMAKE_BINARY_DIR}/test/ConverToRecords.py @ONLY)

add_executable(PriorityThreadPoolTest PriorityThreadPoolTest.cpp)
target_link_libraries(PriorityThreadPoolTest testLib)

add_executable(CheckCp CheckCp.cpp )
target_link_libraries(CheckCp threadPool)

if(CURL_LIB)
    add_executable(SimpleUrlTest SimpleUrlTest.cpp)
    target_link_libraries(SimpleUrlTest testLib )

    add_executable(CopyRangeTest CopyRangeTest.cpp)
    target_link_libraries(CopyRangeTest testLib)
endif()

configure_file(FileStressTest.sh ${CMAKE_BINARY_DIR}/test/FileStressTest.sh @ONLY)

add_executable(HistTest HistTest.cpp )
target_link_libraries(HistTest testLib)