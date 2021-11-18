// -*-Mode: C++;-*- // technically C99

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

#include "Config.h"
#include "ScalableCache.h"
#include "StealingAllocator.h"
#include "Timer.h"
#include <signal.h>
#include "xxhash.h"
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <future>
#include <algorithm>
#include <memory>
#include <deque>
#include <queue>
#include <cmath>
#include <cfloat>

#define DPRINTF(...)

// #define DPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)
#define PPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

ScalableCache::ScalableCache(std::string cacheName, CacheType type, uint64_t blockSize, uint64_t maxCacheSize) : 
Cache(cacheName, type),
evictHisto(100),
access(0), 
misses(0),
startTimeStamp((uint64_t)Timer::getCurrentTime()) {
    stats.start();
    log(this) << _name << std::endl;
    _cacheLock = new ReaderWriterLock();
    _blockSize = blockSize;

    if(Config::scalableCacheAllocator == 0) {
        PPRINTF("[JS] AdaptiveAllocator::addAdaptiveAllocator\n");
        _allocator = AdaptiveAllocator::addAdaptiveAllocator(blockSize, maxCacheSize, this);
    }
    else if(Config::scalableCacheAllocator == 1) {
        PPRINTF("[JS] StealingAllocator::addStealingAllocator\n");
        _allocator = StealingAllocator::addStealingAllocator(blockSize, maxCacheSize, this);
    }
    else if(Config::scalableCacheAllocator == 2) {
        PPRINTF("[JS] StealingAllocator::addRandomStealingAllocator false\n");
        _allocator = RandomStealingAllocator::addRandomStealingAllocator(blockSize, maxCacheSize, this, false);
    }
    else if(Config::scalableCacheAllocator == 3) {
        PPRINTF("[JS] StealingAllocator::addRandomStealingAllocator true\n");
        _allocator = RandomStealingAllocator::addRandomStealingAllocator(blockSize, maxCacheSize, this, true);
    }
    else if(Config::scalableCacheAllocator == 4) {
        PPRINTF("[JS] StealingAllocator::addLargestStealingAllocator\n");
        _allocator = LargestStealingAllocator::addLargestStealingAllocator(blockSize, maxCacheSize, this);
    }
    else if(Config::scalableCacheAllocator == 5) {
        PPRINTF("[JS] StealingAllocator::addFirstTouchAllocator\n");
        _allocator = FirstTouchAllocator::addFirstTouchAllocator(blockSize, maxCacheSize);
    }
    else {
        PPRINTF("[JS] SimpleAllocator::addSimpleAllocator\n");
        _allocator = SimpleAllocator::addSimpleAllocator(blockSize, maxCacheSize);
    }

    srand(time(NULL));
    stats.end(false, CacheStats::Metric::constructor);
}

ScalableCache::~ScalableCache() {
    _terminating = true;
    log(this) << "deleting " << _name << " in ScalableCache" << std::endl;
    delete _cacheLock;
    evictHisto.printBins();
    stats.print(_name);
}

Cache* ScalableCache::addScalableCache(std::string cacheName, CacheType type, uint64_t blockSize, uint64_t maxCacheSize) {
    bool created = false;
    auto ret = Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [&]() -> Cache * {
            Cache *temp = new ScalableCache(cacheName, type, blockSize, maxCacheSize);
            return temp;
        }, created);
    return ret;
}

void ScalableCache::addFile(uint32_t fileIndex, std::string filename, uint64_t blockSize, std::uint64_t fileSize) {
    trackBlock(_name, "[ADD_FILE]", fileIndex, blockSize, fileSize);

    _cacheLock->writerLock();
    if (_fileMap.count(fileIndex) == 0) {
        std::string hashstr(_name + filename);
        uint64_t hash = (uint64_t)XXH32(hashstr.c_str(), hashstr.size(), 0);
        _fileMap.emplace(fileIndex, FileEntry{filename, blockSize, fileSize, hash});
        _metaMap[fileIndex] = new ScalableMetaData(blockSize, fileSize);
        //_allocator->openFile(_metaMap[fileIndex]);
        DPRINTF("[JS] ScalableCache::addFile %s %u\n", filename.c_str(), fileIndex);
    }
    _cacheLock->writerUnlock();
    
    if (_nextLevel) {
        _nextLevel->addFile(fileIndex, filename, blockSize, fileSize);
    }
}

void ScalableCache::closeFile(uint32_t fileIndex) {
    DPRINTF("[JS] ScalableCache::closeFile %u\n", fileIndex);
    _cacheLock->readerLock();
    _allocator->closeFile(_metaMap[fileIndex]);
    _cacheLock->readerUnlock();
}

uint8_t * ScalableCache::getBlockDataOrReserve(uint32_t fileIndex, uint64_t blockIndex, uint64_t fileOffset, bool &reserve) {
    _cacheLock->readerLock();
    auto ret = _metaMap[fileIndex]->getBlockData(blockIndex, fileOffset, reserve, true);
    _cacheLock->readerUnlock();
    DPRINTF("[JS] ScalableCache::getBlockDataOrReserve %u\n", reserve);
    return ret;
    // reserve = true;
    // return NULL;
}

uint8_t * ScalableCache::getBlockData(uint32_t fileIndex, uint64_t blockIndex, uint64_t fileOffset) {
    bool dontCare;
    _cacheLock->readerLock();
    auto ret = _metaMap[fileIndex]->getBlockData(blockIndex, fileOffset, dontCare, false);
    _cacheLock->readerUnlock();
    return ret;
}

void ScalableCache::setBlock(uint32_t fileIndex, uint64_t blockIndex, uint8_t * data, uint64_t dataSize) {
    //JS: For trackBlock
    uint64_t sourceBlockIndex;
    uint32_t sourceFileIndex;
    
    _cacheLock->readerLock();
    auto meta = _metaMap[fileIndex];
    uint8_t * dest = NULL;
    auto doAlloc = meta->checkPattern(this, fileIndex);

    while(!dest) {
        //JS: Are we allowed to get more blocks
        auto must = (meta->getNumBlocks()>0 ? false : true );
        if(must && doAlloc) { //doAlloc is left here but shouldn't affect the if check 
            DPRINTF("[JS] ScalableCache::setBlock new block\n");
            dest = _allocator->allocateBlock(fileIndex, must);
        }

        //JS: We can't due to our pattern so lets LRU ourself
        if(!dest) {
            DPRINTF("[JS] ScalableCache::setBlock Reusing block\n");
            dest = meta->oldestBlock(sourceBlockIndex);
            if(dest)
                trackBlockEviction(fileIndex, sourceBlockIndex);
        }

        //JS: Try to backout
        if(!dest && meta->backOutOfReservation(blockIndex)) {
            DPRINTF("[JS] ScalableCache::setBlock Backing out\n");
            stats.addAmt(false, CacheStats::Metric::backout, 1);
            break;
        }

    }
    
    if(dest) {
        DPRINTF("[JS] ScalableCache::setBlock got dest\n");
        //JS: Copy the block back to our cache
        memcpy(dest, data, dataSize);
        meta->setBlock(blockIndex, dest);
        trackBlock(_name, "[BLOCK_WRITE]", fileIndex, blockIndex, 0);
    }
    else {
        DPRINTF("[Tazer] Allocation failed, potential race condition!\n");
    }
    _cacheLock->readerUnlock();
}

bool ScalableCache::writeBlock(Request *req){
    DPRINTF("[JS] ScalableCache::writeBlock start\n");
    req->time = Timer::getCurrentTime();
    bool ret = false;
    if (req->originating == this) {
        DPRINTF("[JS] ScalableCache::writeBlock hit cleanup\n");
        //JS: Decrement our block usage
        _cacheLock->readerLock();
        _metaMap[req->fileIndex]->decBlockUsage(req->blkIndex);
        _cacheLock->readerUnlock();
        delete req;
        ret = true;
    }
    else {
        DPRINTF("[JS] ScalableCache::writeBlock pass cleanup\n");
        //JS: Write block to cache
        setBlock(req->fileIndex, req->blkIndex, req->data, req->size);
        if (_nextLevel) {
            ret &= _nextLevel->writeBlock(req);
        }
    }
    DPRINTF("[JS] ScalableCache::writeBlock done\n");
    return ret;
}

void ScalableCache::readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority){
    stats.start(); //read
    uint8_t *buff = nullptr;
    auto index = req->blkIndex;
    auto fileIndex = req->fileIndex;
    auto fileOffset = req->offset;

    if (!req->size) { //JS: This get the blockSize from the file
        _cacheLock->readerLock();
        req->size = _fileMap[fileIndex].blockSize;
        _cacheLock->readerUnlock();
    }
    
    DPRINTF("[JS] ScalableCache::readBlock req->size: %lu req->blkIndex: %lu\n", req->size, req->blkIndex);
    trackBlock(_name, (priority != 0 ? " [BLOCK_PREFETCH_REQUEST] " : " [BLOCK_REQUEST] "), fileIndex, index, priority);

    if (req->size <= _blockSize) {
        bool reserve;
        //JS: Fill in the data if we have it
        buff = getBlockDataOrReserve(fileIndex, index, fileOffset, reserve);
        access.fetch_add(1);

        // std::cerr << " [JS] readBlock " << buff << " " << reserve << std::endl;
        if(buff) {
            DPRINTF("[JS] ScalableCache::readBlock hit\n");
            //JS: Update the req, block is currently held by count
            req->data=buff;
            req->originating=this;
            req->reservedMap[this]=1;
            req->ready = true;
            req->time = Timer::getCurrentTime() - req->time;
            updateRequestTime(req->time);

            trackBlock(_name, "[BLOCK_READ_HIT]", fileIndex, index, priority);
            stats.addAmt(false, CacheStats::Metric::hits, req->size);
            stats.end(false, CacheStats::Metric::hits);
        }
        else { //JS: Data not currently present
            trackBlock(_name, "[BLOCK_READ_MISS_CLIENT]", fileIndex, index, priority);
            misses.fetch_add(1);
            stats.addAmt(false, CacheStats::Metric::misses, 1);
            stats.end(false, CacheStats::Metric::misses);

            if (reserve) { //JS: We reserved block
                DPRINTF("[JS] ScalableCache::readBlock reseved\n");
                _nextLevel->readBlock(req, reads, priority);
            }
            else { //JS: Someone else will fullfill request
                DPRINTF("[JS] ScalableCache::readBlock waiting\n");
                auto fut = std::async(std::launch::deferred, [this, req] {
                    uint64_t offset = req->offset;
                    auto blockIndex = req->blkIndex;
                    auto fileIndex = req->fileIndex;

                    //JS: Wait for data to be ready
                    uint8_t * buff = getBlockData(fileIndex, blockIndex, offset);
                    while (!buff) {
                        sched_yield();
                        buff = getBlockData(fileIndex, blockIndex, offset);
                    }

                    //JS: Update the req
                    req->data=buff;
                    req->originating=this;
                    req->reservedMap[this]=1;
                    req->ready = true;

                    //JS TODO: Check if this is right and add to unbounded cache
                    req->waitingCache = _lastLevel->type();
                    req->time = Timer::getCurrentTime()-req->time;
                    updateRequestTime(req->time);

                    //JS: Future of a future required by network cache impl
                    std::promise<Request*> prom;
                    auto innerFuture = prom.get_future();
                    prom.set_value(req);
                    return innerFuture.share();
                });
                reads[index] = fut.share();
            }
        }
    }
    else {
        std::cerr << "[TAZER] We do not currently support blockSizes bigger than cache size!" << std::endl;
        raise(SIGSEGV);
    }
    DPRINTF("[JS] ScalableCache::readBlock done\n");
}

ScalableMetaData * ScalableCache::oldestFile(uint32_t &oldestFileIndex) {
    _cacheLock->readerLock();
    uint64_t minTime = (uint64_t)-1;
    oldestFileIndex = (uint32_t) -1;
    for (auto const &x : _metaMap) {
        auto fileIndex = x.first;
        auto meta = x.second;
        uint64_t temp = meta->getLastTimeStamp();
        if(temp < minTime) {
            minTime = temp;
            oldestFileIndex = fileIndex;
        }
    }

    ScalableMetaData * ret = NULL;
    if(oldestFileIndex != (uint64_t)-1) {
        ret = _metaMap[oldestFileIndex];
    }
    _cacheLock->readerUnlock();
    return ret;
}

ScalableMetaData * ScalableCache::findVictim(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, bool mustSucceed) {
    _cacheLock->readerLock();
    DPRINTF("---------- %u of %u ----------\n", allocateForFileIndex, _metaMap.size());
    //JS: For calcRank
    auto localMisses = misses.load();
    uint64_t timestamp = Timer::getCurrentTime();

    //JS: Find unit marginal benefits
    double sourceFileRank;
    double minRank = std::numeric_limits<double>::max();
    uint32_t minFileIndex = (uint32_t) -1;
    //Burcu: if we have streaming pattern files, they are priority victim for stealing, so keep track of minimum ranked streaming pattern
    double minStreamRank = std::numeric_limits<double>::max();
    uint32_t minStreamFileIndex = (uint32_t) -1;


    //JS: Find lowest rank
    DPRINTF("+++++++++++Starting Search\n");
    for (auto const &x : _metaMap) {
        auto fileIndex = x.first;
        auto meta = x.second;
        if(allocateForFileIndex != fileIndex) {
            auto temp = meta->calcRank(timestamp-startTimeStamp, localMisses);
            DPRINTF("-------Index: %u %lf\n", fileIndex, temp);
            if(!std::isnan(temp) && temp > 0 && temp < minRank) {
                minRank = temp;
                minFileIndex = fileIndex;
                DPRINTF("*******NEW MIN: %u : %lf\n", minFileIndex, minRank);
            }
            //second check for minimum streaming pattern search 
            //Burcu TODO : stridepattern[blockstreaming] = 1 , fixme so enum order won't chenge the code!!
            if(meta->getPattern() == 1 && !std::isnan(temp) && temp > 0 && temp < minStreamRank) {
                minStreamRank = temp;
                minStreamFileIndex = fileIndex;
                DPRINTF("*******NEW STREAM MIN: %u : %lf\n", minStreamFileIndex, minStreamRank);
            }
        }
        else {
            sourceFileRank = meta->calcRank(timestamp-startTimeStamp, localMisses);
            DPRINTF("-------Source Index: %u %lf\n", fileIndex, sourceFileRank);
        }
    }

    //if we fond a streaming pattern file, select that one as the victim
    if(minStreamFileIndex!= (uint32_t) -1) {
        minFileIndex = minStreamFileIndex;
        minRank = minStreamRank;
        DPRINTF("********STREAMING VICTIM: %u : %lf\n", minFileIndex, minFileRank);
    }
    DPRINTF("+++++++++++End Search\n");
    ScalableMetaData * ret = NULL;
    sourceFileIndex = (uint32_t) -1;
    if(minFileIndex != (uint32_t) -1) {
        if(mustSucceed || std::isnan(sourceFileRank) || sourceFileRank > minRank) {
            ret = _metaMap[minFileIndex];
            //JS: Do update rank here
            DPRINTF("**************SOURCE: %u DEST: %u\n", minFileIndex, allocateForFileIndex);
            _metaMap[minFileIndex]->updateRank(true);
            _metaMap[allocateForFileIndex]->updateRank(false);
            //JS: Set this for the tracking
            sourceFileIndex = minFileIndex;
        }
    }
    _cacheLock->readerUnlock();
    return ret;
}

ScalableMetaData * ScalableCache::randomFile(uint32_t &sourceFileIndex) {
    _cacheLock->readerLock();
    ScalableMetaData * ret = NULL;
    if(!_metaMap.empty()) {
        sourceFileIndex = rand() % _metaMap.size();
        ret = _metaMap[sourceFileIndex];
    }
    _cacheLock->readerUnlock();
    return ret;
}

ScalableMetaData * ScalableCache::largestFile(uint32_t &largestFileIndex) {
    _cacheLock->readerLock();
    uint64_t maxBlocks = 0;
    largestFileIndex = (uint32_t) -1;
    for (auto const &x : _metaMap) {
        auto fileIndex = x.first;
        auto meta = x.second;
        uint64_t temp = meta->getNumBlocks();
        if(maxBlocks < temp) {
            maxBlocks = temp;
            largestFileIndex = fileIndex;
        }
    }

    ScalableMetaData * ret = NULL;
    if(largestFileIndex != (uint64_t)-1) {
        ret = _metaMap[largestFileIndex];
    }
    _cacheLock->readerUnlock();
    return ret;
}

//JS: Wrote this function for allocator to track an eviction (trackBlock is private)
void ScalableCache::trackBlockEviction(uint32_t fileIndex, uint64_t blockIndex) {
    stats.addAmt(false, CacheStats::Metric::evictions, 1);
    evictHisto.addData((double) fileIndex, (double) 1);
    trackBlock(_name, "[BLOCK_EVICTED]", fileIndex, blockIndex, 0);
}

void ScalableCache::trackPattern(uint32_t fileIndex, std::string pattern) {
    DPRINTF("SETTING PATTER %s\n", pattern.c_str());
    trackBlock(_name, pattern, fileIndex, -1, -1);
}
