configure_file("server_test_file.txt.meta.in" "${CMAKE_BINARY_DIR}/inputs/server_test_file.txt.meta.in")
configure_file("client_test_file.txt.meta.out" "${CMAKE_BINARY_DIR}/inputs/client_test_file.txt.meta.out")
configure_file("warAndPeace.txt.meta.in" "${CMAKE_BINARY_DIR}/inputs/warAndPeace.txt.meta.in")
configure_file("dictionary.txt.meta.in"
"${CMAKE_BINARY_DIR}/inputs/dictionary.txt.meta.in")
file(COPY "client_test_file.txt" DESTINATION "${CMAKE_BINARY_DIR}/inputs/")
file(COPY "warAndPeace.txt" DESTINATION "${CMAKE_BINARY_DIR}/inputs/")
