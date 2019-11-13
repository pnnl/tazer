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

#include "Cache.h"
#include "Config.h"
#include "ThreadPool.h"
#include "Timer.h"
#include "xxhash.h"
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <future>
#include <iomanip>
#include <signal.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

// ThreadPool<std::function<void()>> *Cache::_writePool = NULL;
// PriorityThreadPool<std::function<void()>> Cache::_prefetchPool(1);
// std::mutex Cache::_pMutex;
// std::unordered_set<std::string> Cache::_prefetches;

Cache::Cache(std::string name) : Loggable(Config::CacheLog, name),
                                 _ioTime(0),
                                 _ioAmt(0),
                                 _name(name),
                                 _level(0),
                                 _nextLevel(NULL),
                                 _lastLevel(this),
                                 _shared(false),
                                 _outstandingWrites(0),
                                 _terminating(false) {
    stats.start();

    // std::cout << "[TAZER] "
    //           << "Constructing " << _name << " in cache" << std::endl;
    if (_name == BASECACHENAME) {
        // _fm_lock = new ReaderWriterLock();
        _writePool = new ThreadPool<std::function<void()>>(Config::numWriteBufferThreads, "write pool");
        _writePool->initiate();
        _prefetchPool = new PriorityThreadPool<std::function<void()>>(Config::numPrefetchThreads, "prefetch pool");
        _prefetchPool->initiate();
        _base = this;
        _lastLevel = this;
    }
    // std::string shmPath("/" + std::string(getenv("USER")) + _name + "_stats.lck");
    // int shmFd = shm_open(shmPath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
    // if (shmFd == -1) {
    //     shmFd = shm_open(shmPath.c_str(), O_RDWR, 0644);
    //     if (shmFd != -1) {
    //         std::cout << "resusing fcntl shm lock" << std::endl;
    //         ftruncate(shmFd, sizeof(uint32_t) + 2 * sizeof(uint64_t) + 2 * sizeof(uint8_t) + _ioWinSize * sizeof(uint64_t));
    //         void *ptr = mmap(NULL, sizeof(uint32_t) + 2 * sizeof(uint64_t) + 2 * sizeof(uint8_t) + _ioWinSize * sizeof(uint64_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    //         uint32_t *init = (uint32_t *)ptr;
    //         while (!*init) {
    //             sched_yield();
    //         }
    //         _curIoTime = (std::atomic<uint64_t> *)((uint8_t *)init + sizeof(uint32_t));
    //         _curIoAmt = (std::atomic<uint64_t> *)((uint8_t *)_curIoTime + sizeof(uint64_t));
    //         _ioIndex = (std::atomic<uint8_t> *)((uint8_t *)_curIoAmt + sizeof(uint64_t));
    //         _ioCnt = (std::atomic<uint8_t> *)((uint8_t *)_ioIndex + sizeof(uint8_t));
    //         _ioTimes = (uint64_t *)((uint8_t *)_ioCnt + sizeof(uint8_t));
    //     }
    //     else {
    //         std::cerr << "[TAZER]"
    //                   << "Error opening shared memory " << strerror(errno) << std::endl;
    //     }
    // }
    // else {
    //     std::cout << "creating fcntl shm lock" << std::endl;
    //     ftruncate(shmFd, sizeof(uint32_t) + 2 * sizeof(uint64_t) + 2 * sizeof(uint8_t) + _ioWinSize * sizeof(uint64_t));
    //     void *ptr = mmap(NULL, sizeof(uint32_t) + 2 * sizeof(uint64_t) + 2 * sizeof(uint8_t) + _ioWinSize * sizeof(uint64_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);

    //     uint32_t *init = (uint32_t *)ptr;
    //     _curIoTime = (std::atomic<uint64_t> *)((uint8_t *)init + sizeof(uint32_t));
    //     std::atomic_init(_curIoTime, (uint64_t)0);
    //     _curIoAmt = (std::atomic<uint64_t> *)((uint8_t *)_curIoTime + sizeof(uint64_t));
    //     std::atomic_init(_curIoAmt, (uint64_t)0);
    //     _ioIndex = (std::atomic<uint8_t> *)((uint8_t *)_curIoAmt + sizeof(uint64_t));
    //     std::atomic_init(_ioIndex, (uint8_t)0);
    //     _ioCnt = (std::atomic<uint8_t> *)((uint8_t *)_ioIndex + sizeof(uint8_t));
    //     std::atomic_init(_ioCnt, (uint8_t)0);
    //     _ioTimes = (uint64_t *)((uint8_t *)_ioCnt + sizeof(uint8_t));
    //     memset(_ioTimes, 0, _ioWinSize * sizeof(uint64_t));
    //     *init = 1;
    // }

    void *ptr = malloc(sizeof(uint32_t) + 2 * sizeof(uint64_t) + 2 * sizeof(uint8_t) + _ioWinSize * sizeof(uint64_t));

    uint32_t *init = (uint32_t *)ptr;
    _curIoTime = (std::atomic<uint64_t> *)((uint8_t *)init + sizeof(uint32_t));
    std::atomic_init(_curIoTime, (uint64_t)0);
    _curIoAmt = (std::atomic<uint64_t> *)((uint8_t *)_curIoTime + sizeof(uint64_t));
    std::atomic_init(_curIoAmt, (uint64_t)0);
    _ioIndex = (std::atomic<uint8_t> *)((uint8_t *)_curIoAmt + sizeof(uint64_t));
    std::atomic_init(_ioIndex, (uint8_t)0);
    _ioCnt = (std::atomic<uint8_t> *)((uint8_t *)_ioIndex + sizeof(uint8_t));
    std::atomic_init(_ioCnt, (uint8_t)0);
    _ioTimes = (uint64_t *)((uint8_t *)_ioCnt + sizeof(uint8_t));
    memset(_ioTimes, 0, _ioWinSize * sizeof(uint64_t));
    *init = 1;

    memset(_ioTimes, 0, _ioWinSize * sizeof(uint64_t));
    memset(_ioAmts, 0, _ioWinSize * sizeof(uint64_t));
    stats.end(false, CacheStats::Metric::constructor);
}

Cache::~Cache() {
    log(this) << "deleting " << _name << " in cache" << std::endl;
    stats.start();
    while (_outstandingWrites.load()) {
        std::this_thread::yield();
    }

    _terminating = true;

    if (_name == BASECACHENAME) {
        log(this) << std::endl;

        _prefetchPool->terminate();
        while (_outstandingWrites.load()) {
            std::this_thread::yield();
        }
        _writePool->terminate();
        delete _prefetchPool;
        delete _writePool;
        // delete _fm_lock;
        stats.end(false, CacheStats::Metric::destructor);
        stats.print(_name);
        std::cout << std::endl;
    }

    if (_nextLevel) {
        std::cout << "going to delete next level" << std::endl;
        delete _nextLevel;
    }
    std::string shmPath("/" + std::string(getenv("USER")) + _name + "_stats.lck");
    shm_unlink(shmPath.c_str());
}

bool Cache::blockReserve(uint32_t index, uint32_t fileIndex, int &reservedIndex, bool prefetch) {
    return false;
}

//Must lock first!
void Cache::blockSet(uint32_t index, uint32_t fileIndex, uint32_t blockIndex) {
}

bool Cache::writeBlock(Request *req) {

    if (_nextLevel) {
        // std::cout<<"[TAZER] " << _name << " writeblock " << std::hex << (void *)buffer << std::dec << std::endl;
        return _nextLevel->writeBlock(req);
    }
    return false;
}

Request *Cache::requestBlock(uint32_t index, uint64_t &size, uint32_t fileIndex, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority) {
    Request *req = new Request(index, fileIndex, size, this, NULL);
    Cache *tmp = this;
    while (tmp) {
        req->reservedMap[tmp] = 0;
        tmp = tmp->getNextLevel();
    }
    req->ready = false;
    log(this) << _name << " " << _nextLevel->name() << std::endl;
    if (_nextLevel) {
        _nextLevel->readBlock(req, reads, priority);
    }
    // log(this) << "req: " << (void *)req << std::endl;
    return req;
}

void Cache::readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority) {
    if (_nextLevel) {
        _nextLevel->readBlock(req, reads, priority);
    }
}

//TODO: merge/reimplement from old cache structure
void Cache::cleanUpBlockData(uint8_t *data) {
}

uint32_t Cache::numBlocks() {
    return 0;
}

void Cache::addCacheLevel(Cache *cache, uint64_t level) {
    // std::cout<<"[TAZER] " << "adding level: " << level << " " << (void *)this << " " << (void *)cache << std::endl;
    //loop through to add at proper level;
    if (_level != level - 1) {
        _lastLevel = cache;
        _nextLevel->addCacheLevel(cache, level);
    }
    else {
        cache->setLevel(level);
        _nextLevel = cache;
        _lastLevel = cache;
        cache->setBase(_base);
    }
}

Cache *Cache::getCacheAtLevel(uint64_t level) {
    if (level == _level) {
        return this;
    }
    else if (_nextLevel) {
        return _nextLevel->getCacheAtLevel(level);
    }
    else {
        return NULL;
    }
}

Cache *Cache::getCacheByName(std::string name) {
    if (name == _name) {
        return this;
    }
    else if (_nextLevel) {
        return _nextLevel->getCacheByName(name);
    }
    else {
        return NULL;
    }
}

Cache *Cache::getNextLevel() {
    return _nextLevel;
}

void Cache::setLevel(uint64_t level) {
    _level = level;
}
void Cache::setBase(Cache *base) {
    _base = base;
}

//TODO: merge/reimplement from old cache structure
void Cache::cleanReservation() {
}

bool Cache::bufferWrite(Request *req) {
    //std::cout<<"[TAZER] " << "buffered write: " << (void *)originating << std::endl;
    if (Config::bufferFileCacheWrites) { // && !_terminating) {
        _outstandingWrites.fetch_add(1);
        _writePool->addTask([this, req] {
            //std::cout<<"[TAZER] " << "buffered write: " << (void *)originating << std::endl;
            if (!writeBlock(req)) {
                DPRINTF("FAILED WRITE...\n");
            }
            _outstandingWrites.fetch_sub(1);
        });
        return false;
    }

    if (!req->data) //This will cause a seg fault instead of infinitely spinning
        raise(SIGSEGV);

    if (!writeBlock(req)) {
        DPRINTF("FAILED WRITE...\n");
    }

    return true;
}


void Cache::updateRequestTime(uint64_t time) {
    if (_ioCnt->load() < _ioWinSize) {
        _ioCnt->fetch_add(1);
    }
    // log(this) << _name << " " << (uint32_t)_ioCnt->load() << std::endl;
    uint8_t myIndex = _ioIndex->fetch_add(1) % _ioWinSize;
    _curIoTime->fetch_sub(_ioTimes[myIndex]);
    _ioTimes[myIndex] = time;
    _curIoTime->fetch_add(time);
}

double Cache::getRequestTime() {

    if (_ioCnt->load() == 0) {
        // log(this) << _name << " " << (_curIoTime / 1000000000.0) / 0.0 << std::endl;
        return 0.0;
    }
    // log(this) << _name << " " << (_curIoTime / 1000000000.0) / _ioCnt << std::endl;
    return (_curIoTime->load() / 1000000000.0) / _ioCnt->load();
}


void Cache::addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize) {
    // std::cout<<"[TAZER] " << "adding file: " << filename << " " << (void *)this << " " << (void *)_nextLevel << std::endl;
    log(this) << "adding" << _name << " " << filename << " " << fileSize << " " << blockSize << std::endl;
    if (_nextLevel) {
        _nextLevel->addFile(index, filename, blockSize, fileSize);
    }
}

void Cache::prefetchBlocks(uint32_t index, std::vector<uint64_t> blocks, uint64_t fileSize, uint64_t blkSize, uint64_t regFileIndex) {
    _prefetchPool->addTask(0, [this, index, blocks, fileSize, blkSize, regFileIndex] {
        prefetch(index, blocks, fileSize, blkSize, regFileIndex);
    });
}

void Cache::prefetch(uint32_t index, std::vector<uint64_t> blocks, uint64_t fileSize, uint64_t blkSize, uint64_t regFileIndex) {
    std::string sIndex = std::to_string(index) + "-";
    std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> reads;
    std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> net_reads;
    std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> local_reads;

    int numBlks = blocks.size();
    uint64_t startBlk = blocks[0];

    for (auto blk : blocks) {
        uint32_t priority = 1 + (blk - startBlk);
        auto request = requestBlock(blk, blkSize, regFileIndex, reads, priority);
        if (request->ready) { //the block was in a client side cache!!
            //std::cout << "********************Data was on client side!!!" <<std::endl;
            bufferWrite(request);
            request->originating->stats.addAmt(true, CacheStats::Metric::read, blkSize);
        }
        else {
            //std::cout << "********************Data prerequested!!!" <<std::endl;

            if (request->originating->name() == NETWORKCACHENAME) {
                net_reads.insert(std::make_pair(blk, reads[blk]));
            }
            else {
                local_reads.insert(std::make_pair(blk, reads[blk]));
            }
        }
    }

    for (auto it = net_reads.begin(); it != net_reads.end(); ++it) {
        uint32_t blk = (*it).first;
        auto request = (*it).second.get().get(); //need to do two gets cause we cant chain futures properly yet (c++ 2x supposedly)

        if (request->data) { // hmm what does it mean if this is NULL? do we need to catch and report this?
            bufferWrite(request);
            request->originating->stats.addAmt(true, CacheStats::Metric::read, blkSize);
            stats.addAmt(true, CacheStats::Metric::read, blkSize);
        }
    }
    for (auto it = local_reads.begin(); it != local_reads.end(); ++it) {
        uint32_t blk = (*it).first;
        auto request = (*it).second.get().get(); //need to do two gets cause we cant chain futures properly yet (c++ 2x supposedly)
        if (request->data) {                     // hmm what does it mean if this is NULL? do we need to catch and report this?
            bufferWrite(request);
            request->originating->stats.addAmt(true, CacheStats::Metric::read, blkSize);
            stats.addAmt(true, CacheStats::Metric::read, blkSize);
        }
    }
}
