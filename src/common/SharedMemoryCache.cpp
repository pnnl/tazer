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

#include "SharedMemoryCache.h"
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "Message.h"
#include "ReaderWriterLock.h"
#include "Timer.h"
#include "lz4.h"

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <future>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

SharedMemoryCache::SharedMemoryCache(std::string cacheName, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity) : BoundedCache(cacheName, cacheSize, blockSize, associativity) {
    // std::cout<<"[TAZER] " << "Constructing " << _name << " in shared memory cache" << std::endl;
    stats.start();
    std::string filePath("/" + std::string(getenv("USER")) + "_" + _name + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity));

    int fd = shm_open(filePath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
    if (fd == -1) {
        DPRINTF("Reusing shared memory\n");
        log(this) << "Reusing shared memory" << std::endl;
        fd = shm_open(filePath.c_str(), O_RDWR, 0644);
        if (fd != -1) {
            ftruncate(fd, sizeof(uint32_t) + _cacheSize + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins));
            void *ptr = mmap(NULL, sizeof(uint32_t) + _cacheSize + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            uint32_t *init = (uint32_t *)ptr;
            log(this) << "init: " << *init << std::endl;
            while (!*init) {
                sched_yield();
            }
            _blocks = (uint8_t *)init + sizeof(uint32_t);
            _blkIndex = (MemBlockEntry *)((uint8_t *)_blocks + _cacheSize);
            auto binLockDataAddr = (uint8_t *)_blkIndex + _numBlocks * sizeof(MemBlockEntry);
            _binLock = new MultiReaderWriterLock(_numBins, binLockDataAddr);

            log(this) << "init: " << (uint32_t)*init << std::endl;
        }
        else {
            std::cerr << "[TAZER] "
                      << "Error opening shared memory " << strerror(errno) << std::endl;
        }
    }
    else {
        DPRINTF("Created shared memory\n");
        log(this) << _name << "created shared memory" << std::endl;
        ftruncate(fd, sizeof(uint32_t) + _cacheSize + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins));
        void *ptr = mmap(NULL, sizeof(uint32_t) + _cacheSize + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        uint32_t *init = (uint32_t *)ptr;
        log(this) << "init: " << *init << std::endl;
        *init = 0;
        log(this) << "init: " << *init << std::endl;
        _blocks = (uint8_t *)init + sizeof(uint32_t);
        _blkIndex = (MemBlockEntry *)((uint8_t *)_blocks + _cacheSize);
        auto binLockDataAddr = (uint8_t *)_blkIndex + _numBlocks * sizeof(MemBlockEntry);
        _binLock = new MultiReaderWriterLock(_numBins, binLockDataAddr, true);
        _binLock->writerLock(0);
        memset(_blocks, 0, _cacheSize);
        memset(_blkIndex, 0, _numBlocks * sizeof(MemBlockEntry));
        _binLock->writerUnlock(0);
        *init = 1;
        log(this) << "init: " << *init << std::endl;
    }
    log(this) << (void *)_blkIndex << " " << (void *)_binLock << std::endl;

    _shared = true;
    stats.end(false, CacheStats::Metric::constructor);
}

SharedMemoryCache::~SharedMemoryCache() {
    //std::cout<<"[TAZER] " << "deleting " << _name << " in shared memory cache, collisions: " << _collisions << std::endl;
    //std::cout<<"[TAZER] " << "numBlks: " << _numBlocks << " numBins: " << _numBins << " cacheSize: " << _cacheSize << std::endl;
    stats.start();
    if (false) {
        //code from FileCacheRegister...
    }
    for (int i = 0; i < _numBlocks; i++) {
        if (_blkIndex[i].activeCnt > 0) {
            std::cout << "[TAZER] " << _name << " " << i << " " << _numBlocks << " " << _blkIndex[i].activeCnt << " " << _blkIndex[i].fileIndex - 1 << " " << _blkIndex[i].blockIndex - 1 << " "
                      << "prefetched" << _blkIndex[i].prefetched << std::endl;
        }
    }
    stats.end(false, CacheStats::Metric::destructor);
    stats.print(_name);
    std::cout << std::endl;
}

void SharedMemoryCache::setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size) {
    uint64_t dstart = Timer::getCurrentTime();
    // std::cout << "[TAZER] " << _name << " setting block: " << blockIndex << std::endl;
    // std::cerr << "bi: " << blockIndex << " bs: " << _blockSize << " " << blockIndex * _blockSize << " s: " << size << " " << (void *)data << std::endl;
    memcpy(&_blocks[blockIndex * _blockSize], data, size);
    auto elapsed = Timer::getCurrentTime() - dstart;
    // _dataAmt += _blockSize;
    // _dataTime += elapsed;
    // updateIoRate(elapsed,_blockSize);
}
uint8_t *SharedMemoryCache::getBlockData(unsigned int blockIndex) {
    // auto dstart = Timer::getCurrentTime();
    // std::cout << "[TAZER] " << _name << " getting block: " << blockIndex << std::endl;
    // auto start = Timer::getCurrentTime();
    uint8_t *temp = (uint8_t *)&_blocks[blockIndex * _blockSize];
    // auto elapsed = Timer::getCurrentTime()-start;
    // updateIoRate(elapsed,_blockSize);
    // std::cerr << "bi: " << blockIndex << " bs: " << _blockSize << " " << blockIndex * _blockSize << " " << (void *)temp << std::endl;
    // _dataTime += Timer::getCurrentTime() - dstart;
    // _dataAmt += _blockSize;
    return temp;
}

void SharedMemoryCache::blockSet(uint32_t index, uint32_t fileIndex, uint32_t blockIndex, uint8_t status, int32_t prefetched, std::string cacheName) {
    _blkIndex[index].fileIndex = fileIndex + 1;
    _blkIndex[index].blockIndex = blockIndex + 1;
    _blkIndex[index].timeStamp = Timer::getCurrentTime();
    if (prefetched >= 0) {
        _blkIndex[index].prefetched = prefetched;
    }
    memset(_blkIndex[index].origCache, 0, MAX_CACHE_NAME_LEN);
    memcpy(_blkIndex[index].origCache, cacheName.c_str(), MAX_CACHE_NAME_LEN);
    _blkIndex[index].status = status;
}

bool SharedMemoryCache::blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs, uint32_t cnt, char *origCache) {
    bool avail = _blkIndex[index].status == BLK_AVAIL;
    if (origCache && avail) {
        memset(origCache, 0, MAX_CACHE_NAME_LEN);
        memcpy(origCache, _blkIndex[index].origCache, MAX_CACHE_NAME_LEN);
        // std::cout << _name << " " << _blkIndex[index].origCache << std::endl;
        return true;
    }
    return avail;
}

void SharedMemoryCache::readBlockEntry(uint32_t blockIndex, BlockEntry *entry) {

    *entry = *(BlockEntry *)&_blkIndex[blockIndex];

    // memcpy(entry, &_blkIndex[blockIndex], sizeof(BlockEntry));
}
void SharedMemoryCache::writeBlockEntry(uint32_t blockIndex, BlockEntry *entry) {
    *(BlockEntry *)&_blkIndex[blockIndex] = *entry;
    // memcpy(&_blkIndex[blockIndex], entry, sizeof(BlockEntry));
}

void SharedMemoryCache::readBin(uint32_t binIndex, BlockEntry *entries) {
    // memcpy(entries, &_blkIndex[binIndex * _associativity], sizeof(BlockEntry) * _associativity);
    int startIndex = binIndex * _associativity;
    for (int i = 0; i < _associativity; i++) {
        readBlockEntry(i + startIndex, &entries[i]);
    }
}
std::vector<std::shared_ptr<BoundedCache<MultiReaderWriterLock>::BlockEntry>> SharedMemoryCache::readBin(uint32_t binIndex) {
    // memcpy(entries, &_blkIndex[binIndex * _associativity], sizeof(BlockEntry) * _associativity);
    std::vector<std::shared_ptr<BlockEntry>> entries;
    int startIndex = binIndex * _associativity;
    for (int i = 0; i < _associativity; i++) {
        // entries.emplace_back(std::make_shared<BlockEntry>(*(BlockEntry*)&_blkIndex[i+startIndex]));
        entries.emplace_back(new MemBlockEntry());
        *entries[i].get() = *(BlockEntry *)&_blkIndex[i + startIndex];
        // readBlockEntry(i+startIndex,&entries[i]);
    }
    return entries;
}

int SharedMemoryCache::incBlkCnt(uint32_t blk) {
    return _blkIndex[blk].activeCnt.fetch_add(1);
}
int SharedMemoryCache::decBlkCnt(uint32_t blk) {
    return _blkIndex[blk].activeCnt.fetch_sub(1);
}

bool SharedMemoryCache::anyUsers(uint32_t blk) {
    return _blkIndex[blk].activeCnt;
}

Cache *SharedMemoryCache::addNewSharedMemoryCache(std::string cacheName, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity) {
    //std::string newFileName("SharedMemoryCache");
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [&]() -> Cache * {
            Cache *temp = new SharedMemoryCache(cacheName, cacheSize, blockSize, associativity);
            return temp;
        });
}
