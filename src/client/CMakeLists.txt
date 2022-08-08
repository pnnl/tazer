SET(TAZER_CLIENT_INC_DIR ${TAZER_INC_DIR}/client)
set(CLIENT_HEADERS
    ${TAZER_CLIENT_INC_DIR}/TazerFile.h
    ${TAZER_CLIENT_INC_DIR}/InputFile.h
    ${TAZER_CLIENT_INC_DIR}/OutputFileInner.h
    ${TAZER_CLIENT_INC_DIR}/OutputFile.h
    ${TAZER_CLIENT_INC_DIR}/LocalFile.h
    ${TAZER_CLIENT_INC_DIR}/TrackFile.h
    ${TAZER_CLIENT_INC_DIR}/TazerFileDescriptor.h
    ${TAZER_CLIENT_INC_DIR}/TazerFileStream.h
)

set(CLIENT_FILES
    TazerFile.cpp
    InputFile.cpp
    OutputFileInner.cpp
    OutputFile.cpp
    LocalFile.cpp
    TrackFile.cpp
    TazerFileDescriptor.cpp
    TazerFileStream.cpp
    Lib.cpp
)

add_library(client SHARED ${CLIENT_HEADERS} ${CLIENT_FILES} $<TARGET_OBJECTS:common> $<TARGET_OBJECTS:caches> $<TARGET_OBJECTS:prefetch>)
target_link_libraries(client ${RDMACM_LIB} ${RT_LIB} ${CURL_LIB} ${SSL_LIB} stdc++fs) 
target_include_directories(client PUBLIC ${TAZER_INC_DIR} ${TAZER_CLIENT_INC_DIR} ${TAZER_CACHE_INC_DIR} ${TAZER_PREFETCH_INC_DIR})
install(TARGETS client LIBRARY DESTINATION lib)
 
