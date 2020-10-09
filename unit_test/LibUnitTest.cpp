//LibUnitTest.cpp tests the functions found in Lib.cpp using the catch2 unit testing framework
//There is a TEST_CASE() for each function. Every REQUIRE() in a test case must evaluate to true for the test case to pass.
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <fcntl.h>
#include "Lib.h"

TEST_CASE("removeStr test", "[Lib.cpp]") {
    char a[10];
    std::string b = "abcxyzdef";
    strcpy(a, b.c_str());
    REQUIRE( removeStr(a, "xyz") == 0 );

    b = "abcdefg";
    strcpy(a, b.c_str());
    REQUIRE( removeStr(a, "xyz") == -1 );
}

TEST_CASE("splitter test", "[Lib.cpp]") {
    std::string full[3] = {"path/directory/file.meta.in", "path/directory/file.meta.in/extra", "path/directory/file.meta.out"};
    std::string path[3];
    std::string file[3];
    std::string tok(".meta.in");
    REQUIRE(splitter(tok, full[0], path[0], file[0]) == true);
    REQUIRE((path[0] == full[0] && file[0] == full[0]) == true);

    REQUIRE(splitter(tok, full[1], path[1], file[1]) == true);
    REQUIRE((path[1] == "path/directory/file.meta.in" && file[1] == "extra") == true);

    REQUIRE(splitter(tok, full[2], path[2], file[2]) == false);
    REQUIRE((path[2] == full[2] && file[2] == full[2]) == true);
}

TEST_CASE("checkMeta test", "[Lib.cpp]") {
    std::string path;
    std::string file;
    TazerFile::Type type;
    REQUIRE(checkMeta("path/directory/file.meta.in", path, file, type) == true);
    REQUIRE(type == TazerFile::Input);
    REQUIRE(checkMeta("path/directory/file.meta.out", path, file, type) == true);
    REQUIRE(type == TazerFile::Output);
    REQUIRE(checkMeta("path/directory/file.meta.local", path, file, type) == true);
    REQUIRE(type == TazerFile::Local);

    REQUIRE(checkMeta("path/directory/file.notmeta", path, file, type) == false);
}

TEST_CASE("trackFile test", "[Lib.cpp]") {
    int fd = 0;
    FILE *file = NULL;
    REQUIRE(trackFile("local_data/fakefile") == 0);
    REQUIRE(trackFile(fd) == 0);
    REQUIRE(trackFile(file) == 0);

    fd = open("local_data/local1GB.dat",O_RDONLY);
    file = fopen("local_data/local1GB.dat", "r");
    addToSet(track_fp, file, unixfopen);

    REQUIRE(trackFile("local_data/local1GB.dat") == 1);
    REQUIRE(trackFile("local_data/local1GB.dat") == 1);
    REQUIRE(trackFile(file) == 1);

    close(fd);
    removeFromSet(track_fp, file, unixfclose);
    fclose(file);
}

TEST_CASE("ignoreFile test","[Lib.cpp]") {
    int fd = open("local_data/local1GB.dat",O_RDONLY);
    FILE *file = fopen("local_data/local1GB.dat", "r");
    addToSet(ignore_fd, fd, unixopen);
    addToSet(ignore_fp, file, unixfopen);

    REQUIRE(ignoreFile(fd) == 1);
    REQUIRE(ignoreFile(file) == 1);
    REQUIRE(ignoreFile(-1) == 0);
    FILE *fp = NULL;
    REQUIRE(ignoreFile(fp) == 0);

    removeFromSet(ignore_fd, fd, unixclose);
    removeFromSet(ignore_fp, file, unixfclose);
    close(fd);
    fclose(file);
}

TEST_CASE("removeFileStream test", "[Lib.cpp]") {
    FILE *fp;
    FILE *fp2;
    ReaderWriterLock *lock;
    ReaderWriterLock *lock2;
    TazerFileStream::addStream(fp, 1);
    TazerFileStream::addStream(fp2, 2);

    REQUIRE_NOTHROW(removeFileStream(unixfclose, fp));
    REQUIRE(TazerFileStream::lookupStream(fp, lock) == -1);
    REQUIRE_NOTHROW(removeFileStream(fclose, fp2));
    REQUIRE(TazerFileStream::lookupStream(fp2, lock2) == 2);
    REQUIRE_NOTHROW(removeFileStream(unixfclose, fp2));
    REQUIRE(TazerFileStream::lookupStream(fp2, lock2) == -1);
}

TEST_CASE("innerWrapper test", "[Lib.cpp]") {
    std::string pathname[3] = {"local_data/local1GB.dat", "local_data/tazer1GB.dat.meta.in", "local_data/fake.dat"};
    std::string tazerFile = "tazer1GB.dat";
    int mode = 0;
    int flags = O_RDONLY;
    bool isTazerFile = false;
    int fd[3] = {0,0,0};
    fd[0] = innerWrapper(pathname[0].c_str(), isTazerFile, tazerOpen, unixopen, pathname[0].c_str(), flags, mode);
    REQUIRE(fd[0] > 0);
    REQUIRE(isTazerFile == false);
    fd[1] = innerWrapper(pathname[1].c_str(), isTazerFile, tazerOpen, unixopen, pathname[1].c_str(), flags, mode);
    REQUIRE(fd[1] > 0);
    REQUIRE(isTazerFile == true);
    fd[2] = innerWrapper(pathname[2].c_str(), isTazerFile, tazerOpen, unixopen, pathname[2].c_str(), flags, mode);
    REQUIRE(fd[2] == -1);

    isTazerFile = false;
    unsigned int fp = 0;
    REQUIRE(innerWrapper(fd[0], isTazerFile, tazerClose, unixclose, fd[0]) == 0);
    REQUIRE(isTazerFile == false);

    isTazerFile = false;
    fp = 0;
    REQUIRE(innerWrapper(fd[1], isTazerFile, tazerClose, unixclose, fd[1]) == 0);
    REQUIRE(isTazerFile == true);

    fp = 0;
    REQUIRE(innerWrapper(fd[2], isTazerFile, tazerClose, unixclose, fd[2]) == -1);

    isTazerFile =false;
    FILE *file = unixfopen(pathname[0].c_str(), "r");
    FILE *file2 = unixfopen(pathname[1].c_str(), "r");
    REQUIRE(innerWrapper(file, isTazerFile, tazerFclose, unixfclose, file) == 0);
    REQUIRE(innerWrapper(file2, isTazerFile, tazerFclose, unixfclose, file2) == 0);
}

TEST_CASE("addToSet test", "[Lib.cpp]") {
    std::unordered_set<int> set;
    std::unordered_set<FILE *> set2;

    unixopen_t fake = NULL;
    unixfopen_t fake2 = NULL;

    REQUIRE_NOTHROW(addToSet(set, 1, unixopen));
    REQUIRE(set.count(1) == 1);

    REQUIRE_NOTHROW(addToSet(set, 2, unixopen64));
    REQUIRE(set.count(2) == 1);

    REQUIRE_NOTHROW(addToSet(set, 3, fake));
    REQUIRE(set.count(3) == 0);

    FILE *fp = fopen("local_data/local1GB.dat", "r");

    REQUIRE_NOTHROW(addToSet(set2, fp, fake2));
    REQUIRE(set2.count(fp) == 0);

    REQUIRE_NOTHROW(addToSet(set2, fp, unixfopen));
    REQUIRE(set2.count(fp) == 1);

    fclose(fp);
}

TEST_CASE("outerWrapper test","[Lib.cpp]") {
    std::string pathname[3] = {"local_data/local1GB.dat","local_data/tazer1GB.dat.meta.in","local_data/fake"};
    int flags[3] = {O_RDONLY, O_WRONLY, O_RDWR};
    Timer::Metric metric; 
    int mode = 0;
    int i, j, fd;
    for(i = 0; i < 2; i++) {
        for(j = 0; j < 3; j++) {
            metric = (flags[j] & O_WRONLY || flags[j] & O_RDWR) ? Timer::Metric::out_open : Timer::Metric::in_open;
            int fd = outerWrapper("open", pathname[i].c_str(), metric, tazerOpen, unixopen, pathname[i].c_str(), flags[j], mode);
            REQUIRE(fd > 0);
            vLock.readerLock();
            REQUIRE(outerWrapper("lseek", fd, Timer::Metric::seek, tazerLseek<off_t>, unixlseek, fd, 10, SEEK_SET) == 10);
            vLock.readerUnlock();
            REQUIRE(outerWrapper("close", fd, Timer::Metric::close, tazerClose, unixclose, fd) == 0);
        }
    }
    
    fd = outerWrapper("open", pathname[i].c_str(), metric, tazerOpen, unixopen, pathname[i].c_str(), flags[2], mode);
    REQUIRE(fd == -1);
    vLock.readerLock();
    REQUIRE(outerWrapper("lseek", fd, Timer::Metric::seek, tazerLseek<off_t>, unixlseek, fd, 10, SEEK_SET) == -1);
    vLock.readerUnlock();
    REQUIRE(outerWrapper("close", fd, Timer::Metric::close, tazerClose, unixclose, fd) == -1);
}

TEST_CASE("tazerOpen test", "[Lib.cpp]") {
    int fd = 0;
    std::string name[2] = {"tazer1GB.dat", "fake"};
    std::string metaName[2] = {"local_data/tazer1GB.dat.meta.in", "local_data/fake.meta.in"};
    TazerFile::Type type = TazerFile::Input;
    REQUIRE(tazerOpen(name[0], metaName[1], type, "", 0, 0) == -1);
    REQUIRE(tazerOpen(name[1], metaName[1], type, "", 0, 0) == -1);
    fd = tazerOpen(name[0], metaName[0], type, "", 0, 0);
    REQUIRE(fd > 0);
    close(fd);
    fd = tazerOpen(name[1], metaName[0], type, "", 0, 0);
    REQUIRE(fd > 0);
    close(fd);
}

TEST_CASE("open test", "[Lib.cpp]") {
    int temp[3] = {O_RDONLY, O_WRONLY, O_RDWR};
    int fd = 0;
    for (int i = 0; i < 3; i++)
    {
        fd = open("local_data/local1GB.dat" ,temp[i]);
        REQUIRE(fd > 0);
        close(fd);
        REQUIRE(open("local_data/local1GB.fake" ,temp[i]) == -1);
        fd = open("local_data/tazer1GB.dat.meta.in" ,temp[i]);
        REQUIRE(fd > 0);
        close(fd);
        REQUIRE(open("local_data/tazer1GB.fake.meta.in" ,temp[i]) == -1);
    }
}

TEST_CASE("open64 test", "[Lib.cpp]") {
    int temp[3] = {O_RDONLY, O_WRONLY, O_RDWR};
    int fd = 0;
    for (int i = 0; i < 3; i++)
    {
        fd = open64("local_data/local1GB.dat" ,temp[i]);
        REQUIRE(fd > 0);
        close(fd);
        REQUIRE(open64("local_data/local1GB.fake" ,temp[i]) == -1);
        fd = open64("local_data/tazer1GB.dat.meta.in" ,temp[i]);
        REQUIRE(fd > 0);
        close(fd);
        REQUIRE(open64("local_data/tazer1GB.fake.meta.in" ,temp[i]) == -1);
    }
}

TEST_CASE("tazerClose test", "[Lib.cpp]") {
    std::string name = "tazer1GB.dat";
    std::string metaName = "local_data/tazer1GB.dat.meta.in";
    TazerFile::Type type = TazerFile::Input;
    int fd = tazerOpen(name, metaName, type, "", 0, 0);
    TazerFile *file = NULL;
    unsigned int fp = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, fp);

    REQUIRE(tazerClose(file, fp, fd) == 0);
    REQUIRE(tazerClose(file, fp, fd) == -1);
    fd = tazerOpen(name, "local_data/fake.dat.meta.in", type, "", 0, 0);
    fp = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, fp);
    REQUIRE(tazerClose(file, fp, fd) == -1);
}

TEST_CASE("close test", "[Lib.cpp]") {
    int fd = open("local_data/local1GB.dat", O_RDONLY);
    int fd2 = open("local_data/tazer1GB.dat.meta.in", O_RDONLY);
    int fd3 = open("local_data/fake", O_RDONLY);
    REQUIRE(close(fd) == 0);
    REQUIRE(close(fd2) == 0);
    REQUIRE(close(fd3) == -1);
}

TEST_CASE("tazerRead test","[Lib.cpp]") {
    int fd = tazerOpen("tazer1GB.dat","local_data/tazer1GB.dat.meta.in",TazerFile::Input,"",0,0);
    TazerFile *file = NULL;
    char *buff[10];
    unsigned int fp = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, fp);
    REQUIRE(tazerRead(file, fp, fd, buff, 10) == 10);
    char *buff2[10];
    int fd2 = open("tazer_data/tazer1GB.dat", O_RDONLY);
    unixread(fd2, buff2, 10);
    REQUIRE(memcmp(buff, buff2, 10) == 0);
    close(fd2);
    close(fd);
}

TEST_CASE("read test", "[Lib.cpp]") {
    int fd = open("local_data/local1GB.dat", O_RDONLY);
    int fd2 = open("local_data/tazer1GB.dat.meta.in", O_RDONLY);
    char *buff[10];
    REQUIRE(read(fd, buff, 10) == 10);
    REQUIRE(read(fd2, buff, 10) == 10);
    close(fd);
    close(fd2);
}

TEST_CASE("tazerWrite test", "[Lib.cpp]") {
    char *buff[10];
    memcpy(buff, "thisistext", 10);
    int fd = open("local_data/tazerWrite.txt.meta.out", O_WRONLY);
    TazerFile *file = NULL;
    unsigned int fp = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, fp);
    int size = lseek(fd, 0, SEEK_END);
    REQUIRE(tazerWrite(file, fp, fd, buff, 10) == 10);
    lseek(fd, 0, SEEK_SET);
    REQUIRE(lseek(fd, 0, SEEK_END) == (size + 11));
    close(fd);

    char *buff2[10];
    int fd2 = open("local_data/tazerWrite.txt.meta.in", O_RDONLY);
    lseek(fd2, 0, SEEK_SET);
    read(fd2, buff2, 10);
    
    REQUIRE(memcmp(buff, buff2, 10) == 0);
    close(fd2);
}

TEST_CASE("write test","[Lib.cpp]") {
    char buff[10];
    memcpy(buff, "abcdefghij", 10);
    char buff2[10];
    std::ofstream newFile("local_data/local_write_file.txt");
    newFile.close();
    int fd = open("local_data/local_write_file.txt", O_RDWR);
    REQUIRE(write(fd, buff, 10) == 10);
    lseek(fd, -10, SEEK_CUR);
    unixread(fd, buff2, 10);
    REQUIRE(memcmp(buff, buff2, 10) == 0);
    remove("local_data/local_write_file.txt");
    close(fd);

    int fd2 = open("local_data/write.txt.meta.out", O_WRONLY);
    int size = lseek(fd, 0, SEEK_END);
    REQUIRE(write(fd2, buff, 10) == 10);
    lseek(fd, 0, SEEK_SET);
    REQUIRE(lseek(fd2, 0, SEEK_END) == (size + 11));
    close(fd2);
}

TEST_CASE("tazerLseek test","[Lib.cpp]") {
    int fd = tazerOpen("tazer1GB.dat","local_data/tazer1GB.dat.meta.in",TazerFile::Input,"",0,0);
    TazerFile *file = NULL;
    unsigned int fp = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, fp);
    REQUIRE(tazerLseek<off_t>(file, fp, fd, 10, SEEK_SET) == 10);
    REQUIRE(tazerLseek<off_t>(file, fp, fd, 10, SEEK_CUR) == 20);
    REQUIRE(tazerLseek<off_t>(file, fp, fd, -20, SEEK_CUR) == 0);
    off_t size = tazerLseek<off_t>(file, fp, fd, 0, SEEK_END);
    REQUIRE(tazerLseek<off_t>(file, fp, fd, 1, SEEK_END) == (size + 1));
    close(fd);
}

TEST_CASE("lseek test","[Lib.cpp]") {
    int fd[3] = {open("local_data/local1GB.dat", O_RDONLY),open("local_data/tazer1GB.dat.meta.in", O_RDONLY),open("local_data/fake", O_RDONLY)};
    int size = 0;
    int i;
    for(i = 0; i < 2; i++)
    {
        REQUIRE(lseek(fd[i], 10, SEEK_SET) == 10);
        REQUIRE(lseek(fd[i], 10, SEEK_CUR) == 20);
        REQUIRE(lseek(fd[i], -20, SEEK_CUR) == 0);
        size = lseek(fd[i], 0, SEEK_END);
        REQUIRE(lseek(fd[i], 1, SEEK_END) == (size + 1));
    }
    REQUIRE(lseek(fd[i], 10, SEEK_SET) == -1);
    close(fd[0]);
    close(fd[1]);
    close(fd[2]);
}

TEST_CASE("lseek64 test","[Lib.cpp]") {
    int fd[3] = {open("local_data/local1GB.dat", O_RDONLY),open("local_data/tazer1GB.dat.meta.in", O_RDONLY),open("local_data/fake", O_RDONLY)};
    int size = 0;
    int i;
    for(i = 0; i < 2; i++)
    {
        REQUIRE(lseek64(fd[i], 10, SEEK_SET) == 10);
        REQUIRE(lseek64(fd[i], 10, SEEK_CUR) == 20);
        REQUIRE(lseek64(fd[i], -20, SEEK_CUR) == 0);
        size = lseek64(fd[i], 0, SEEK_END);
        REQUIRE(lseek64(fd[i], 1, SEEK_END) == (size + 1));
    }
    REQUIRE(lseek64(fd[i], 10, SEEK_SET) == -1);
    close(fd[0]);
    close(fd[1]);
    close(fd[2]);
}

TEST_CASE("__xstat test","[Lib.cpp]") {
    int version = _STAT_VER_LINUX;
    std::string filename = "local_data/local1GB.dat";
    struct stat buff;
    int fd = open("local_data/local1GB.dat", O_RDONLY);
    REQUIRE(__xstat(version, filename.c_str(), &buff) == 0);
    REQUIRE(buff.st_size == 1048576000);
    close(fd);

    fd = open("local_data/tazer1GB.dat.meta.in", O_RDONLY);
    filename = "local_data/tazer1GB.dat.meta.in";
    struct stat buff2;
    REQUIRE(__xstat(version, filename.c_str(), &buff2) == 0);
    REQUIRE(buff.st_size == 1048576000);
    close(fd);
    filename = "local_data/fake";
    REQUIRE(__xstat(version, filename.c_str(), &buff) == -1);
}

TEST_CASE("__xstat64 test","[Lib.cpp]") {
    int version = _STAT_VER_LINUX;
    std::string filename = "local_data/local1GB.dat";
    struct stat64 buff;
    int fd = open("local_data/local1GB.dat", O_RDONLY);
    REQUIRE(__xstat64(version, filename.c_str(), &buff) == 0);
    REQUIRE(buff.st_size == 1048576000);
    close(fd);

    fd = open("local_data/tazer1GB.dat.meta.in", O_RDONLY);
    filename = "local_data/tazer1GB.dat.meta.in";
    struct stat64 buff2;
    REQUIRE(__xstat64(version, filename.c_str(), &buff2) == 0);
    REQUIRE(buff2.st_size == 1048576000);
    close(fd);
    filename = "local_data/fake";
    REQUIRE(__xstat64(version, filename.c_str(), &buff) == -1);
}

TEST_CASE("innerStat test","[Lib.cpp]") {
    int version = _STAT_VER_LINUX;
    std::string filename = "local_data/local1GB.dat";
    struct stat buff;
    struct stat64 buff2;
    int fd = open(filename.c_str(), O_RDONLY);
    REQUIRE(innerStat(version, filename.c_str(), &buff) == 0);
    REQUIRE(buff.st_size == 1048576000);
    REQUIRE(innerStat(version, filename.c_str(), &buff2) == 0);
    REQUIRE(buff2.st_size == 1048576000);
    close(fd);
}

TEST_CASE("tazerStat test","[Lib.cpp]") {
    std::ofstream newFile("local_data/fake_file.meta.in");
    newFile.close();
    std::string name = "tazer1GB.dat";
    std::string metaName = "local_data/tazer1GB.dat.meta.in";
    TazerFile::Type type = TazerFile::Input;
    int version = _STAT_VER_LINUX;
    struct stat buff;
    struct stat buff2;

    REQUIRE(tazerStat(name, metaName, type, version, name.c_str(), &buff) == 0);
    REQUIRE(buff.st_size == 1048576000);
    name = "fake_file";
    metaName = "local_data/fake_file.meta.in";
    REQUIRE(tazerStat(name, metaName, type, version, name.c_str(), &buff2) == -1);
    REQUIRE(buff2.st_size == 0);
    remove("local_data/fake_file.meta.in");
}

TEST_CASE("__lxstat test","[Lib.cpp]") {
    int version = _STAT_VER_LINUX;
    std::string filename = "local_data/local1GB.dat";
    struct stat buff;
    int fd = open("local_data/local1GB.dat", O_RDONLY);
    REQUIRE(__lxstat(version, filename.c_str(), &buff) == 0);
    REQUIRE(buff.st_size == 1048576000);
    close(fd);

    fd = open("local_data/tazer1GB.dat.meta.in", O_RDONLY);
    filename = "local_data/tazer1GB.dat.meta.in";
    struct stat buff2;
    REQUIRE(__lxstat(version, filename.c_str(), &buff2) == 0);
    REQUIRE(buff.st_size == 1048576000);
    close(fd);
    filename = "local_data/fake";
    REQUIRE(__lxstat(version, filename.c_str(), &buff) == -1);
}

TEST_CASE("__lxstat64 test","[Lib.cpp]") {
    int version = _STAT_VER_LINUX;
    std::string filename = "local_data/local1GB.dat";
    struct stat64 buff;
    int fd = open("local_data/local1GB.dat", O_RDONLY);
    REQUIRE(__lxstat64(version, filename.c_str(), &buff) == 0);
    REQUIRE(buff.st_size == 1048576000);
    close(fd);

    fd = open("local_data/tazer1GB.dat.meta.in", O_RDONLY);
    filename = "local_data/tazer1GB.dat.meta.in";
    struct stat64 buff2;
    REQUIRE(__lxstat64(version, filename.c_str(), &buff2) == 0);
    REQUIRE(buff.st_size == 1048576000);
    close(fd);
    filename = "local_data/fake";
    REQUIRE(__lxstat64(version, filename.c_str(), &buff) == -1);
}

TEST_CASE("tazerFsynce test","[Lib.cpp]") {
    std::string filepath = "local_data/tazer1GB.dat.meta.in";
    int fd = open(filepath.c_str(),O_RDONLY);
    TazerFile *file = NULL;
    unsigned int fp = 0;
    REQUIRE(tazerFsync(file, fd, fp) == 0);
    close(fd);
}

TEST_CASE("fsync test","[Lib.cpp]") {
    int fd = open("local_data/local1GB.dat", O_RDONLY);
    REQUIRE(fsync(fd) == 0);
    close(fd);
    fd = open("local_data/tazer1GB.dat.meta.in", O_RDONLY);
    REQUIRE(fsync(fd) == 0);
    close(fd);
}

TEST_CASE("tazerVector test","[Lib.cpp]") {
    int fd = open("local_data/local1GB.dat", O_RDONLY);
    int fd2 = open("local_data/tazer1GB.dat.meta.in", O_RDONLY);
    struct iovec iov[10];
    int iovcnt = 100;
    char buff[10][10];
    for (int i = 0; i < 10; i++) {
        iov[i].iov_base = buff[i];
        iov[i].iov_len = 10;
    }
    char temp[100];
    int fd_temp = open("local_data/local1GB.dat", O_RDONLY);
    read(fd_temp, temp, 100);
    close(fd_temp);

    REQUIRE(tazerVector("read", Timer::Metric::readv, tazerRead, unixread, fd, iov, iovcnt) == iovcnt);
    REQUIRE(memcmp(buff, temp, 100) == 0);

    fd_temp = open("tazer_data/tazer1GB.dat", O_RDONLY);
    read(fd_temp, temp, 100);
    close(fd_temp);

    REQUIRE(tazerVector("read", Timer::Metric::readv, tazerRead, unixread, fd2, iov, iovcnt) == iovcnt);
    REQUIRE(memcmp(buff, temp, 100) == 0);

    REQUIRE(tazerVector("read", Timer::Metric::readv, tazerRead, unixread, -1, iov, iovcnt) == -1);
    REQUIRE(tazerVector("write", Timer::Metric::writev, tazerWrite, unixwrite, -1, iov, iovcnt) == -1);

    std::ofstream newFile("local_data/local_write_file.txt");
    newFile.close();
    int fd3 = open("local_data/local_write_file.txt", O_RDWR);
    memcpy(buff,"abcabcabca", 10);
    memcpy(temp,"abcabcabca", 10);
    iovcnt = 10;

    REQUIRE(tazerVector("write", Timer::Metric::writev, tazerWrite, unixwrite, fd3, iov, iovcnt) == iovcnt);
    lseek(fd3, -10, SEEK_CUR);
    memcpy(buff,"asdfghhjkl", 10);
    REQUIRE(tazerVector("read", Timer::Metric::readv, tazerRead, unixread, fd3, iov, iovcnt) == iovcnt);
    REQUIRE(memcmp(buff, temp, 10) == 0);

    memcpy(buff,"abcdefghij", 10);
    memcpy(temp,"abcdefghij", 10);
    int fd4 = open("local_data/tazerVector.txt.meta.out", O_WRONLY);
    REQUIRE(tazerVector("write", Timer::Metric::writev, tazerWrite, unixwrite, fd4, iov, iovcnt) == iovcnt);
    close(fd4);
    memcpy(buff,"asdfghhjkl", 10);
    fd4 = open("local_data/tazerVector.txt.meta.in", O_RDONLY);
    REQUIRE(tazerVector("read", Timer::Metric::readv, tazerRead, unixread, fd4, iov, iovcnt) == iovcnt);
    REQUIRE(memcmp(buff, temp, 10) == 0);

    close(fd);
    close(fd2);
    close(fd3);
    close(fd4);
    remove("local_data/local_write_file.txt");   
}

TEST_CASE("readv test", "[Lib.cpp]") {
    int fd = open("local_data/local1GB.dat", O_RDONLY);
    int fd2 = open("local_data/tazer1GB.dat.meta.in", O_RDONLY);
    struct iovec iov[10];
    int iovcnt = 100;
    char buff[10][10];
    for (int i = 0; i < 10; i++) {
        iov[i].iov_base = buff[i];
        iov[i].iov_len = 10;
    }
    char temp[100];
    int fd_temp = unixopen("local_data/local1GB.dat", O_RDONLY);
    unixread(fd_temp, temp, 100);
    close(fd_temp);
    REQUIRE(readv(fd, iov, iovcnt) == iovcnt);
    REQUIRE(memcmp(buff, temp, iovcnt) == 0);
    fd_temp = unixopen("tazer_data/tazer1GB.dat", O_RDONLY);
    unixread(fd_temp, temp, 100);
    close(fd_temp);
    REQUIRE(readv(fd2, iov, iovcnt) == iovcnt);
    REQUIRE(memcmp(buff, temp, iovcnt) == 0);
    close(fd);
    close(fd2);
}

TEST_CASE("writev test","[Lib.cpp]") {
    std::ofstream newFile("local_data/local_write_file.txt");
    newFile.close();
    int fd = open("local_data/local_write_file.txt", O_RDWR);
    int fd2 = open("local_data/writev.txt.meta.out", O_WRONLY);
    struct iovec iov[10];
    int iovcnt = 10;
    char buff[10][1];
    for (int i = 0; i < 10; i++) {
        iov[i].iov_base = buff[i];
        iov[i].iov_len = 1;
    }
    char temp[10];
    memcpy(buff,"abcabcabca", 10);
    memcpy(temp,"abcabcabca", 10);
    REQUIRE(writev(fd, iov, iovcnt) == iovcnt);
    lseek(fd, 0, SEEK_SET);
    REQUIRE(readv(fd, iov, iovcnt) == iovcnt);
    REQUIRE(memcmp(buff, temp, iovcnt) == 0);

    memcpy(buff,"abcabcabca", 10);
    int size = lseek(fd2, 0, SEEK_END);
    REQUIRE(writev(fd2, iov, iovcnt) == iovcnt);
    lseek(fd2, 0, SEEK_SET);
    REQUIRE(lseek(fd2, 0, SEEK_END) == (size + 10));
    close(fd);
    close(fd2);
    remove("local_data/local_write_file.txt");
}

TEST_CASE("tazerFopen test","[Lib.cpp]") {
    std::string metaPath = "local_data/tazer1GB.dat.meta.in";
    TazerFile::Type type = TazerFile::Input;
    FILE *fp = NULL;
    
    fp = tazerFopen(metaPath, metaPath, type, metaPath.c_str(),"r");
    REQUIRE(fp != NULL);
    fclose(fp);
    fp = tazerFopen(metaPath, metaPath, type, metaPath.c_str(),"w");
    REQUIRE(fp != NULL);
    fclose(fp);
    std::string fake = "local_data/fake.meta.in";
    REQUIRE(tazerFopen(fake, fake, type, fake.c_str(), "r") == NULL);
}

TEST_CASE("fopen test","[Lib.cpp]") {
    FILE *fp = fopen("local_data/local1GB.dat", "r");
    FILE *fp2 = fopen("local_data/tazer1GB.dat.meta.in", "r");
    REQUIRE(fp != NULL);
    REQUIRE(fp2 != NULL);
    fclose(fp);
    fclose(fp2);
    fp = fopen("local_data/temp", "w");
    fp2 = fopen("local_data/tazer1GB.dat.meta.in", "w");
    REQUIRE(fp != NULL);
    REQUIRE(fp2 != NULL);
    REQUIRE(fopen("local_data/fake", "r") == NULL);
    fclose(fp);
    fclose(fp2);
    remove("local_data/temp");
}

TEST_CASE("fopen64 test","[Lib.cpp]") {
    FILE *fp = fopen64("local_data/local1GB.dat", "r");
    FILE *fp2 = fopen64("local_data/tazer1GB.dat.meta.in", "r");
    REQUIRE(fp != NULL);
    REQUIRE(fp2 != NULL);
    fclose(fp);
    fclose(fp2);
    fp = fopen64("local_data/temp", "w");
    fp2 = fopen64("local_data/tazer1GB.dat.meta.in", "w");
    REQUIRE(fp != NULL);
    REQUIRE(fp2 != NULL);
    REQUIRE(fopen64("local_data/fake", "r") == NULL);
    fclose(fp);
    fclose(fp2);
    remove("local_data/temp");
}

TEST_CASE("tazerFclose test", "[Lib.cpp]") {
    FILE *fp = fopen("local_data/tazer1GB.dat.meta.in", "r");
    ReaderWriterLock *lock = NULL;
    int fd = TazerFileStream::lookupStream(fp, lock);
    REQUIRE(fd != -1);
    lock->writerLock();
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFclose(file,pos,fd,fp) == 0);
    lock->writerUnlock();
    removeFileStream(unixfclose, fp);
}

TEST_CASE("fclose test","[Lib.cpp]") {
    FILE *fp = fopen("local_data/local1GB.dat", "r");
    FILE *fp2 = fopen("local_data/tazer1GB.dat.meta.in", "r");

    REQUIRE(fclose(fp) == 0);
    REQUIRE(fclose(fp2) == 0);
}

TEST_CASE("tazerFread test", "[Lib.cpp]") {
    char buff[10];
    char buff2[10];

    FILE *fp = fopen("local_data/tazer1GB.dat.meta.in", "r");
    ReaderWriterLock *lock = NULL;
    int fd = TazerFileStream::lookupStream(fp, lock);
    lock->writerLock();
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFread(file, pos, fd, buff, 1, 10, fp) == 10);
    lock->writerUnlock();
    fclose(fp);

    fd = open("tazer_data/tazer1GB.dat",O_RDONLY);
    read(fd, buff2, 10);
    close(fd);

    REQUIRE(memcmp(buff, buff2, 10) == 0);
}

TEST_CASE("fread test","[Lib.cpp]") {
    char buff[10];
    char buff2[10];
    int temp = open("local_data/local1GB.dat",O_RDONLY);
    read(temp, buff2, 10);
    close(temp);

    FILE *fp = fopen("local_data/local1GB.dat","r");
    REQUIRE(fread(buff, 1, 10, fp) == 10);
    fclose(fp);
    REQUIRE(memcmp(buff, buff2, 10) == 0);

    temp = open("tazer_data/tazer1GB.dat",O_RDONLY);
    read(temp, buff2, 10);
    close(temp);

    FILE *fp2 = fopen("local_data/tazer1GB.dat.meta.in", "r");
    REQUIRE(fread(buff, 1, 10, fp2) == 10);
    fclose(fp2);
    REQUIRE(memcmp(buff, buff2, 10) == 0);
}

TEST_CASE("tazerFwrite test","[Lib.cpp]") {
    FILE *fp = fopen("local_data/tazerFwrite.txt.meta.out", "w");

    char *buff[10];
    char *buff2[10];
    memcpy(buff, "qwertyuiop", 10);

    ReaderWriterLock *lock = NULL;
    int fd = TazerFileStream::lookupStream(fp, lock);

    lock->writerLock();
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFwrite(file, pos, fd, buff, 1, 10, fp) == 10);
    lock->writerUnlock();

    fclose(fp);

    fp = fopen("local_data/tazerFwrite.txt.meta.in", "r");
    fseek(fp, 0, SEEK_SET);
    fread(buff2, 1, 10, fp);
    fclose(fp);
    REQUIRE(memcmp(buff, buff2, 10) == 0);
}

TEST_CASE("fwrite test","[Lib.cpp]") {
    std::ofstream newFile("local_data/write_file.txt");
    newFile.close();
    FILE* fp = fopen("local_data/write_file.txt","r+");
    char buff[10];
    char buff2[10];
    memcpy(buff, "qwertyuiop", 10);
    REQUIRE(fwrite(buff, 1, 10, fp) == 10);
    fseek(fp, 0, SEEK_SET);
    fread(buff2, 1, 10, fp) == 10;
    REQUIRE(memcmp(buff, buff2, 10) == 0);    

    FILE * fp2 = fopen("local_data/fwrite.txt.meta.out","w");
    REQUIRE(ftell(fp2) == 0);
    REQUIRE(fwrite(buff, 1, 10, fp2) == 10);
    REQUIRE(ftell(fp2) == 10);

    fclose(fp);
    fclose(fp2);
    remove("local_data/write_file.txt");
}

TEST_CASE("tazerFtell test","[Lib.cpp]") {
    FILE *fp = fopen("local_data/tazer1GB.dat.meta.in","r");
    char buff[10];
    ReaderWriterLock *lock = NULL;
    int fd = TazerFileStream::lookupStream(fp, lock);

    lock->writerLock();
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFtell(file, pos, fd, fp) == 0);
    lock->writerUnlock();

    fread(buff, 1, 10, fp);

    lock->writerLock();
    file = NULL;
    pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFtell(file, pos, fd, fp) == 10);
    lock->writerUnlock();

    fclose(fp);
}

TEST_CASE("ftell test","[Lib.cpp]") {
    FILE *fp = fopen("local_data/local1GB.dat","r");
    char buff[100];
    FILE *fp2 = fopen("local_data/tazer1GB.dat.meta.in","r");

    REQUIRE(ftell(fp) == 0);
    fread(buff, 1, 100, fp);
    REQUIRE(ftell(fp) == 100);
    REQUIRE(ftell(fp2) == 0);
    fread(buff, 1, 100, fp2);
    REQUIRE(ftell(fp2) == 100);

    fclose(fp);
    fclose(fp2);
}

TEST_CASE("tazerFseek","[Lib.cpp]") {
    FILE *fp = fopen("local_data/tazer1GB.dat.meta.in","r");
    char buff[10];
    ReaderWriterLock *lock = NULL;
    int fd = TazerFileStream::lookupStream(fp, lock);

    lock->writerLock();
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFseek(file, pos, fd, fp, 10, SEEK_SET) == 10);
    lock->writerUnlock();

    fread(buff, 10, 10, fp);

    lock->writerLock();
    file = NULL;
    pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFseek(file, pos, fd, fp, 10, SEEK_SET) == 10);
    REQUIRE(tazerFseek(file, pos, fd, fp, -5, SEEK_CUR) == 5);
    int size = tazerFseek(file, pos, fd, fp, 0, SEEK_END);
    REQUIRE(tazerFseek(file, pos, fd, fp, 1, SEEK_END) == (size + 1));
    lock->writerUnlock();

    fclose(fp);
}

TEST_CASE("fseek test","[Lib.cpp]") {
    FILE * fp[2] = {fopen("local_data/local1GB.dat","r"),fopen("local_data/tazer1GB.dat.meta.in","r")};
    char buff[100];

    REQUIRE(fseek(fp[0], 100, SEEK_SET) == 0);
    REQUIRE(fseek(fp[0], -50, SEEK_CUR) == 0);
    REQUIRE(fseek(fp[0], 1, SEEK_END) == 0);
    fclose(fp[0]);

    REQUIRE(fseek(fp[1], 10, SEEK_SET) == 10);
    fread(buff, 10, 10, fp[1]);
    REQUIRE(fseek(fp[1], -50, SEEK_CUR) == 60);
    int size = fseek(fp[1], 0, SEEK_END);
    REQUIRE(fseek(fp[1], 1, SEEK_END) == (size + 1));
    fclose(fp[1]);
}

TEST_CASE("tazerFgetc test","[Lib.cpp]") {
    FILE *fp = fopen("local_data/tazer1GB.dat.meta.in","r");
    unsigned char buff[10];
    fread(buff, 1, 10, fp);
    fseek(fp, 0, SEEK_SET);

    ReaderWriterLock *lock = NULL;
    int fd = TazerFileStream::lookupStream(fp, lock);
    lock->writerLock();
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    for (int i = 0; i < 10; i++) {
        REQUIRE(tazerFgetc(file, pos, fd, fp) == (int)buff[i]);
    }
    lock->writerUnlock();

    //fseek(fp, 1, SEEK_END);
    //REQUIRE(tazerFgetc(file, pos, fd, fp) == EOF);

    fclose(fp);
}

TEST_CASE("fgetc test", "[Lib.cpp]") {
    FILE * fp[2] = {fopen("local_data/local1GB.dat","r"),fopen("local_data/tazer1GB.dat.meta.in","r")};
    unsigned char buff[2][10];
    fread(buff[0],1,10,fp[0]);
    fread(buff[1],1,10,fp[1]);
    fseek(fp[0],0,SEEK_SET);
    fseek(fp[1],0,SEEK_SET);
    for(int i = 0; i < 2; i++) {
        for (int j = 0; j < 10; j++)
            REQUIRE(fgetc(fp[i]) == buff[i][j]);
    }
    fclose(fp[0]);
    fclose(fp[1]);
}

TEST_CASE("tazerFgets test", "[Lib.cpp]") {
    FILE *fp = fopen("local_data/tazer1GB.dat.meta.in","r");
    char buff[100];
    char buff2[100];
    memcpy(buff2,buff,100);

    ReaderWriterLock *lock = NULL;
    int fd = TazerFileStream::lookupStream(fp, lock);

    lock->writerLock();
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFgets(file, pos, fd, buff, 100, fp) != NULL);
    lock->writerUnlock();

    REQUIRE(memcmp(buff, buff2, 100) != 0);

    fclose(fp);
}

TEST_CASE("fgets test", "[Lib.cpp]") {
    FILE *fp = fopen("local_data/local1GB.dat","r");
    FILE *fp2 = fopen("local_data/tazer1GB.dat.meta.in","r");
    char buff[100];
    char buff2[100];
    memcpy(buff2,buff,100);
    REQUIRE(fgets(buff, 100, fp) != NULL);
    REQUIRE(memcmp(buff, buff2, 100) != 0);
    memcpy(buff2,buff,100);
    REQUIRE(fgets(buff, 100, fp2) != NULL);
    REQUIRE(memcmp(buff, buff2, 100) != 0);

    fseek(fp, 1, SEEK_END);
    REQUIRE(fgets(buff, 100, fp) == NULL);

    //fseek(fp2, 1, SEEK_END);
    //REQUIRE(fgets(buff, 100, fp2) == NULL);

    fclose(fp);
    fclose(fp2);
}

TEST_CASE("tazerFputc test","[Lib.cpp]") {
    FILE *fp = fopen("local_data/tazerFputc.txt.meta.out", "w");
    int chars[5] = {97, 98, 99, 100, 101};
    ReaderWriterLock *lock = NULL;
    int fd = TazerFileStream::lookupStream(fp, lock);

    lock->writerLock();
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    for(int i = 0; i < 5; i++) {
        REQUIRE(tazerFputc(file, pos, fd, chars[i], fp) == chars[i]);
    }
    lock->writerUnlock();
    fclose(fp);

    fp = fopen("local_data/tazerFputc.txt.meta.in","r");
    fseek(fp, 0, SEEK_SET);
    for(int i = 0; i < 5; i++) {
        REQUIRE(fgetc(fp) == chars[i]);
    }
    fclose(fp); 
}

TEST_CASE("fputc test", "[Lib.cpp]") {
    std::ofstream newFile("local_data/local_write_file.txt");
    newFile.close();
    int chars[5] = {97, 98, 99, 100, 101};

    FILE *fp = fopen("local_data/fputc.txt.meta.out","w");

    for(int i = 0; i < 5; i++) {
        REQUIRE(fputc(chars[i],fp) == chars[i]);
    }
    fclose(fp);

    fp = fopen("local_data/fputc.txt.meta.in","r");
    fseek(fp, 0, SEEK_SET);
    for(int i = 0; i < 5; i++) {
        REQUIRE(fgetc(fp) == chars[i]);
    }
    fclose(fp);

//I was never able to get this section to work.
    // FILE *fp2 = fopen("local_data/local_write.txt","w");

    // for(int i = 0; i < 5; i++) {
    //     std::cout << "i = " << i << " " << chars[i] << std::endl;
    //     REQUIRE(fputc(chars[i],fp2) == chars[i]);
    // }
    // fclose(fp2);

    // fp2 = fopen("local_data/local_write_file.txt","r");
    // fseek(fp2, 0, SEEK_SET);
    // for(int i = 0; i < 5; i++) {
    //     REQUIRE(fgetc(fp2) == chars[i]);
    // }
    // fclose(fp2);  
    remove("local_data/local_write_file.txt");
}

TEST_CASE("tazerFeof test","[Lib.cpp]") {
    FILE *fp = fopen("local_data/tazer1GB.dat.meta.in","r");
    ReaderWriterLock *lock = NULL;
    int fd = TazerFileStream::lookupStream(fp, lock);

    lock->writerLock();
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFeof(file, pos, fd, fp) == 0);
    lock->writerUnlock();

    fseek(fp, 0, SEEK_END);
    fgetc(fp);

    lock = NULL;
    fd = TazerFileStream::lookupStream(fp, lock);
    lock->writerLock();
    file = NULL;
    pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFeof(file, pos, fd, fp) > 0);
    lock->writerUnlock();

    fclose(fp);
}

TEST_CASE("tazerFputs test", "[Lib.cpp]") {
    std::string str = "abcdefghij";
    char buff2[10];
    memcpy(buff2, "abcdefghij", 10);
    char buff[10];
    FILE *fp = fopen("local_data/tazerFputs.txt.meta.out", "w");

    ReaderWriterLock *lock = NULL;
    int fd = TazerFileStream::lookupStream(fp, lock);

    lock->writerLock();
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    REQUIRE(tazerFputs(file, pos, fd, str.c_str(), fp) >= 0);
    lock->writerUnlock();
    fseek(fp, 0, SEEK_SET);
    fclose(fp);

    fp = fopen("local_data/tazerFputs.txt.meta.in", "r");
    fread(buff, 1, 10, fp);
    REQUIRE(memcmp(buff, buff2, 10) == 0);
    fclose(fp);
}

TEST_CASE("fputs test", "[Lib.cpp]") {
    std::ofstream newFile2("local_data/local_write_file.txt");
    newFile2.close();

    std::string str = "abcdefghij";
    char buff2[10];
    memcpy(buff2, "abcdefghij", 10);
    char buff[10];
    FILE *fp = fopen("local_data/local_write_file.txt", "w");

    REQUIRE(fputs(str.c_str(), fp) >= 0);
    fseek(fp, 0, SEEK_SET);
    fclose(fp);

    fp = fopen("local_data/local_write_file.txt", "r");
    fread(buff, 1, 10, fp);
    REQUIRE(memcmp(buff, buff2, 10) == 0);

    fclose(fp);
    remove("local_data/local_write_file.txt");

    str = "jihgfedcba";
    memcpy(buff, str.c_str(), 10);
    fp = fopen("local_data/fputs.txt.meta.out", "w");

    REQUIRE(fputs(str.c_str(), fp)  == 10);
    fclose(fp);

    fp = fopen("local_data/fputs.txt.meta.in", "r");
    fread(buff2, 1, 10, fp);
    REQUIRE(memcmp(buff, buff2, 10) == 0);

    fclose(fp);
}

TEST_CASE("feof test", "[Lib.cpp]") {
    FILE *fp = fopen("local_data/local1GB.dat","r");
    REQUIRE(feof(fp) == 0);
    fseek(fp, 0, SEEK_END);
    fgetc(fp);
    REQUIRE(feof(fp) > 0);

    FILE *fp2 = fopen("local_data/tazer1GB.dat.meta.in","r");
    REQUIRE(feof(fp2) == 0);

    fseek(fp2, 1, SEEK_END);
    fgetc(fp2);
    REQUIRE(feof(fp2) > 0);

    fclose(fp);
    fclose(fp2);
}

TEST_CASE("tazerRewind test","[Lib.cpp]") {
    int fd = open("local_data/tazer1GB.dat.meta.in",O_RDONLY);
    int fd2 = 0;
    
    TazerFile *file = NULL;
    unsigned int pos = 0;
    TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);

    REQUIRE(tazerRewind(file, pos, fd, fd2, 10, SEEK_SET) == 10);
    REQUIRE(tazerRewind(file, pos, fd, fd2, -5, SEEK_CUR) == 5);
    REQUIRE(tazerRewind(file, pos, fd, fd2, 0, SEEK_SET) == 0);

    close(fd);

//This is probably what you would want to use if tazerRewind was actually called through outerWrapper like most of 
//the other tazer functions are.
    // FILE *fp = fopen("local_data/tazer1GB.dat.meta.in",O_RDONLY);
    // ReaderWriterLock *lock = NULL;

    // int fd = TazerFileStream::lookupStream(fp, lock);
    // int fd2 = 0;
    // lock->writerLock();
    // TazerFile *file = NULL;
    // unsigned int pos = 0;
    // TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    // REQUIRE(tazerRewind(file, pos, fd, fd2, 10, SEEK_SET) == 10);
    // lock->writerUnlock();

    // lock = NULL;
    // fd = TazerFileStream::lookupStream(fp, lock);
    // lock->writerLock();
    // file = NULL;
    // pos = 0;
    // TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    // REQUIRE(tazerRewind(file, pos, fd, fd2, -5, SEEK_CUR) == 5);
    // lock->writerUnlock();

    // lock = NULL;
    // fd = TazerFileStream::lookupStream(fp, lock);
    // lock->writerLock();
    // file = NULL;
    // pos = 0;
    // TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
    // REQUIRE(tazerRewind(file, pos, fd, fd2, 0, SEEK_SET) == 0);
    // lock->writerUnlock();

    // fclose(fp);
}

TEST_CASE("rewind test", "[Lib.cpp]") {
//rewind in Lib.cpp doesn't appear to be finished, so this only tests a local file and not a meta file
    FILE *fp = fopen("local_data/local1GB.dat","r");
    fgetc(fp);
    REQUIRE(ftell(fp) == 1);
    REQUIRE_NOTHROW(rewind(fp));
    REQUIRE(ftell(fp) == 0);
    fclose(fp);
}
