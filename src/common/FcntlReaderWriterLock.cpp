// -*-Mode: C++;-*-

//*BeginLicense**************************************************************
//
//---------------------------------------------------------------------------
// TAZeR (github.com/pnnl/tazer/)
//---------------------------------------------------------------------------
//
// Copyright ((c)) 2019, Battelle Memorial Institute
//
// 1. Battelle Memorial Institute (hereinafter Battelle) hereby grants
//    permission to any person or entity lawfully obtaining a copy of
//    this software and associated documentation files (hereinafter "the
//    Software") to redistribute and use the Software in source and
//    binary forms, with or without modification.  Such person or entity
//    may use, copy, modify, merge, publish, distribute, sublicense,
//    and/or sell copies of the Software, and may permit others to do
//    so, subject to the following conditions:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimers.
//
//    * Redistributions in binary form must reproduce the above
//      copyright notice, this list of conditions and the following
//      disclaimer in the documentation and/or other materials provided
//      with the distribution.
//
//    * Other than as used herein, neither the name Battelle Memorial
//      Institute or Battelle may be used in any form whatsoever without
//      the express written consent of Battelle.
//
// 2. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
//    CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
//    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
//    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//    DISCLAIMED. IN NO EVENT SHALL BATTELLE OR CONTRIBUTORS BE LIABLE
//    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
//    OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
//    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
//    USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
//    DAMAGE.
//
// ***
//
// This material was prepared as an account of work sponsored by an
// agency of the United States Government.  Neither the United States
// Government nor the United States Department of Energy, nor Battelle,
// nor any of their employees, nor any jurisdiction or organization that
// has cooperated in the development of these materials, makes any
// warranty, express or implied, or assumes any legal liability or
// responsibility for the accuracy, completeness, or usefulness or any
// information, apparatus, product, software, or process disclosed, or
// represents that its use would not infringe privately owned rights.
//
// Reference herein to any specific commercial product, process, or
// service by trade name, trademark, manufacturer, or otherwise does not
// necessarily constitute or imply its endorsement, recommendation, or
// favoring by the United States Government or any agency thereof, or
// Battelle Memorial Institute. The views and opinions of authors
// expressed herein do not necessarily state or reflect those of the
// United States Government or any agency thereof.
//
//                PACIFIC NORTHWEST NATIONAL LABORATORY
//                             operated by
//                               BATTELLE
//                               for the
//                  UNITED STATES DEPARTMENT OF ENERGY
//                   under Contract DE-AC05-76RL01830
//
//*EndLicense****************************************************************

#include "FcntlReaderWriterLock.h"
#include "Config.h"
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)

FcntlReaderWriterLock::FcntlReaderWriterLock() {
}

FcntlReaderWriterLock::~FcntlReaderWriterLock() {
}

void FcntlReaderWriterLock::readerLock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt) {
    _lock.readerLock();
    uint32_t prev = blkCnt.fetch_add(1); //compare and swap 1 and 0
    if (prev == 0) {
        _fdMutex.writerLock();
        prev = blkCnt.load();
        if (prev == 0) {
            struct flock lock = {};
            lock.l_type = F_RDLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = blk;
            lock.l_len = 1;
            fcntl(fd, F_SETLKW, &lock);
        }
        _fdMutex.writerUnlock();
    }
}

void FcntlReaderWriterLock::readerUnlock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt) {
    uint32_t prev = blkCnt.fetch_sub(1); //compare and swap 0 and 1
    if (prev == 1) {
        _fdMutex.writerLock();
        prev = blkCnt.load();
        if (prev == 0) {
            struct flock lock = {};
            lock.l_type = F_UNLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = blk;
            lock.l_len = 1;
            fcntl(fd, F_SETLKW, &lock);
        }
        _fdMutex.writerUnlock();
    }
    _lock.readerUnlock();
}

void FcntlReaderWriterLock::writerLock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt) {
    _lock.writerLock();
    uint32_t prev = blkCnt.fetch_add(1);
    if (prev == 0) { //i think this should always be true...
        struct flock lock = {};
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = blk;
        lock.l_len = 1;
        fcntl(fd, F_SETLKW, &lock);
    }
    else {
        std::cout << "fcntlwritelock This should not be possible" << std::endl;
    }
}

int FcntlReaderWriterLock::writerLock2(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt) {
    int ret = -1;
    _fdMutex.writerLock();

    // uint32_t prev = blkCnt.fetch_add(1);
    uint32_t zero = 0;
    if (blkCnt.compare_exchange_strong(zero, 1)) { //i think this should always be true...
        struct flock lock = {};
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = blk;
        lock.l_len = 1;
        ret = fcntl(fd, F_SETLK, &lock);
        if (ret == -1) {
            blkCnt = 0;
            // _lock.writerUnlock();
        }
    }
    _fdMutex.writerUnlock();
    return ret != -1;
}

int FcntlReaderWriterLock::writerLock3(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt, std::string blkPath) {
    int ret = -1;
    ret = mkdir((blkPath + "/" + std::to_string(blk)).c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    return ret == 0;
}

//retu
int FcntlReaderWriterLock::lockAvail(int fd, uint64_t blk) {
    int ret = -1;
    struct flock lock = {};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = blk;
    lock.l_len = 1;
    lock.l_pid = 0;
    fcntl(fd, F_GETLK, &lock);
    if (lock.l_type == F_UNLCK) {
        ret = 1;
    }
    return ret;
}

int FcntlReaderWriterLock::lockAvail2(int fd, uint64_t blk, std::string blkPath) {
    struct stat statbuf;
    int ret = (*stat)((blkPath + "/" + std::to_string(blk)).c_str(), &statbuf);
    return ret == -1;
}

void FcntlReaderWriterLock::writerUnlock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt) {
    uint32_t prev = blkCnt.fetch_sub(1);
    if (prev == 1) { //i think this should always be true...
        struct flock lock = {};
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = blk;
        lock.l_len = 1;
        fcntl(fd, F_SETLKW, &lock);
    }
    else {
        std::cout << "fcntlwriteunlock This should not be possible" << std::endl;
    }
    _lock.writerUnlock();
}

void FcntlReaderWriterLock::writerUnlock2(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt) {
    _fdMutex.writerLock();
    // uint32_t prev = blkCnt.fetch_sub(1);
    // std::cout << "trying to unlock " << fd << " " << blk << " " << prev << std::endl;
    uint32_t one = 1;
    if (blkCnt.compare_exchange_strong(one, 2)) { //i think this should always be true...
        struct flock lock = {};
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = blk;
        lock.l_len = 1;
        int ret = fcntl(fd, F_SETLKW, &lock);
        blkCnt = 0;
        // std::cout << "unlocking: " << fd << " " << blk << "ret: " << ret << std::endl;
    }
    else {
        std::cout << "fcntlwriteunlock why am i here?" << std::endl;
    }
    _fdMutex.writerUnlock();
}

void FcntlReaderWriterLock::writerUnlock3(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt, std::string blkPath) {
    rmdir((blkPath + "/" + std::to_string(blk)).c_str());
}

// FcntlBoundedReaderWriterLock::FcntlBoundedReaderWriterLock(uint32_t entrySize, uint32_t numEntries, std::string lockPath) : _entrySize(entrySize),
//                                                                                                                             _numEntries(numEntries),
//                                                                                                                             _lockPath(lockPath) {

//     std::string filePath = Config::sharedMemName + "_" + Config::tazer_id  + "_lock_" + std::to_string(_entrySize) + "_" + std::to_string(_numEntries);
//     std::atomic<uint16_t> temp[4096];
//     std::cout << _numEntries << " " << 2 * _numEntries * sizeof(std::atomic<uint16_t>) << " " << sizeof(temp) << " " << 2 * sizeof(temp) << std::endl;

//     int fd = shm_open(filePath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
//     if (fd == -1) {
//         // DPRINTF("Reusing shared memory\n");
//         fd = shm_open(filePath.c_str(), O_RDWR, 0644);
//         if (fd != -1) {
//             if (ftruncate(fd, 2 * _numEntries * sizeof(std::atomic<uint16_t>) + sizeof(ReaderWriterLock)) != -1) {
//                 void *ptr = mmap(NULL, 2 * _numEntries * sizeof(std::atomic<uint16_t>) + sizeof(ReaderWriterLock), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

//                 _readers = (std::atomic<uint16_t> *)ptr;
//                 _writers = (std::atomic<uint16_t> *)((uint8_t *)_readers + _numEntries * sizeof(std::atomic<uint16_t>));
//                 _fdMutex = (ReaderWriterLock *)((uint8_t *)_writers + _numEntries * sizeof(std::atomic<uint16_t>));
//             }
//             else {
//                 std::cerr << "[TAZER] "
//                           << "Error sizing shared memory " << std::endl; // << strerror(errno) << std::endl;
//             }
//         }
//         else {
//             std::cerr << "[TAZER] "
//                       << "Error opening shared memory " << std::endl; // << strerror(errno) << std::endl;
//         }
//     }
//     else {
//         // DPRINTF("Created shared memory\n");
//         if (ftruncate(fd, 2 * _numEntries * sizeof(std::atomic<uint16_t>) + sizeof(ReaderWriterLock)) != -1) {
//             void *ptr = mmap(NULL, 2 * (_numEntries * sizeof(std::atomic<uint16_t>)) + sizeof(ReaderWriterLock), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//             _readers = (std::atomic<uint16_t> *)ptr;
//             _writers = (std::atomic<uint16_t> *)((uint8_t *)_readers + _numEntries * sizeof(std::atomic<uint16_t>));
//             _fdMutex = new ((uint8_t *)_writers + _numEntries * sizeof(std::atomic<uint16_t>)) ReaderWriterLock();
//             _fdMutex->writerLock();

//             memset(_readers, 0, _numEntries * sizeof(std::atomic<uint16_t>));
//             memset(_writers, 0, _numEntries * sizeof(std::atomic<uint16_t>));

//             _fdMutex->writerUnlock();
//         }
//         else {
//             std::cerr << "[TAZER] "
//                       << "Error sizing shared memory " << std::endl; // << strerror(errno) << std::endl;
//         }
//     }
//     _fd = (*(unixopen_t)dlsym(RTLD_NEXT, "open"))(_lockPath.c_str(), O_RDWR);
// }

FcntlBoundedReaderWriterLock::FcntlBoundedReaderWriterLock(uint32_t entrySize, uint32_t numEntries, std::string lockPath) : _entrySize(entrySize),
                                                                                                                            _numEntries(numEntries),
                                                                                                                            _lockPath(lockPath) {

    _readers = new std::atomic<uint16_t>[_numEntries];
    memset(_readers, 0, _numEntries * sizeof(std::atomic<uint16_t>));
    _writers = new std::atomic<uint16_t>[_numEntries];
    memset(_writers, 0, _numEntries * sizeof(std::atomic<uint16_t>));

    _fdMutex = new ReaderWriterLock();
    _fd = (*(unixopen_t)dlsym(RTLD_NEXT, "open"))(_lockPath.c_str(), O_RDWR);
    if (Config::enableSharedMem){
        

        std::string shmPath("/" + Config::tazer_id + "_fcntlbnded_shm.lck");
        int shmFd = shm_open(shmPath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
        if (shmFd == -1) {
            shmFd = shm_open(shmPath.c_str(), O_RDWR, 0644);
            if (shmFd != -1) {
                std::cout << "resusing fcntl shm lock" << std::endl;
                ftruncate(shmFd, sizeof(uint32_t) + sizeof(ReaderWriterLock));
                void *ptr = mmap(NULL, sizeof(uint32_t) + sizeof(ReaderWriterLock), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
                uint32_t *init = (uint32_t *)ptr;
                while (!*init) {
                    sched_yield();
                }
                _shmLock = (ReaderWriterLock *)((uint8_t *)init + sizeof(uint32_t));
            }
            else {
                std::cerr << "[TAZER]"
                        << "Error opening shared memory " << strerror(errno) << std::endl;
            }
        }
        else {
            std::cout << "creating fcntl shm lock" << std::endl;
            ftruncate(shmFd, sizeof(uint32_t) + sizeof(ReaderWriterLock));
            void *ptr = mmap(NULL, sizeof(uint32_t) + sizeof(ReaderWriterLock), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);

            uint32_t *init = (uint32_t *)ptr;
            _shmLock = new ((uint8_t *)init + sizeof(uint32_t)) ReaderWriterLock();
            *init = 1;
        }
    }
    else{
        _shmLock = new ReaderWriterLock();
    }
}

//todo better delete aka make sure all readers/writers are done...
FcntlBoundedReaderWriterLock::~FcntlBoundedReaderWriterLock() {
    delete[] _readers;
    delete[] _writers;
    if (Config::enableSharedMem){
        std::string shmPath("/" + Config::tazer_id + "_fcntlbnded_shm.lck");
        shm_unlink(shmPath.c_str());
        (*(unixclose_t)dlsym(RTLD_NEXT, "close"))(_fd);
    }
    delete _fdMutex;
    delete _shmLock;
}

void FcntlBoundedReaderWriterLock::readerLock(uint64_t entry) {
    // std::cout << "read locking " << entry << " " << _readers[entry] << std::endl;
    int cnt = 0;
    while (1) {
        cnt++;
        while (_writers[entry].load()) {
            std::this_thread::yield();
        }

        auto prev = _readers[entry].fetch_add(1);
        if (!_writers[entry].load()) {
            if (prev == 0) {
                // std::cout << "got memory readlock " << entry << std::endl;
                _fdMutex->writerLock();
                struct flock lock = {};
                lock.l_type = F_RDLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = entry * _entrySize;
                lock.l_len = _entrySize;
                _shmLock->writerLock();
                int ret = fcntl(_fd, F_SETLK, &lock);
                _shmLock->writerUnlock();
                _fdMutex->writerUnlock();
                if (ret == -1) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    if (errno != EACCES && errno != EAGAIN) {
                        std::cout << "[TAZER] R LOCK ERROR!!! " << entry << " " << __LINE__ << " " << strerror(errno) << std::endl;
                    }
                }
                else {
                    // std::cout << "read locking " << entry << " (" << (entry * _entrySize) << "-" << (entry * _entrySize) + _entrySize << ")"
                    //           << " " << ::getpid() << std::endl;
                    break;
                }
            }
            else {
                break;
            }
        }
        _readers[entry].fetch_sub(1);
    }
    // std::cout << "leaving read locking " << entry << " " << _readers[entry] << std::endl;
}

void FcntlBoundedReaderWriterLock::readerUnlock(uint64_t entry) {
    // std::cout << "read unlocking " << entry << " " << _readers[entry] << std::endl;
    uint16_t one = 1;
    if (_readers[entry].compare_exchange_strong(one, 0)) {
        _fdMutex->writerLock();
        struct flock lock = {};
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = entry * _entrySize;
        lock.l_len = _entrySize;
        _shmLock->writerLock();
        int ret = fcntl(_fd, F_SETLK, &lock);
        _shmLock->writerUnlock();
        int cnt = 0;
        while (ret == -1) {
            _fdMutex->writerUnlock();
            cnt++;
            // if (cnt == 100000) {
            //     cnt = 0;
            std::cout << "[TAZER] R UNLOCK ERROR!!! " << entry << " " << __LINE__ << " " << strerror(errno) << std::endl;
            // }
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            _fdMutex->writerLock();
            _shmLock->writerLock();
            ret = fcntl(_fd, F_SETLK, &lock);
            _shmLock->writerUnlock();
        }
        // std::cout << "lock ret: " << ret << std::endl;
        // std::cout << "read unlocking " << entry << " (" << (entry * _entrySize) << "-" << (entry * _entrySize) + _entrySize << ")"
        //           << " " << ::getpid() << std::endl;
        _fdMutex->writerUnlock();
    }
    else {
        _readers[entry].fetch_sub(1);
    }
}

void FcntlBoundedReaderWriterLock::writerLock(uint64_t entry) {

    uint16_t check = 1;
    while (_writers[entry].exchange(check) == 1) {
        std::this_thread::yield();
    }
    // std::cout << "got memory writelock " << entry << std::endl;
    while (_readers[entry].load()) {
        std::this_thread::yield();
    }
    _fdMutex->writerLock();
    struct flock lock = {};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = entry * _entrySize;
    lock.l_len = _entrySize;
    _shmLock->writerLock();
    int ret = fcntl(_fd, F_SETLK, &lock);
    _shmLock->writerUnlock();
    int cnt = 0;
    while (ret == -1) {
        struct flock lock2 = {};
        lock2.l_type = F_WRLCK;
        lock2.l_whence = SEEK_SET;
        lock2.l_start = entry * _entrySize;
        lock2.l_len = _entrySize;
        fcntl(_fd, F_GETLK, &lock2);
        _fdMutex->writerUnlock();
        cnt++;
        if (cnt % 10000 == 0) {
            // cnt = 0;
            std::cout << "[TAZER] W LOCK ERROR!!! " << entry << " " << __LINE__ << " " << strerror(errno) << " " << ::getpid() << std::endl;
            std::cout << "[TAZER] lock info " << lock2.l_type << " (" << F_RDLCK << "," << F_WRLCK << "," << F_UNLCK << ") " << lock2.l_start << " " << lock2.l_len << " " << lock2.l_pid << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        _fdMutex->writerLock();
        _shmLock->writerLock();
        ret = fcntl(_fd, F_SETLK, &lock);
        _shmLock->writerUnlock();
    }
    // std::cout << "write locking " << entry << " (" << (entry * _entrySize) << "-" << (entry * _entrySize) + _entrySize << ")"
    //           << " " << ::getpid() << std::endl;
    // std::cout << "lock ret: " << ret << std::endl;
    _fdMutex->writerUnlock();
}

void FcntlBoundedReaderWriterLock::writerUnlock(uint64_t entry) {
    // std::cout << "write unlocking " << entry << std::endl;
    _fdMutex->writerLock();
    struct flock lock = {};
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = entry * _entrySize;
    lock.l_len = _entrySize;
    _shmLock->writerLock();
    int ret = fcntl(_fd, F_SETLK, &lock);
    _shmLock->writerUnlock();
    int cnt = 0;
    while (ret == -1) {
        _fdMutex->writerUnlock();
        cnt++;
        if (cnt == 1000000) {
            cnt = 0;
            std::cout << "[TAZER] W UNLOCK ERROR!!! " << entry << " " << __LINE__ << " " << strerror(errno) << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        _fdMutex->writerLock();
        _shmLock->writerLock();
        ret = fcntl(_fd, F_SETLK, &lock);
        _shmLock->writerUnlock();
    }
    // std::cout << "write unlocking " << entry << " (" << (entry * _entrySize) << "-" << (entry * _entrySize) + _entrySize << ")"
    //           << " " << ::getpid() << std::endl;
    _fdMutex->writerUnlock();
    _writers[entry].store(0);
}

int FcntlBoundedReaderWriterLock::lockAvail(uint64_t entry) {

    int ret = -1;
    if (_readers[entry] == 0 && _writers[entry] == 0) {
        struct flock lock = {};
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = entry * _entrySize;
        lock.l_len = _entrySize;
        lock.l_pid = 0;
        _shmLock->writerLock();
        fcntl(_fd, F_GETLK, &lock);
        _shmLock->writerUnlock();
        if (lock.l_type == F_UNLCK) {
            ret = 1;
        }
    }
    return ret;
}