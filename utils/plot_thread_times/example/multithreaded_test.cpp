#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <sys/stat.h>
#include <string>
#include <cstring>
#include <assert.h>

void task(int total_threads, int thread_num, char *in_metafile_path, char *out_metafile_path, char *test_directory_path) {
    //read a buffer of random data
    int mult = thread_num;
    while(mult > 10) {
        mult -= 10;
    }
    ssize_t buffer_size = 10000000 * mult;
    assert(buffer_size <= 100000000);
    //ssize_t buffer_size = 100000000;
    char *rand_buf = new char[buffer_size];
    int fd_random = open("/dev/random", O_RDONLY);
    read(fd_random, rand_buf, buffer_size);
    rand_buf[buffer_size-1] = '\0';
    close(fd_random);

    //read and write from local file
    std::string workspace = test_directory_path;
    workspace.append("/thread");
    workspace.append(std::to_string(thread_num));
    mkdir(workspace.c_str(), 0777);

    std::string file1 = workspace + "/file1";
    int fd = open(file1.c_str(), O_RDWR | O_CREAT, S_IRWXU | S_IRWXG);
    assert(fd > 0);

    //assert(write(fd, rand_buf, buffer_size) == buffer_size);
            int write_return = write(fd, rand_buf, buffer_size);
            if(write_return != buffer_size) {
                std::cout << "WRITE RETURNED: " << write_return << " Expected: " << buffer_size << std::endl;
            }
            assert(write_return > 0);

    assert(lseek(fd, 0, SEEK_SET) == 0);

    char *read_buf = new char[buffer_size];
    //assert(read(fd, read_buf, buffer_size) == buffer_size);
            assert(read(fd, read_buf, write_return) == buffer_size);
    assert(std::strcmp(rand_buf, read_buf) == 0);

    close(fd);
    delete read_buf;

    //read from a tazer file shared between threads
    int fd2 = open(in_metafile_path, O_RDONLY);
    assert(fd2 > 0);

    int file_size = lseek(fd2, 0, SEEK_END);
            if(file_size <= 0) {
                std::cout << "Error: file size = " << file_size << std::endl;
            }
    assert(file_size > 0);
    assert(lseek(fd2, 0, SEEK_SET) == 0);

    // if(file_size > 1000)
    //     file_size = 1000;

    file_size = file_size / mult;
    char* read_buf2 = new char[file_size];
    assert(read(fd2, read_buf2, file_size) == file_size);
    //std::cout << "read " << file_size << " bytes" << std::endl;

    close(fd2);

    //each thread writes to the same tazer file
    int fd3 = open(out_metafile_path, O_WRONLY | O_CREAT | O_APPEND);

    assert(fd3 > 0);

    // std::string tazer_write_buf = "thread";
    // tazer_write_buf.append(std::to_string(thread_num).c_str());
    // tazer_write_buf.append(read_buf2);
    // assert(write(fd3, tazer_write_buf.c_str(), tazer_write_buf.length()) == tazer_write_buf.length());

    assert(write(fd3, rand_buf, buffer_size) == buffer_size);

    close(fd3);

    delete[] read_buf2;
    return;
}

int main(int argc, char *args[]) {
    if(argc < 5) {
        std::cerr << "error: expected 4 args, (threads, input metafile path, output metafile path, test directory path)" << std::endl;
        return 1;
    }

    int num_threads = atoi(args[1]);
    char *in_metafile_path = args[2];
    char *out_metafile_path = args[3];
    char *test_dir_path = args[4];

    std::thread threads[num_threads];

    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread(task, num_threads, i+1, in_metafile_path, out_metafile_path, test_dir_path);
    }

    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    return 0;
}