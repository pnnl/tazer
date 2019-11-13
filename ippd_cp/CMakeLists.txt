#I would not copy what I am doing here...
#Basically this file needs completely different compile flags
#and it is a pain to try to change them...
set(CMAKE_CXX_FLAGS "--std=c++14 -g -O3 -fopenmp")
#add_compile_options("-fsanitize=address" )
#add_compile_options("-fno-omit-frame-pointer")
#link_libraries("-fsanitize=address")
add_executable(ippd_cp ippd_cp.cpp)
install(TARGETS ippd_cp RUNTIME DESTINATION bin)