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

#include "FcntlCache.h"
#include "Config.h"

#include "ReaderWriterLock.h"
#include "Timer.h"
#include <experimental/filesystem>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

//TODO: create version that locks the file when reserved and only releases after it has written it...
FcntlCache::FcntlCache(std::string cacheName, uint64_t blockSize, std::string cachePath) : UnboundedCache(cacheName, blockSize),
                                                                                           _open((unixopen_t)dlsym(RTLD_NEXT, "open")),
                                                                                           _close((unixclose_t)dlsym(RTLD_NEXT, "close")),
                                                                                           _read((unixread_t)dlsym(RTLD_NEXT, "read")),
                                                                                           _write((unixwrite_t)dlsym(RTLD_NEXT, "write")),
                                                                                           _lseek((unixlseek_t)dlsym(RTLD_NEXT, "lseek")),
                                                                                           _fsync((unixfdatasync_t)dlsym(RTLD_NEXT, "fsync")),
                                                                                           _stat((unixxstat_t)dlsym(RTLD_NEXT, "stat")),
                                                                                           _cachePath(cachePath) {
    // std::cout<<"[TAZER] " << "Constructing " << _name << " in filelock cache " <<_blockSize<<" "<<blockSize << std::endl;
    _lock = new ReaderWriterLock();
    // mkdir(_cachePath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    std::error_code err;
    std::experimental::filesystem::create_directories(_cachePath, err);
    _shared = true;
    std::atomic_init(&_tcnt, (uint32_t)0);
}

FcntlCache::~FcntlCache() {
    for (auto fd : _fdLock) {
        (*_close)(fd.second);
    }
    for (auto fd : _fdData) {
        (*_close)(fd.second);
    }
    delete _lock;
}

void FcntlCache::writeToFile(int fd, uint64_t size, uint8_t *buff) {
    uint8_t *local = buff;
    while (size) {
        int bytes = (*_write)(fd, local, size);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            *this << "Failed a write " << fd << " " << size << std::endl;
        }
    }
}

void FcntlCache::readFromFile(int fd, uint64_t size, uint8_t *buff) {
    uint8_t *local = buff;
    while (size) {
        // std::cout << "reading " << size << " more" << std::endl;
        int bytes = (*_read)(fd, local, size);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            *this << "Failed a read " << fd << " " << size << std::endl;
        }
    }
}

uint8_t *FcntlCache::getBlockData(unsigned int blockIndex, unsigned int fileIndex) {
    uint8_t *buff = NULL;
    uint64_t size = _blockSize;
    _lock->readerLock();
    uint64_t filesize = _fileMap[fileIndex].fileSize;
    std::string name = _fileMap[fileIndex].name;
    _lock->readerUnlock();

    uint64_t numBlocks = (filesize / _blockSize) + (((filesize % _blockSize) > 0) ? 1 : 0);
    if (blockIndex + 1 == numBlocks) {
        size = filesize - (blockIndex * _blockSize);
    }
    // if (blockAvailable(blockIndex, fileIndex)) {
    std::string blkPath = _cachePath + "/" + name + "/data";
    buff = new uint8_t[size];
    int fd = (*_open)(blkPath.c_str(), O_RDWR);

    (*_lseek)(fd, blockIndex * _blockSize, SEEK_SET);
    readFromFile(fd, size, buff);
    (*_close)(fd);
    *this << "FileCache Read: " << blockIndex << " " << size << std::endl;
    // }
    return buff;
}

void FcntlCache::setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size, unsigned int fileIndex) {
    // std::cout << "[TAZER] " << _name << " " << _fileMap[fileIndex].name << " " << fileIndex << " setting block: " << blockIndex << std::endl;
    _lock->readerLock();
    std::string blkPath = _cachePath + "/" + _fileMap[fileIndex].name + "/data";
    int fd = (*_open)(blkPath.c_str(), O_RDWR); //Open file for writing
    _lock->readerUnlock();
    (*_lseek)(fd, blockIndex * _blockSize, SEEK_SET);
    writeToFile(fd, size, data);
    (*_fsync)(fd); //flush changes
    (*_close)(fd);
}

//Must lock first!
bool FcntlCache::blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs) {
    uint8_t byte = 0;
    bool avail = false;
    if (_blkIndex[fileIndex][index] == UBC_BLK_AVAIL) {
        avail = true;
    }
    if (checkFs && !avail) {
        _lock->readerLock();
        std::atomic<uint32_t> &blkCnt = _blkCnts[fileIndex][index];
        std::string name = _fileMap[fileIndex].name;
        _lock->readerUnlock();
        std::string blkPath = _cachePath + "/" + name + "/lock";

        if (blkCnt == 0) { // if blkCnt != 0 means im responsible for transfering and thus its not yet available
            int fd = (*_open)(blkPath.c_str(), O_RDWR);
            if (_fcntlLock.lockAvail2(fd, index, _cachePath + "/" + name)) { //if locked someone else is transfering the block (dont read until block is unlocked)

                (*_lseek)(fd, index, SEEK_SET);
                (*_read)(fd, &byte, sizeof(uint8_t));
                (*_close)(fd);
                if (byte == UBC_BLK_AVAIL) {
                    avail = true;
                    _blkIndex[fileIndex][index] = UBC_BLK_AVAIL;
                }
            }
            else {
                (*_close)(fd);
            }
        }
        if (!avail) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
    return avail;
}

bool FcntlCache::blockWritable(unsigned int index, unsigned int fileIndex, bool checkFs) {
    bool writable = !blockAvailable(index, fileIndex, checkFs);
    // std::cout << "checking if writable: " << fileIndex << " " << index << " " << writable << " " << (uint32_t)_blkIndex[fileIndex][index] << std::endl;
    if (writable) {
        if (checkFs) {
            _lock->readerLock();
            std::atomic<uint32_t> &blkCnt = _blkCnts[fileIndex][index];
            std::string name = _fileMap[fileIndex].name;
            _lock->readerUnlock();
            std::string blkPath = _cachePath + "/" + name + "/lock";
            int fd = (*_open)(blkPath.c_str(), O_RDWR);
            if (blkCnt > 0) {
                (*_lseek)(fd, index, SEEK_SET);
                uint8_t byte;
                (*_read)(fd, &byte, sizeof(uint8_t));
                if (byte == UBC_BLK_WR || byte == UBC_BLK_AVAIL) {
                    writable = false;
                }
                else {
                    writable = true;
                    (*_lseek)(fd, index, SEEK_SET);
                    byte = UBC_BLK_WR;
                    (*_write)(fd, &byte, sizeof(uint8_t));
                    (*_fsync)(fd);
                    _blkIndex[fileIndex][index] = byte;
                }
            }
            (*_close)(fd);
        }
        else {
            if (_blkIndex[fileIndex][index] == UBC_BLK_WR) {
                writable = false;
            }
        }
    }
    return writable;
}

bool FcntlCache::blockReserve(unsigned int index, unsigned int fileIndex) {
    // std::cout << "read locking to check reserve: " << fileIndex << " " << index << " " << (uint32_t)_blkIndex[fileIndex][index] << std::endl;
    _lock->readerLock();
    std::atomic<uint32_t> &blkCnt = _blkCnts[fileIndex][index];
    std::string name = _fileMap[fileIndex].name;
    _lock->readerUnlock();
    // uint64_t otime = Timer::getCurrentTime();
    std::string blkPath = _cachePath + "/" + name + "/lock";

    bool empty = false;
    bool reserved = false;

    if (_blkIndex[fileIndex][index] == UBC_BLK_EMPTY) {
        empty = true;
    }
    if (empty) {
        if (blkCnt == 0) {
            // uint64_t otime = Timer::getCurrentTime();
            //int fd = (*_open)(blkPath.c_str(), O_RDWR);
            _fdMutex.writerLock();
            int fd = _fdLock[fileIndex];
            bool locked = _fcntlLock.writerLock3(fd, index, blkCnt, _cachePath + "/" + name);
            // _ovhTime4 += Timer::getCurrentTime() - otime;
            // otime = Timer::getCurrentTime();
            if (locked) {
                uint8_t byte;
                (*_lseek)(fd, index, SEEK_SET);
                (*_read)(fd, &byte, sizeof(uint8_t));
                // _ovhTime += Timer::getCurrentTime() - otime;
                // otime = Timer::getCurrentTime();
                if (byte == UBC_BLK_EMPTY) {
                    (*_lseek)(fd, index, SEEK_SET);
                    byte = UBC_BLK_RES;
                    (*_write)(fd, &byte, sizeof(uint8_t));
                    (*_fsync)(fd);
                    _blkIndex[fileIndex][index] = byte;
                    reserved = true;
                }
                else {
                    empty = false;
                    _fcntlLock.writerUnlock3(fd, index, blkCnt, _cachePath + "/" + name);
                }
                // _ovhTime3 += Timer::getCurrentTime() - otime;
            }
            _fdMutex.writerUnlock();
            // (*_close)(fd);
            // _ovhTime4 += Timer::getCurrentTime() - otime;
            //}
        }
    }

    // std::cout << "read unlocking to check reserve: " << fileIndex << " " << index << " " << empty << " " << (uint32_t)_blkIndex[fileIndex][index] << std::endl;
    return reserved;
}

bool FcntlCache::blockSet(unsigned int index, unsigned int fileIndex, uint8_t byte) {
    int ret = true;
    // std::cout << "write locking to set: " << fileIndex << " " << index << " " << (uint32_t)byte << std::endl;
    _lock->writerLock();
    std::atomic<uint32_t> &blkCnt = _blkCnts[fileIndex][index];
    std::string name = _fileMap[fileIndex].name;
    std::string blkPath = _cachePath + "/" + name + "/lock";
    int fd = (*_open)(blkPath.c_str(), O_RDWR);
    (*_lseek)(fd, index, SEEK_SET);
    (*_write)(fd, &byte, sizeof(uint8_t));
    (*_fsync)(fd);
    (*_close)(fd);
    _blkIndex[fileIndex][index] = byte;
    _lock->writerUnlock();

    _fcntlLock.writerUnlock3(fd, index, blkCnt, _cachePath + "/" + name);
    // std::cout << "unlocking to set: " << fileIndex << " " << index << " " << (uint32_t)byte << " " << std::endl;
    return ret;
}

void FcntlCache::cleanUpBlockData(uint8_t *data) {
    delete[] data;
}

int FcntlCache::do_mkdir(const char *path, mode_t mode) {
    struct stat st;
    int status = 0;

    if ((*stat)(path, &st) != 0) {
        /* Directory does not exist. EEXIST for race condition */
        if (mkdir(path, mode) != 0) // && errno != EEXIST)
            status = -1;
    }
    else if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        status = -1;
    }

    return (status);
}

/**
** mkpath - ensure all directories in path exist
** Algorithm takes the pessimistic view and works top-down to ensure
** each directory in path exists, rather than optimistically creating
** the last element and working backwards.
*/
//hmmm can we just replace this with c++ filesystem creat_directories? (we use it in filecache)
int FcntlCache::mkpath(const char *path, mode_t mode) {
    char *pp;
    char *sp;
    int status;
    char *copypath = strdup(path);

    status = 0;
    pp = copypath;
    while (status == 0 && (sp = strchr(pp, '/')) != 0) {
        if (sp != pp) {
            /* Neither root nor double slash in path */
            *sp = '\0';
            status = do_mkdir(copypath, mode);
            *sp = '/';
        }
        pp = sp + 1;
    }
    if (status == 0)
        status = do_mkdir(path, mode);
    free(copypath);
    return (status);
}

void FcntlCache::addFile(unsigned int index, std::string filename, uint64_t blockSize, uint64_t fileSize) {
    uint64_t numBlocks = (fileSize / _blockSize) + (((fileSize % _blockSize) > 0) ? 1 : 0);
    if (_fileMap.count(index) == 0) {
        // std::string hashstr(_name + filename); //should cause each level of the cache to have different indicies for a given file
        uint64_t hash = 0;
        _lock->writerLock();
        _fileMap.emplace(index, FileEntry{filename, blockSize, fileSize, hash});
        auto temp = new std::atomic<uint32_t>[numBlocks];
        for (uint32_t i = 0; i < numBlocks; i++) {
            std::atomic_init(&temp[i], (uint32_t)0);
        }
        _blkCnts.emplace(index, temp);
        std::cout << "[TAZER] " << _name << " fn: " << filename << " fi: " << index << std::endl;
        _lock->writerUnlock();

        std::string filePath = Config::sharedMemName + "_" + std::to_string(index);
        int fd = shm_open(filePath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
        if (fd == -1) {
            DPRINTF("Reusing shared memory\n");
            fd = shm_open(filePath.c_str(), O_RDWR, 0644);
            if (fd != -1) {
                ftruncate(fd, numBlocks * sizeof(std::atomic<uint8_t>));
                void *ptr = mmap(NULL, numBlocks * sizeof(std::atomic<uint8_t>), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                _blkIndex[index] = (std::atomic<uint8_t> *)ptr;
            }
            else {
                std::cerr << "[TAZER] "
                          << "Error opening shared memory " << strerror(errno) << std::endl;
            }
        }
        else {
            DPRINTF("Created shared memory\n");
            ftruncate(fd, numBlocks * sizeof(std::atomic<uint8_t>));
            void *ptr = mmap(NULL, numBlocks * sizeof(std::atomic<uint8_t>), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            _blkIndex[index] = (std::atomic<uint8_t> *)ptr;
            for (uint32_t i = 0; i < numBlocks; i++) {
                std::atomic_init(&_blkIndex[index][i], (uint8_t)0);
            }
        }

        std::string tmp = _cachePath + "/" + filename;
        std::cout << tmp << std::endl;

        mkpath(tmp.c_str(), 0777);
        int ret = mkdir((tmp + "/init/").c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); //if -1 means another process has already reserved

        if (ret == 0) {
            std::cout << "making lock and data file" << std::endl;
            _lock->writerLock();
            int fd = (*_open)((tmp + "/lock_tmp").c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
            ftruncate(fd, numBlocks * sizeof(uint8_t));
            uint8_t byte = 0;
            (*_lseek)(fd, numBlocks * sizeof(uint8_t), SEEK_SET);
            (*_write)(fd, &byte, sizeof(uint8_t));
            (*_fsync)(fd);
            _fdMutex.writerLock();
            _fdLock[index] = fd;
            _fdMutex.writerUnlock();
            //(*_close)(fd);
            int ret = rename((tmp + "/lock_tmp").c_str(), (tmp + "/lock").c_str());
            if (ret != 0) {
                std::cout << "[TAZER] " << _name << " ERROR: lock rename went wrong!!!" << strerror(errno) << std::endl;
                exit(1);
            }
            fd = (*_open)((tmp + "/data_tmp").c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
            ftruncate(fd, numBlocks * _blockSize * sizeof(uint8_t));
            (*_lseek)(fd, numBlocks * _blockSize * sizeof(uint8_t), SEEK_SET);
            (*_write)(fd, &byte, sizeof(uint8_t));
            (*_fsync)(fd);
            (*_close)(fd);
            ret = rename((tmp + "/data_tmp").c_str(), (tmp + "/data").c_str());
            if (ret != 0) {
                std::cout << "[TAZER] " << _name << " ERROR: data rename went wrong!!!" << strerror(errno) << std::endl;
                exit(1);
            }
            _lock->writerUnlock();
        }
        else {
            std::cout << "reusing lock and data file" << std::endl;
            int cnt = 0;
            while (!std::experimental::filesystem::exists(tmp + "/lock")) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (cnt++ > 100) {
                    std::cout << "[TAZER] " << _name << " ERROR: waiting for lock to appear!!!" << strerror(errno) << std::endl;
                    exit(1);
                }
            }
            _fdMutex.readerLock();
            _fdLock[index] = (*_open)((tmp + "/lock").c_str(), O_RDWR);
            _fdMutex.readerUnlock();
            cnt = 0;
            while (!std::experimental::filesystem::exists(tmp + "/data")) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (cnt++ > 100) {
                    std::cout << "[TAZER] " << _name << " ERROR: waiting for data to appear!!!" << strerror(errno) << std::endl;
                    exit(1);
                }
            }
        }
    }
    if (_nextLevel) {
        _nextLevel->addFile(index, filename, blockSize, fileSize);
    }
}

Cache *FcntlCache::addNewFcntlCache(std::string fileName, uint64_t blockSize, std::string cachePath) {
    return Trackable<std::string, Cache *>::AddTrackable(
        fileName, [=]() -> Cache * {
            Cache *temp = new FcntlCache(fileName, blockSize, cachePath);
            if (temp)
                return temp;
            delete temp;
            return NULL;
        });
}
