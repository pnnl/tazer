set (TAZER_SERVER_INC_DIR ${TAZER_INC_DIR}/server)
set(SERVER_HEADERS
    ${TAZER_SERVER_INC_DIR}/ServeFile.h
)

set(SERVER_FILES
    ServeFile.cpp
)

add_executable(server Server.cpp ${SERVER_HEADERS} ${SERVER_FILES} $<TARGET_OBJECTS:common> $<TARGET_OBJECTS:caches>)

target_link_libraries(server ${RDMACM_LIB} ${RT_LIB} ${CURL_LIB} ${SSL_LIB} stdc++fs)
set_target_properties(server PROPERTIES LINK_SEARCH_START_STATIC 1)
set_target_properties(server PROPERTIES LINK_SEARCH_END_STATIC 1)

target_include_directories(server PUBLIC ${TAZER_INC_DIR} ${TAZER_SERVER_INC_DIR} ${TAZER_CACHE_INC_DIR})

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
 
install(TARGETS server RUNTIME DESTINATION bin)
