set(CLIENT_HEADERS
    ${CMAKE_SOURCE_DIR}/inc/TazerFile.h
    ${CMAKE_SOURCE_DIR}/inc/InputFile.h
    ${CMAKE_SOURCE_DIR}/inc/OutputFile.h
    ${CMAKE_SOURCE_DIR}/inc/LocalFile.h
    ${CMAKE_SOURCE_DIR}/inc/TazerFileDescriptor.h
    ${CMAKE_SOURCE_DIR}/inc/TazerFileStream.h
)

set(CLIENT_FILES
    TazerFile.cpp
    InputFile.cpp
    OutputFile.cpp
    LocalFile.cpp
    TazerFileDescriptor.cpp
    TazerFileStream.cpp
    Lib.cpp
)

add_library(client SHARED ${CLIENT_HEADERS} ${CLIENT_FILES} $<TARGET_OBJECTS:common>)
target_link_libraries(client ${RDMACM_LIB} ${RT_LIB} ${CURL_LIB} ${SSL_LIB} stdc++fs) 
install(TARGETS client LIBRARY DESTINATION lib)
