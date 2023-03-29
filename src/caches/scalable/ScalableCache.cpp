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

#define DEADLOCK_SAFEGUARD 100

#define DPRINTF(...)
//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

//#define MeMPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)
#define MeMPRINTF(...)
#define BPRINTF(...) //fprintf(stderr, __VA_ARGS__); fflush(stderr)
//#define BPRINTF(...)
#define StealPRINTF(...) //fprintf(stderr, __VA_ARGS__); fflush(stderr)
ScalableCache::ScalableCache(std::string cacheName, CacheType type, uint64_t blockSize, uint64_t maxCacheSize) : 
Cache(cacheName, type),
evictHisto(100),
access(0), 
misses(0),
resizeNumbers(0),
resizePercentage(20),
startTimeStamp((uint64_t)Timer::getCurrentTime()),
oustandingBlocksRequested(0),
maxOutstandingBlocks(0),
maxBlocksInUse(0){
    stats.start(false, CacheStats::Metric::constructor);
    log(this) << _name << std::endl;
    _lastVictimFileIndexLock = new ReaderWriterLock();
    _cacheLock = new ReaderWriterLock();
    _blockSize = blockSize;
    _numBlocks = maxCacheSize / blockSize;
    resizeAmount = _numBlocks * resizePercentage / 100; 
    if(Config::scalableCacheAllocator == 0) {
        DPRINTF("[JS] AdaptiveForceWithUMBAllocator::addAdaptiveForceWithUMBAllocator\n");
        _allocator = AdaptiveForceWithUMBAllocator::addAdaptiveForceWithUMBAllocator(blockSize, maxCacheSize, this);
    }
    else if(Config::scalableCacheAllocator == 1) {
        DPRINTF("[JS] StealingAllocator::addStealingAllocator\n");
        _allocator = StealingAllocator::addStealingAllocator(blockSize, maxCacheSize, this);
    }
    else if(Config::scalableCacheAllocator == 2) {
        DPRINTF("[JS] StealingAllocator::addRandomStealingAllocator false\n");
        _allocator = RandomStealingAllocator::addRandomStealingAllocator(blockSize, maxCacheSize, this, false);
    }
    else if(Config::scalableCacheAllocator == 3) {
        DPRINTF("[JS] StealingAllocator::addRandomStealingAllocator true\n");
        _allocator = RandomStealingAllocator::addRandomStealingAllocator(blockSize, maxCacheSize, this, true);
    }
    else if(Config::scalableCacheAllocator == 4) {
        DPRINTF("[JS] StealingAllocator::addLargestStealingAllocator\n");
        _allocator = LargestStealingAllocator::addLargestStealingAllocator(blockSize, maxCacheSize, this);
    }
    else if(Config::scalableCacheAllocator == 5) {
        DPRINTF("[JS] StealingAllocator::addFirstTouchAllocator\n");
        _allocator = FirstTouchAllocator::addFirstTouchAllocator(blockSize, maxCacheSize);
    }
    else if(Config::scalableCacheAllocator == 6){
        DPRINTF("[JS] AdaptiveAllocator::addAdaptiveAllocator\n");
        _allocator = AdaptiveAllocator::addAdaptiveAllocator(blockSize, maxCacheSize, this);
    }
    else if(Config::scalableCacheAllocator == 7){
        DPRINTF("[JS] AdaptiveForceWithOldestAllocator::addAdaptiveForceWithOldestAllocator\n");
        _allocator = AdaptiveForceWithOldestAllocator::addAdaptiveForceWithOldestAllocator(blockSize, maxCacheSize, this);
    }
    else {
        DPRINTF("[JS] SimpleAllocator::addSimpleAllocator\n");
        _allocator = SimpleAllocator::addSimpleAllocator(blockSize, maxCacheSize);
    }
    debug()<< "BURCU in scalablacache "<< cacheName << " " << _blockSize << " " << maxCacheSize << " " << _numBlocks << std::endl;
    MeMPRINTF( "In Scalablacache Blocksize: %d, Cachesize: %d, Numblocks:%d\n", _blockSize, maxCacheSize, _numBlocks );
    splice_misses = {0,0,0,0,0,0,0,0,0,0,0};
    splice_hits  = {0,0,0,0,0,0,0,0,0,0,0};
    srand(time(NULL));
    stats.end(false, CacheStats::Metric::constructor);
}

ScalableCache::~ScalableCache() {
    _terminating = true;
    log(this) << "deleting " << _name << " in ScalableCache" << std::endl;
    delete _lastVictimFileIndexLock;
    delete _cacheLock;
    evictHisto.printBins();
    stats.print(_name);
    if(Config::TraceHistogram){
        for ( auto met : _metaMap ){
            met.second->printHistLogs(met.first);
        }
    }
    std::cout<<"Splice Hits"<<splice_hits[0]<< " " << splice_hits[1] << " " << splice_hits[2] << " " << splice_hits[3] << " " << splice_hits[4] <<" " << splice_hits[5] <<" " << splice_hits[6]<< " " << splice_hits[7] <<" " << splice_hits[8]<< " " << splice_hits[9]<< " " << splice_hits[10] << std::endl;  
    std::cout<<"Splice Misses"<<splice_misses[0]<< " " << splice_misses[1] << " " << splice_misses[2] <<" " << splice_misses[3] <<" " << splice_misses[4] <<" " << splice_misses[5] <<" " << splice_misses[6] <<" " << splice_misses[7]<< " " << splice_misses[8] <<" " << splice_misses[9]<< " " << splice_misses[10] << std::endl;
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
        MeMPRINTF("ADDFILE:%u:%s\n", fileIndex, filename.c_str());
    }
    _allocator->openFile(_metaMap[fileIndex]);
    _cacheLock->writerUnlock();
    
    if (_nextLevel) {
        _nextLevel->addFile(fileIndex, filename, blockSize, fileSize);
    }
}

void ScalableCache::closeFile(uint32_t fileIndex) {
    MeMPRINTF("ScalableCache::closeFile:%u\n", fileIndex);
    _cacheLock->readerLock();
    if(_metaMap.count(fileIndex))
        _allocator->closeFile(_metaMap[fileIndex]);
    _cacheLock->readerUnlock();
}

uint8_t * ScalableCache::getBlockDataOrReserve(uint32_t fileIndex, uint64_t blockIndex, uint64_t fileOffset, bool &reserve, bool &full) {
    _cacheLock->readerLock();
    //JS: We only care about it being full if we are requesting a new block
    auto index = oustandingBlocksRequested.fetch_add(1);
    full = (index >= _numBlocks);
    //MeMPRINTF("updating stats for file %d \n", fileIndex);
    auto ret = _metaMap[fileIndex]->getBlockData(blockIndex, fileOffset, reserve, true);
    if(!reserve) {
        index = oustandingBlocksRequested.fetch_sub(1);
        //JS: This is a hit thus we don't care if it is full or not so we set it to false
        full = false;
    }
    // checkMaxInFlightRequests(index);
    _cacheLock->readerUnlock();
    return ret;
}

uint8_t * ScalableCache::getBlockData(uint32_t fileIndex, uint64_t blockIndex, uint64_t fileOffset) {
    bool reserve = false;
    _cacheLock->readerLock();
    auto ret = _metaMap[fileIndex]->getBlockData(blockIndex, fileOffset, reserve, false);
    _cacheLock->readerUnlock();
    return ret;
}

void ScalableCache::setBlock(uint32_t fileIndex, uint64_t blockIndex, uint8_t * data, uint64_t dataSize, bool writeOptional) {
    //JS: For trackBlock
    uint64_t sourceBlockIndex;
    uint32_t sourceFileIndex;
    
    _cacheLock->readerLock();

    auto meta = _metaMap[fileIndex];
    bool backout = false;
    uint8_t * dest = NULL;
    unsigned int count = 0;
    while(!dest && !_terminating && count < DEADLOCK_SAFEGUARD) {
        
        //JS: Ask for a new block from allocator
        MeMPRINTF("ASKING FOR NEW BLOCK:%d:%d:%lu\n", fileIndex, meta->getNumBlocks(),Timer::getCurrentTime());
        dest = _allocator->allocateBlock(fileIndex, (meta->getNumBlocks() == 0));

        //JS: Try to reuse my oldest block
        if(!dest) {
            MeMPRINTF("REUSE for file: %d\n", fileIndex);
            dest = meta->oldestBlock(sourceBlockIndex, true);
            if(dest) {
                DPRINTF("[JS] ScalableCache::setBlock Reusing block\n");
                trackBlockEviction(fileIndex, sourceBlockIndex);
            }
        }

        //JS: Try to backout
        if(!dest && meta->backOutOfReservation(blockIndex)) {
            DPRINTF("[JS] ScalableCache::setBlock Backing out\n");
            stats.addAmt(false, CacheStats::Metric::backout, 1);
            backout = true;
            break;
        }

        //JS: Last resort
        if(!dest) {
            //JS: This is an orphan write
            if(writeOptional) {
                break;
            }
            //JS: This is a property of the allocators!
            else if(_allocator->canReturnEmpty()) {
                //JS: Someone should have a free block. We will search all files looking for it!
                dest = findBlockFromOldestFile(fileIndex, sourceFileIndex, sourceBlockIndex);
                if(dest) {
                    trackBlockEviction(sourceFileIndex, sourceBlockIndex);
                    break;
                }
            }
        }
        count++;
    }

    if(dest) {
        //JS: Copy the block back to our cache
        memcpy(dest, data, dataSize);
        meta->setBlock(blockIndex, dest);
        if(!writeOptional)
            oustandingBlocksRequested.fetch_sub(1);
        trackBlock(_name, "[BLOCK_WRITE]", fileIndex, blockIndex, 0);
    }
    else if(writeOptional){
        //JS: We still locked the block even though there was no data there
        //which is why we are dec in the overlyfull situation
        meta->decBlockUsage(blockIndex);
    }
    else if(backout) {
        if(!writeOptional) {
            oustandingBlocksRequested.fetch_sub(1);
        }
    }
    else {
        if (!_terminating) {
            DPRINTF("[Tazer] Allocation failed, potential race condition! Total: %lu Outstanding: %lu\n", _numBlocks, oustandingBlocksRequested.load());
            raise(SIGSEGV);
        }
        else {
            DPRINTF("[Tazer] Allocation failed due to termination! Total: %lu Outstanding: %lu\n", _numBlocks, oustandingBlocksRequested.load());
            raise(SIGSEGV);
        }
    }
    _cacheLock->readerUnlock();
}

bool ScalableCache::writeBlock(Request *req){
    DPRINTF("[JS] ScalableCache::writeBlock start\n");
    //PPRINTF("DELIVERY TIME FOR BM: %p %lu\n", req, req->deliveryTime);
    req->time = Timer::getCurrentTime();
    bool ret = false;
    //BM: added delivery time to metamap
    _metaMap[req->fileIndex]->updateDeliveryTime(req->deliveryTime);
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
        setBlock(req->fileIndex, req->blkIndex, req->data, req->size, req->skipWrite);
        if (_nextLevel) {
            ret &= _nextLevel->writeBlock(req);
        }
    }
    DPRINTF("[JS] ScalableCache::writeBlock done\n");
    return ret;
}

void ScalableCache::readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority){
    std::thread::id thread_id = req->threadId;
    bool prefetch = priority != 0;
    stats.start(prefetch, CacheStats::Metric::read, thread_id); //read
    stats.start(prefetch, CacheStats::Metric::ovh, thread_id); //ovh
    uint8_t *buff = nullptr;
    auto index = req->blkIndex;
    auto fileIndex = req->fileIndex;
    auto fileOffset = req->offset;
    
    auto timeStamp = Timer::getCurrentTime();
    for ( auto met : _metaMap ){
        MeMPRINTF("PARTITION INFO:%d:%d:%lu\n", met.first, met.second->getNumBlocks(), timeStamp);
        MeMPRINTF("UNITBENEFIT:%d:%.20lf:%lu\n", met.first, met.second->getUnitBenefit(), timeStamp);
        MeMPRINTF("UNITMARGINALBENEFIT:%d:%.20lf:%lu\n", met.first, met.second->getUnitMarginalBenefit(), timeStamp);
        MeMPRINTF("UPPERLEVELMETRIC:%d:%.20lf:%lu\n", met.first, met.second->getUpperMetric(), timeStamp);
        MeMPRINTF("OLDESTPREDICTION:%d:%.20lf:%lu\n", met.first, met.second->getOldestPrediction(), timeStamp);
    }
    MeMPRINTF("TOTAL Blocks:%d:%d:%lu\n", 0, _numBlocks, timeStamp);
    MeMPRINTF("CURRENT ACCESS:0:%d:%lu\n",access.load(), timeStamp);

    if (!req->size) { //JS: This get the blockSize from the file
        _cacheLock->readerLock();
        req->size = _fileMap[fileIndex].blockSize;
        _cacheLock->readerUnlock();
    }
    
    DPRINTF("[JS] ScalableCache::readBlock req->size: %lu req->fileIndex: %lu req->blkIndex: %lu\n", req->size, req->fileIndex, req->blkIndex);
    trackBlock(_name, (priority != 0 ? " [BLOCK_PREFETCH_REQUEST] " : " [BLOCK_REQUEST] "), fileIndex, index, priority);

    if (req->size <= _blockSize) {
        bool reserve;
        bool full;
        //JS: Fill in the data if we have it
        //with respect to stat keeping this is inconsistent with the other caches because here we are attributing both 
        //the overhead of finding a block + the time it takes to read that block from its backing medium to the "ovh" stat
        //in the other the other caches, the cost of reading the data is attributed to the "hit" stat
        buff = getBlockDataOrReserve(fileIndex, index, fileOffset, reserve, full);
        
        access.fetch_add(1);
        //Right after we record an access, we check if we need to adapt the memory size
        //adaptMemorySize();
        /////
        ////find the right splice 
        int spl = access.load()/500;
        if(spl > 10)
            spl = 10;
        /////
        
        // std::cerr << " [JS] readBlock " << buff << " " << reserve << std::endl;
        if(buff) {

            stats.end(prefetch, CacheStats::Metric::ovh, thread_id); //read
            stats.start(prefetch, CacheStats::Metric::hits, thread_id); //ovh
            DPRINTF("[JS] ScalableCache::readBlock hit\n");
            //JS: Update the req, block is currently held by count
            req->data=buff;
            req->originating=this;
            req->reservedMap[this]=1;
            req->ready = true;
            req->time = Timer::getCurrentTime() - req->time;
            updateRequestTime(req->time);

            trackBlock(_name, "[BLOCK_READ_HIT]", fileIndex, index, priority);
            BPRINTF("FILE:%d hit\n", fileIndex);
            _metaMap[fileIndex]->trackZValue(index);
            _metaMap[fileIndex]->updateStats(false, timeStamp);
            int temp = resizeNumbers.load();
            splice_hits[spl]++;
            BPRINTF("FILE:%d returned from updates\n", fileIndex);
            stats.addAmt(prefetch, CacheStats::Metric::hits, req->size, thread_id);
            stats.end(prefetch, CacheStats::Metric::hits, thread_id);
            stats.start(prefetch, CacheStats::Metric::ovh, thread_id);
        }
        else { //JS: Data not currently present
            trackBlock(_name, "[BLOCK_READ_MISS_CLIENT]", fileIndex, index, priority);
            //setting startTimeStamp to fist miss we encounter
            std::call_once(first_miss, [this]() { startTimeStamp = (uint64_t)Timer::getCurrentTime(); });
            misses.fetch_add(1);
            stats.addAmt(prefetch, CacheStats::Metric::misses, 1, thread_id);
            

            if (reserve || full) { //JS: We reserved block
                //calcrank on this partition after the miss
             //JS: For calcRank
                DPRINTF("[JS] ScalableCache::readBlock reseved\n");

                auto localMisses = misses.load();
                uint64_t timeStamp = Timer::getCurrentTime(); 

                BPRINTF("FILE:%d miss\n", fileIndex);
                //_metaMap[fileIndex]->trackZValue(index);
                _metaMap[fileIndex]->updateStats(true, timeStamp);
                splice_misses[spl]++;
                BPRINTF("FILE:%d returned from updates\n", fileIndex);
                _metaMap[fileIndex]->calcRank(timeStamp-startTimeStamp, localMisses);
                
 
                req->skipWrite = full;
                stats.end(prefetch, CacheStats::Metric::ovh, thread_id);
                stats.start(prefetch, CacheStats::Metric::misses, thread_id); //miss
                _nextLevel->readBlock(req, reads, priority);
                stats.end(prefetch, CacheStats::Metric::misses, thread_id);
                stats.start(prefetch, CacheStats::Metric::ovh, thread_id); 
                
            } ///continue from here
            else { //JS: Someone else will fullfill request
                stats.end(prefetch, CacheStats::Metric::ovh, thread_id);
                stats.start(prefetch, CacheStats::Metric::misses, thread_id);
                DPRINTF("[JS] ScalableCache::readBlock waiting\n");
                auto fut = std::async(std::launch::deferred, [this, req] {
                    int spl = access.load()/500;
                    if(spl > 10)
                        spl = 10;
                    uint64_t offset = req->offset;
                    auto blockIndex = req->blkIndex;
                    auto fileIndex = req->fileIndex;

                    //JS: Wait for data to be ready
                    uint8_t * buff = getBlockData(fileIndex, blockIndex, offset);
                    while (!buff) {
                        sched_yield();
                        buff = getBlockData(fileIndex, blockIndex, offset);
                    }

                    //calcrank on this partition after the miss
                    //JS: For calcRank
                    //auto localMisses = misses.load();
                    uint64_t timeStamp = Timer::getCurrentTime();

                    BPRINTF("FILE:%d future\n", fileIndex);
                    _metaMap[fileIndex]->trackZValue(blockIndex);
                    _metaMap[fileIndex]->updateStats(false, timeStamp);
                    splice_hits[spl]++;
                    BPRINTF("FILE:%d returned from updates\n", fileIndex);
                   

                    //JS: Update the req
                    req->data=buff;
                    req->originating=this;
                    req->reservedMap[this]=1;
                    req->ready = true;

                    //JS TODO: Check if this is right and add to unbounded cache--
                    //RF it likely isnt because it can be waiting on any level of cache, in bounded caches,
                    // the cache entrys contain the originating cache which read to set the waitingCache
                    // this doesn't looked to be stored in the ScalableMetaData::BlockEntry data structure (we can reference BoundedCache::BlockEntry)
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
                stats.end(prefetch, CacheStats::Metric::misses, thread_id);
                stats.start(prefetch, CacheStats::Metric::ovh, thread_id);  
            }
        }
    }
    else {
        std::cerr << "[TAZER] We do not currently support blockSizes bigger than cache size!" << std::endl;
        raise(SIGSEGV);
    }
    DPRINTF("[JS] ScalableCache::readBlock done\n");
    stats.end(prefetch, CacheStats::Metric::ovh, thread_id);
    stats.end(prefetch, CacheStats::Metric::read, thread_id);
}
void ScalableCache::adaptMemorySize(){
    auto curAccesses = access.load();
    int var_A = 500;
    int growLimit = 100 / resizePercentage; //number of times we can grow the memory (assumption: limit is 2X the private memory)

    //std::cout<<"current access:"<<curAccesses<<" numblocks:"<<_numBlocks<<"growlimit:"<<growLimit<<" addblocks:"<<resizeAmount<<std::endl;
    if( ! (curAccesses % var_A)){
        //after A accesses decide on what to do 
        if(resizeNumbers.load() < growLimit){
            BPRINTF("CurAccess: %d , growing with resizeAmount: %d, before numblocks:%d \n", curAccesses, resizeAmount, _numBlocks);
            resizeNumbers.fetch_add(1);
            //grow resizePercentage%
            _numBlocks += resizeAmount;
            _allocator->addAvailableBlocks(resizeAmount);
            BPRINTF("after numblocks:%d\n", _numBlocks);
            
        }
        else if(resizeNumbers.load() < (growLimit * 2)){
            BPRINTF("CurAccess: %d , shrinking with resizeAmount: %d, before numblocks:%d (growLimit: %d)\n", curAccesses, resizeAmount, _numBlocks, growLimit);
            resizeNumbers.fetch_add(1);
            //shrink resizePercentage%
            _numBlocks -= resizeAmount;
            _allocator->removeBlocks(resizeAmount);
            BPRINTF("after numblocks:%d\n", _numBlocks);
        }
        else{
            //we grew and shrunk back, do nothing
        }
        // std::cout<<"After A accesses:NumBlocks:"<<_numBlocks<<std::endl;
    }
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

//BM: calculates UMBs for all files and updates UMBList
void ScalableCache::updateRanks(uint32_t allocateForFileIndex, double & allocateForFileRank) {
    _cacheLock->readerLock();
    
    //JS: Making a local copy of UMBList
    std::vector<std::tuple<uint32_t, double>> UMBList;
    std::vector<std::tuple<uint32_t, double>> localUMBs;
    //JS: For calcRank
    auto localMisses = misses.load();
    uint64_t timestamp = Timer::getCurrentTime();
    
    for (auto const &x : _metaMap) {
        auto fileIndex = x.first;
        auto meta = x.second;
        meta->calcRank(timestamp-startTimeStamp, localMisses);
        UMBList.push_back(std::tuple<uint32_t, double>(fileIndex,meta->getUpperMetric()));
        localUMBs.push_back(std::tuple<uint32_t, double>(fileIndex,meta->getUnitMarginalBenefit()));
        if(fileIndex == allocateForFileIndex){
            allocateForFileRank = meta->getUnitMarginalBenefit();
        }
    }
    sort(UMBList.begin(), UMBList.end(), 
        [](std::tuple<uint32_t, double> &lhs, std::tuple<uint32_t, double> &rhs) -> bool {
                return (std::get<1>(lhs) < std::get<1>(rhs));
        });

    sort(localUMBs.begin(), localUMBs.end(), 
        [](std::tuple<uint32_t, double> &lhs, std::tuple<uint32_t, double> &rhs) -> bool {
                return (std::get<1>(lhs) < std::get<1>(rhs));
        });

    //JS: Updated _UMBList
    _lastVictimFileIndexLock->writerLock();
    _UMBList = UMBList;
    _localUMBs = localUMBs;
    //JS: Update dirty flags to true
    for (auto &x : _UMBDirty) {
        x.second = true;
    }
    _lastVictimFileIndexLock->writerUnlock();
    _cacheLock->readerUnlock();
}


ScalableMetaData * ScalableCache::findVictim(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, bool mustSucceed) {
    _cacheLock->readerLock();
    MeMPRINTF("---------- %u of %u ----------\n", allocateForFileIndex, _metaMap.size());
    
    //JS: Making a local copy of UMBList
    std::vector<std::tuple<uint32_t, double>> UMBList;

    //JS: For calcRank
    auto localMisses = misses.load();
    uint64_t timestamp = Timer::getCurrentTime();

    //JS: Find unit marginal benefits
    double sourceFileRank;
    double minRank = std::numeric_limits<double>::max();
    uint32_t minFileIndex = (uint32_t) -1;

    //JS: Find lowest rank
    DPRINTF("+++++++++++Starting Search %u\n", _metaMap.size());
    for (auto const &x : _metaMap) {
        auto fileIndex = x.first;
        auto meta = x.second;
        if(allocateForFileIndex != fileIndex and meta->getNumBlocks()>1) {
            auto temp = meta->calcRank(timestamp-startTimeStamp, localMisses);
            //JS: This is for recording the unit marginal benefit and making it available to other caches
            UMBList.push_back(std::tuple<uint32_t, double>(fileIndex,temp));
            MeMPRINTF("-------Index: %u %lf\n", fileIndex, temp);
            if(!std::isnan(temp) && temp > 0 && temp < minRank) {
                minRank = temp;
                minFileIndex = fileIndex;
                DPRINTF("*******NEW MIN: %u : %lf\n", minFileIndex, minRank);
            }
        }
        else if (allocateForFileIndex == fileIndex){
            sourceFileRank = meta->calcRank(timestamp-startTimeStamp, localMisses);
            MeMPRINTF("-------Source Index: %u %lf\n", fileIndex, sourceFileRank);
        }
        else{
            sourceFileRank = std::numeric_limits<double>::max();
        }
    }

    DPRINTF("+++++++++++End Search\n");
    ScalableMetaData * ret = NULL;
    sourceFileIndex = (uint32_t) -1;
    if(minFileIndex != (uint32_t) -1) {
        if(mustSucceed || std::isnan(sourceFileRank) || sourceFileRank > minRank) {
            ret = _metaMap[minFileIndex];
            //JS: Do update rank here
            MeMPRINTF("-----------SOURCE: %u DEST: %u\n", minFileIndex, allocateForFileIndex);
            _metaMap[minFileIndex]->updateRank(true);
            _metaMap[allocateForFileIndex]->updateRank(false);
            //JS: Set this for the tracking
            sourceFileIndex = minFileIndex;
        }
    }

    sort(UMBList.begin(), UMBList.end(), 
        [](std::tuple<uint32_t, double> &lhs, std::tuple<uint32_t, double> &rhs) -> bool {
                return (std::get<1>(lhs) < std::get<1>(rhs));
        });

    //JS: Updated _UMBList
    _lastVictimFileIndexLock->writerLock();
    _UMBList = UMBList;
    //JS: Update dirty flags to true
    for (auto &x : _UMBDirty) {
        x.second = true;
    }
    _lastVictimFileIndexLock->writerUnlock();
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

//JS: This will basically search through every block starting from the oldest file. 
//This will succeed as long as there exists a block that is free to steal!
uint8_t * ScalableCache::findBlockFromOldestFile(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, uint64_t &sourceBlockIndex) {
    std::vector<std::tuple<uint32_t, uint64_t>> fileList;
    
    //JS: Build list of file index and access times
    _cacheLock->readerLock();
    for (auto const &x : _metaMap) {
        auto fileIndex = x.first;
        auto meta = x.second;
        uint64_t temp = meta->getLastTimeStamp();
        fileList.push_back(std::tuple<uint32_t, uint64_t>(fileIndex, temp));
    }

    //JS: Sort list
    sort(fileList.begin(), fileList.end(), 
        [](std::tuple<uint32_t, uint64_t> &lhs, std::tuple<uint32_t, uint64_t> &rhs) -> bool {
                return (std::get<1>(lhs) < std::get<1>(rhs));
        });

    //JS: Look until we find a block
    uint8_t * block = NULL;
    for(const auto &entry : fileList) {
        uint32_t index = std::get<0>(entry);
        DPRINTF("Checking file index: %u numBlocks: %lu\n", index, _metaMap[index]->getNumBlocks());
        block = _metaMap[index]->oldestBlock(sourceBlockIndex);
        if(block) {
            if(index != allocateForFileIndex) {
                _metaMap[index]->updateRank(true);
                _metaMap[allocateForFileIndex]->updateRank(false);
                sourceFileIndex = index;
            }
            break;
        }     
    }
    _cacheLock->readerUnlock();
    DPRINTF("[JS] findBlockFromOldestFile %p\n", block);
    return block;
}

/*JS: This will basically search through every block starting from the lowest UMB.
  Here we use the cached UMB since this will be called by the adaptive allocator
  who just ran the findVictim.  This will succeed as long as there exists a block
  that is free to steal!*/
uint8_t * ScalableCache::findBlockFromCachedUMB(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, uint64_t &sourceBlockIndex, double allocateForFileRank) {
    //MeMPRINTF("STEALING FOR file:%d , UMB:%.5lf, blocks:%d\n", allocateForFileIndex, allocateForFileRank,_metaMap[allocateForFileIndex]->getNumBlocks());
    uint8_t * block = NULL;
    _cacheLock->readerLock();
    _lastVictimFileIndexLock->readerLock();
    for(const auto &UMB : _localUMBs) {
        uint32_t index = std::get<0>(UMB);
        double value = std::get<1>(UMB);
        
        if(index != allocateForFileIndex) {
            
            double stealThr = Config::PrivateThreshold; 
            if(allocateForFileRank > value && ( std::abs(allocateForFileRank-value) >= std::abs(value*stealThr))) {
                if(_metaMap[index]->getOldestPrediction() < _metaMap[allocateForFileIndex]->getOldestPrediction() ){  
                    
                    block = _metaMap[index]->oldestBlock(sourceBlockIndex);
                    if(block) {
                         auto localMisses = misses.load();
                        uint64_t timestamp = Timer::getCurrentTime();
                        _metaMap[index]->calcRank(timestamp-startTimeStamp, localMisses);
                        sourceFileIndex = index;
                        break;
                    }
                }
            }
            else {
                //UMBs in the rest of the list is higher than allocateForFileRank, so we shouldn't steal from them
                break;
            }
        } 
    }
    _lastVictimFileIndexLock->readerUnlock();
    _cacheLock->readerUnlock();
    DPRINTF("[JS] findBlockFromCachedUMB %p outstanding: %lu\n", block, oustandingBlocksRequested.load());
    return block;
}

uint8_t * ScalableCache::findBlockFromCachedUMBandOldestPrediction(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, uint64_t &sourceBlockIndex, double allocateForFileRank) {
    MeMPRINTF("STEALING FOR file:%d , UMB:%.5lf, blocks:%d\n", allocateForFileIndex, allocateForFileRank,_metaMap[allocateForFileIndex]->getNumBlocks());
    StealPRINTF("\n\nSTEALING FOR file:%d , UMB:%.5lf, blocks:%d, curaccess:%d\n", allocateForFileIndex, allocateForFileRank,_metaMap[allocateForFileIndex]->getNumBlocks(), access.load());
    uint8_t * block = NULL;
    _cacheLock->readerLock();
    _lastVictimFileIndexLock->readerLock();

    double allocateForFileTime = _metaMap[allocateForFileIndex]->getOldestPrediction();
    StealPRINTF("asking file oldest:%.5lf\n", allocateForFileTime);
    auto minUMB =  std::get<1>(_localUMBs[0]);

    uint32_t victim_u, victim_w; 
    double victim_u_time, victim_w_time;

    victim_u=0;
    victim_u_time = std::numeric_limits<double>::max();

    victim_w = 0;
    victim_w_time = std::numeric_limits<double>::max();

    for(const auto &UMB : _localUMBs) {
        uint32_t cur_index = std::get<0>(UMB);
        double cur_umb = std::get<1>(UMB);
        if(cur_index == allocateForFileIndex)
            continue;
        StealPRINTF("CurFile: %d, CurUMB:%.10lf, CurOldest: %.10lf\n", cur_index, cur_umb, _metaMap[cur_index]->getOldestPrediction());
        //if UMB is within a percentage of the minimum UMB, we consider them equivalent 
        if(std::abs(cur_umb-minUMB) <= std::abs(minUMB*Config::PrivateThreshold) && _metaMap[cur_index]->getNumBlocks()>0){
            //here we find the partition that has minimum equivalent UMB and the oldest prediction 
            if( _metaMap[cur_index]->getOldestPrediction() < victim_u_time){
                victim_u = cur_index;
                victim_u_time = _metaMap[cur_index]->getOldestPrediction();
            }
        }
        //the partition with the oldest prediction
        StealPRINTF("Current index:%d, current blocks:%d\n", cur_index,_metaMap[cur_index]->getNumBlocks() );
        if( _metaMap[cur_index]->getOldestPrediction() < victim_w_time && _metaMap[cur_index]->getNumBlocks()>0 ){
            StealPRINTF("-New victimw! \n");
            victim_w = cur_index;
            victim_w_time = _metaMap[cur_index]->getOldestPrediction();
        }
    }
    StealPRINTF("After loop: victim_u: %d, victim_u_time: %.15lf\n",victim_u,victim_u_time  );
    StealPRINTF("After loop: victim_w: %d, victim_w_time: %.15lf\n",victim_w,victim_w_time  );

    uint64_t timestamp = Timer::getCurrentTime();
    auto localMisses = misses.load();
    double average_miss = ((double)timestamp-startTimeStamp) / localMisses;
    //if(allocateForFileRank > std::get<1>(_localUMBs[victim_u]) && ( std::abs(allocateForFileRank-std::get<1>(_localUMBs[victim_u])) >= std::abs(std::get<1>(_localUMBs[victim_u])*Config::StealThreshold))) {
    auto sth = Config::PrivateThreshold;
    auto vic_umb = std::get<1>(_localUMBs[victim_u]);
    //if we found a umb victim && asking file's umb is higher than victim && asking file's umb is significantly higher && victim has bloks)
    if(victim_u>0 && allocateForFileRank > vic_umb && ( std::abs(allocateForFileRank-vic_umb) >= std::abs(vic_umb*sth)) && _metaMap[victim_u]->getNumBlocks()>0) {
        StealPRINTF("In the first if; stealing from victim_u: %d\n", victim_u);
        if(_metaMap[allocateForFileIndex]->getNumBlocks() == 0 || victim_u_time < allocateForFileTime){
            block = _metaMap[victim_u]->oldestBlock(sourceBlockIndex);
            if (block){
                _metaMap[victim_u]->calcRank(timestamp-startTimeStamp, localMisses);
                sourceFileIndex = victim_u;
            }
        }
    }
    else if(victim_w > 0 &&  victim_w_time < timestamp - (Config::k_parameter*average_miss) ){ //if oldest predicted block is older than 5*average-miss we consider it stale 
        StealPRINTF("In the else if; stealing from victim_w: %d\n", victim_w);
        block = _metaMap[victim_w]->oldestBlock(sourceBlockIndex);
        if(block){
            _metaMap[victim_w]->calcRank(timestamp-startTimeStamp, localMisses);
            sourceFileIndex = victim_w;
        }
    }


    _lastVictimFileIndexLock->readerUnlock();
    _cacheLock->readerUnlock();
    DPRINTF("[JS] findBlockFromCachedUMBand oldest %p outstanding: %lu\n", block, oustandingBlocksRequested.load());
    return block;
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

std::vector<std::tuple<uint32_t, double>> ScalableCache::getLastUMB(Cache * cache) {
    std::vector<std::tuple<uint32_t, double>> ret;
    _lastVictimFileIndexLock->readerLock();
    // if(_UMBDirty.count(cache)) {
        if(_UMBDirty[cache]) {
            ret = _UMBList;
            _UMBDirty[cache] = false;
        }
    // }
    _lastVictimFileIndexLock->readerUnlock();
    return ret;
}

void ScalableCache::setUMBCache(Cache * cache) {
    _lastVictimFileIndexLock->writerLock();
    _UMBDirty[cache] = true;
    _lastVictimFileIndexLock->writerUnlock();
}

//JS: This is helper function used in debugging.
//Super helpful looking for lost blocks!!!
void ScalableCache::checkMaxBlockInUse(std::string msg, bool die) {
    _cacheLock->readerLock();

    uint64_t sum = 0;
    for (auto const &x : _metaMap) {
        auto meta = x.second;
        sum+=meta->getNumBlocks();
    }

    while(1) {
        auto dirty = maxBlocksInUse.load();
        if(sum < dirty) {
            DPRINTF("LOST BLOCKS: %s %lu vs %lu\n", msg.c_str(), dirty, sum);
            if(die)
                raise(SIGSEGV);
            break;
        }
        else if(dirty < sum) {
            if(maxBlocksInUse.compare_exchange_weak(dirty, sum)) {
                DPRINTF("NEW MAXBLOCKSINUSE: %lu\n", sum);
                break;
            }
        }
        else
            break;
    }

    _cacheLock->readerUnlock();
}

//JS: This is useful for printing when we have a new max inflight requests!
void ScalableCache::checkMaxInFlightRequests(uint64_t index) {
    _cacheLock->readerLock();
    while(1) {
        auto dirty = maxOutstandingBlocks.load();
        if(dirty < index) {
            if(maxOutstandingBlocks.compare_exchange_weak(dirty, index+1)) {
                DPRINTF("NEW OUTSTANDING MAX: %lu\n", index + 1);
                break;
            }
        }
        else
            break;
    }
    _cacheLock->readerUnlock();
}

