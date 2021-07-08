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

#include "FileCache.h"
#include "Config.h"
#include "ReaderWriterLock.h"
#include "Timer.h"
#include "xxhash.h"
#include <chrono>
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

//this cache exists on a single node
FileCache::FileCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string filePath) : BoundedCache(cacheName, type, cacheSize, blockSize, associativity),
                                                                                                                                    _fullFlag(0),
                                                                                                                                    _open((unixopen_t)dlsym(RTLD_NEXT, "open")),
                                                                                                                                    _close((unixclose_t)dlsym(RTLD_NEXT, "close")),
                                                                                                                                    _read((unixread_t)dlsym(RTLD_NEXT, "read")),
                                                                                                                                    _write((unixwrite_t)dlsym(RTLD_NEXT, "write")),
                                                                                                                                    _lseek((unixlseek_t)dlsym(RTLD_NEXT, "lseek")),
                                                                                                                                    _fsync((unixfdatasync_t)dlsym(RTLD_NEXT, "fsync")) {
    //_pid = (unsigned int)getpid();
    std::thread::id thread_id = std::this_thread::get_id();
    stats.checkThread(thread_id, true);
    stats.threadStart(thread_id);
    stats.start();
    _blocksfd = -1;
    _blkIndexfd = -1;

    _filePath = filePath + "/" + Config::tazer_id + "_" + _name + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".tzr";
    std::string indexPath("/" + Config::tazer_id + "_" + _name + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".idx");

    bool *indexInit;
    if (Config::enableSharedMem) {
        _blkIndexfd = shm_open(indexPath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
        if (_blkIndexfd == -1) {
            DPRINTF("Reusing shared memory\n");
            log(this) << _name << "reusing shared memory" << std::endl;
            _blkIndexfd = shm_open(indexPath.c_str(), O_RDWR, 0644);
            if (_blkIndexfd != -1) {
                ftruncate(_blkIndexfd, sizeof(uint32_t) + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins) + sizeof(bool));
                void *ptr = mmap(NULL, sizeof(uint32_t) + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins) + sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED, _blkIndexfd, 0);
                uint32_t *init = (uint32_t *)ptr;
                while (!*init) {
                    sched_yield();
                }
                _blkIndex = (MemBlockEntry *)((uint8_t *)init + sizeof(uint32_t));
                auto binLockDataAddr = (uint8_t *)_blkIndex + _numBlocks * sizeof(MemBlockEntry);
                _binLock = new MultiReaderWriterLock(_numBins, binLockDataAddr);
                indexInit = (bool *)((uint8_t *)binLockDataAddr + MultiReaderWriterLock::getDataSize(_numBins));
            }
            else {
                std::cerr << "[TAZER]"
                          << "Error opening shared memory " << strerror(errno) << std::endl;
            }
        }
        else {
            DPRINTF("Created shared memory\n");
            log(this) << _name << "created shared memory" << std::endl;
            ftruncate(_blkIndexfd, sizeof(uint32_t) + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins) + sizeof(bool));
            void *ptr = mmap(NULL, sizeof(uint32_t) + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins) + sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED, _blkIndexfd, 0);

            uint32_t *init = (uint32_t *)ptr;
            *init = 0;
            _blkIndex = (MemBlockEntry *)((uint8_t *)init + sizeof(uint32_t));
            // _lock = new ((uint8_t *)_blkIndex + _numBlocks * sizeof(MemBlockEntry)) ReaderWriterLock();
            auto binLockDataAddr = (uint8_t *)_blkIndex + _numBlocks * sizeof(MemBlockEntry);
            _binLock = new MultiReaderWriterLock(_numBins, binLockDataAddr, true);
            indexInit = (bool *)((uint8_t *)binLockDataAddr + MultiReaderWriterLock::getDataSize(_numBins));
            _binLock->writerLock(0);
            // memset(_blkIndex, 0, _numBlocks * sizeof(MemBlockEntry));
            for (uint32_t i =0;i<_numBlocks;i++){
                _blkIndex[i].init(this);
            }
            *indexInit = false;
            _binLock->writerUnlock(0);
            *init = 1;
        }
    }
    else {
        _blkIndex = new MemBlockEntry[_numBlocks];
        auto binLockDataAddr = new uint8_t[MultiReaderWriterLock::getDataSize(_numBins)];
        _binLock = new MultiReaderWriterLock(_numBins, binLockDataAddr, true);
        _binLock->writerLock(0);
        // memset(_blkIndex, 0, _numBlocks * sizeof(MemBlockEntry));
        for (uint32_t i =0;i<_numBlocks;i++){
            _blkIndex[i].init(this);
        }
        _binLock->writerUnlock(0);
    }

    _binLock->writerLock(0);
    if (Config::reuseFileCache) {
        //Try to open an already existing file
        _blocksfd = (*_open)(_filePath.c_str(), O_RDWR);
        DPRINTF("Opened file %d\n", _blocksfd);
    }

    //Failed... Try to create file
    if (_blocksfd == -1) {
        // std::cerr << "[TAZER]"
        //           << "Creating file cache: " << _filePath << std::endl;
        std::error_code err;
        std::experimental::filesystem::create_directories(filePath, err);
        _blocksfd = (*_open)(_filePath.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644); //Open file for writing
        if (_blocksfd > -1) {
            uint8_t byte = '\0';
            ftruncate(_blocksfd, _numBlocks * _blockSize * sizeof(uint8_t));
            (*_lseek)(_blocksfd, _numBlocks * _blockSize * sizeof(uint8_t), SEEK_SET);
            (*_write)(_blocksfd, &byte, sizeof(uint8_t));
            //This is so we can read cat the data...
            
            // writeToFile(sizeof(uint8_t), &byte);
            // MemBlockEntry tmp = {};
            // for (uint32_t i = 0; i < _numBlocks; i++) {
            //     writeToFile(sizeof(MemBlockEntry), (uint8_t *)&tmp);
            // }
            // auto ret = (*fsync)(_blocksfd);
            // if (ret != 0) {
            //     std::cerr << _name <<" error syncing to disk!"<<std::endl;
            // }
            (*_lseek)(_blocksfd, 0, SEEK_SET);
            DPRINTF("Created file %s fileName: %s\n", _filePath.c_str(), _fileName.c_str());
        }
        else {
            std::cerr << "[TAZER]"
                      << "Failed to open file cache: " << _filePath << " " << strerror(errno) << std::endl;
        }
    }
    // if (!(*indexInit)) {
    //     // (*_lseek)(_blocksfd, _cacheSize + 1, SEEK_SET);
    //      preadFromFile(_blocksfd,_numBlocks * sizeof(MemBlockEntry), (uint8_t *)_blkIndex, _cacheSize + 1);
    //     *indexInit = true;
    // }
    _binLock->writerUnlock(0);
    stats.end(false, CacheStats::Metric::constructor);
    stats.threadEnd(thread_id, false, CacheStats::Metric::constructor);
}

FileCache::~FileCache() {
    std::thread::id thread_id = std::this_thread::get_id();
    stats.checkThread(thread_id, true);
    stats.threadStart(thread_id);
    stats.start();
    std::cout
        << "[TAZER] " << _name << " deleting" << std::endl;
    for (uint32_t i = 0; i < _numBlocks; i++) {
        if (_blkIndex[i].activeCnt > 0) {
            std::cout << "[TAZER] " << _name << " " << i << " " << _numBlocks << " " << _blkIndex[i].activeCnt << " " << _blkIndex[i].fileIndex << " " << _blkIndex[i].blockIndex << " prefetched: " << _blkIndex[i].prefetched << std::endl;
        }
    }

    // _binLock->writerLock(0);
    // (*_lseek)(_blocksfd, _cacheSize + 1, SEEK_SET);
    // writeToFile(_numBlocks * sizeof(MemBlockEntry), (uint8_t *)_blkIndex);
    // auto ret = (*fsync)(_blocksfd);
    // if (ret != 0) {
    //     std::cerr << _name <<" error syncing to disk!"<<std::endl;
    // }
    // _binLock->writerUnlock(0);
    // std::string cacheName("/" + Config::tazer_id + "_" + _name + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".idx");
    // shm_unlink(cacheName.c_str());
    // _blkIndexfd = -1;
    (*_close)(_blocksfd);
    _blocksfd = -1;
    stats.end(false, CacheStats::Metric::destructor);
    stats.threadEnd(thread_id, false, CacheStats::Metric::destructor);
    stats.print(_name);
    std::cout << std::endl;

    delete _binLock;
}


void FileCache::cleanUpBlockData(uint8_t *data) {
    delete[] data;
}

Cache *FileCache::addNewFileCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string filePath) {
    // std::string newFileName("FileCache");
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [&]() -> Cache * {
            Cache *temp = new FileCache(cacheName, type, cacheSize, blockSize, associativity, filePath);
            return temp;
        });
}

void FileCache::writeToFile(uint64_t size, uint8_t *buff) {
    uint8_t *local = buff;
    while (size) {
        int bytes = (*_write)(_blocksfd, local, size);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            std::cerr <<_name<< " Failed a write " << _blocksfd << " " << size << std::endl;
        }
    }
}

void FileCache::readFromFile(uint64_t size, uint8_t *buff) {
    uint8_t *local = buff;
    while (size) {
        //uint64_t t = Timer::getCurrentTime();
        int bytes = (*_read)(_blocksfd, local, size);
        //readTime.fetch_add(Timer::getCurrentTime() - t);

        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            std::cerr<< _name << " Failed a read " << _blocksfd << " " << size << std::endl;
        }
    }
}

void FileCache::pwriteToFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset) {
    uint8_t *local = buff;
    while (size) {
        int bytes = pwrite(fd, local, size, offset);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
            offset += bytes;
        }
        else {
            std::cerr <<_name<< " Failed a write " << fd << " " << size << std::endl;
        }
    }
}

void FileCache::preadFromFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset) {
    uint8_t *local = buff;
    // auto origSize = size;
    // auto start = Timer::getCurrentTime();
    while (size) {
        // std::cout << "reading " << size << " from " << offset << std::endl;
        int bytes = pread(fd, local, size, offset);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
            offset += bytes;
        }
        else {
           std::cerr<< _name << " Failed a read " << fd << " " << size << std::endl;
        }
    }
    // auto elapsed = Timer::getCurrentTime() - start;
    // updateIoRate(elapsed, origSize);
}

uint8_t *FileCache::getBlockData(unsigned int blockIndex) {
    // uint64_t dstart = Timer::getCurrentTime();
    uint8_t *buff = new uint8_t[_blockSize];
    // (*_lseek)(_blocksfd, blockIndex * _blockSize, SEEK_SET);
    // readFromFile(_blockSize, buff);
    preadFromFile(_blocksfd, _blockSize, buff, blockIndex * _blockSize);
    return buff;
}

void FileCache::setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size) {
    // (*_lseek)(_blocksfd, blockIndex * _blockSize, SEEK_SET);
    // writeToFile(size, data);
    pwriteToFile(_blocksfd, size, data, blockIndex * _blockSize);
    auto ret = (*fsync)(_blocksfd);
    if (ret != 0) {
        std::cerr << _name <<" error syncing to disk!"<<std::endl;
    }
}

void FileCache::blockSet(uint32_t index, uint32_t fileIndex, uint32_t blockIndex, uint8_t status, CacheType type, int32_t prefetched) {
    _blkIndex[index].fileIndex = fileIndex + 1;
    _blkIndex[index].blockIndex = blockIndex + 1;
    _blkIndex[index].timeStamp = Timer::getCurrentTime();
    if (prefetched >= 0) {
        _blkIndex[index].prefetched = prefetched;
    }
    _blkIndex[index].origCache.store(type);
    _blkIndex[index].status = status;
}


// blockAvailable calls are not protected by locks
// we know that when calling blockAvailable the actual data cannot change during the read (due to refernce counting protections)
// what can change is the meta data associated with the last access of the data (and which cache it originated from)
// in certain cases, we want to capture the originating cache and return it for performance reasons
// there is a potential race between when a block is available and when we capture the originating cache
// having origCache be atomic ensures we always get a valid ID (even though it may not always be 100% acurate)
// we are willing to accept some small error in attribution of stats
bool FileCache::blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs, uint32_t cnt, CacheType *origCache) {
    bool avail = _blkIndex[index].status == BLK_AVAIL;
    if (origCache && avail) {
        *origCache = CacheType::empty;
        *origCache = _blkIndex[index].origCache.load();
        // for better or for worse we allow unlocked access to check if a block avail, 
        // there is potential for a race here when some over thread updates the block (not changing the data, just the metadata)
        while(*origCache == CacheType::empty){ 
            *origCache = _blkIndex[index].origCache.load();
        }
        return true;
    }
    return avail;
}

void FileCache::readBlockEntry(uint32_t blockIndex, BlockEntry *entry) {
    *entry = *(BlockEntry *)&_blkIndex[blockIndex];
}
void FileCache::writeBlockEntry(uint32_t blockIndex, BlockEntry *entry) {
    *(BlockEntry *)&_blkIndex[blockIndex] = *entry;
}

void FileCache::readBin(uint32_t binIndex, BlockEntry *entries) {
    int startIndex = binIndex * _associativity;
    for (uint32_t i = 0; i < _associativity; i++) {
        readBlockEntry(i + startIndex, &entries[i]);
    }
}

std::vector<std::shared_ptr<BoundedCache<MultiReaderWriterLock>::BlockEntry>> FileCache::readBin(uint32_t binIndex) {
    std::vector<std::shared_ptr<BlockEntry>> entries;
    int startIndex = binIndex * _associativity;
    for (uint32_t i = 0; i < _associativity; i++) {
        entries.emplace_back(new MemBlockEntry());
        *entries[i].get() = *(BlockEntry *)&_blkIndex[i + startIndex];
    }
    return entries;
}

int FileCache::incBlkCnt(uint32_t blk, Request* req) {
    return _blkIndex[blk].activeCnt.fetch_add(1);
}
int FileCache::decBlkCnt(uint32_t blk, Request* req) {
    return _blkIndex[blk].activeCnt.fetch_sub(1);
}

bool FileCache::anyUsers(uint32_t blk, Request* req) {
    return _blkIndex[blk].activeCnt;
}
