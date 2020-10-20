set(COMMON_HEADERS
    ${CMAKE_SOURCE_DIR}/inc/ServeFile.h
)

set(SERVER_FILES
    ServeFile.cpp
)

add_library(serverLib ${SERVER_FILES} $<TARGET_OBJECTS:common>)

add_executable(server Server.cpp)
target_link_libraries(server serverLib ${RDMACM_LIB} ${RT_LIB} ${CURL_LIB} ${SSL_LIB} stdc++fs)
set_target_properties(server PROPERTIES LINK_SEARCH_START_STATIC 1)
set_target_properties(server PROPERTIES LINK_SEARCH_END_STATIC 1)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

install(TARGETS server RUNTIME DESTINATION bin)
