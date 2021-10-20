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

#include "NewBoundedCache.h"
#include "Config.h"
#include "FcntlReaderWriterLock.h"
#include "FileLinkReaderWriterLock.h"
#include "ThreadPool.h"
#include "Timer.h"
#include "xxhash.h"
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <future>
#include <signal.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

template <class Lock>
NewBoundedCache<Lock>::NewBoundedCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity) : Cache(cacheName,type),
                                                                                                                          _cacheSize(cacheSize),
                                                                                                                          _blockSize(blockSize),
                                                                                                                          _associativity(associativity),
                                                                                                                          _numBlocks(_cacheSize / _blockSize),
                                                                                                                          _collisions(0),
                                                                                                                          _prefetchCollisions(0),
                                                                                                                          _outstanding(0) {

    // log(this) /*debug()*/<< "Constructing " << _name << " in NewBoundedCache" << std::endl;
    stats.start();
    log(this) << _name << " " << _cacheSize << " " << _blockSize << " " << _numBlocks << std::endl;
    if (_associativity == 0 || _associativity > _numBlocks) { //make fully associative
        _associativity = _numBlocks;
    }
    _numBins = _numBlocks / _associativity;
    if (_numBlocks % _associativity) {
        log(this) /*std::cerr*/ << "[TAZER]"
                                << "NumBlocks not a multiple of associativity" << std::endl;
    }
    log(this) << _name << " " << _cacheSize << " " << _blockSize << " " << _numBlocks << " " << _associativity << " " << _numBins << std::endl;
    _localLock = new ReaderWriterLock();
    stats.end(false, CacheStats::Metric::constructor);
}

template <class Lock>
NewBoundedCache<Lock>::~NewBoundedCache() {
    _terminating = true;
    log(this) << _name << " cache collisions: " << _collisions.load() << " prefetch collisions: " << _prefetchCollisions.load() << std::endl;
    // for (uint32_t i = 0; i < _numBlocks; i++) {
    //     log(this) /*debug()*/<< i << std::endl;
    //     if (_blkIndex[i].activeCnt > 0) {
    //         log(this) /*debug()*/<< i << " " << _numBlocks << " " << _blkIndex[i].activeCnt.load() << " " << _blkIndex[i].status << std::endl;
    //     }
    // }

    log(this) << "deleting " << _name << " in NewBoundedCache" << std::endl;
    delete _localLock;
}

// template <class Lock>
// void NewBoundedCache<Lock>::getCompareBlkEntry(uint32_t index, uint32_t fileIndex, BlockEntry *entry) {
//     entry->blockIndex = index;
//     entry->fileIndex = fileIndex;
// }

template <class Lock>
std::shared_ptr<typename NewBoundedCache<Lock>::BlockEntry> NewBoundedCache<Lock>::getCompareBlkEntry(uint32_t index, uint32_t fileIndex) {
    std::shared_ptr<BlockEntry> entry = std::make_shared<BlockEntry>();
    entry->blockIndex= index;
    entry->fileIndex = fileIndex;
    return entry;
}

template <class Lock>
bool NewBoundedCache<Lock>::sameBlk(BlockEntry *blk1, BlockEntry *blk2) {
    return blk1->fileIndex == blk2->fileIndex && blk1->blockIndex == blk2->blockIndex;
}

template <class Lock>
uint32_t NewBoundedCache<Lock>::getBinIndex(uint32_t index, uint32_t fileIndex) {
    _localLock->readerLock();
    uint64_t temp = _fileMap[fileIndex].hash + index;
    _localLock->readerUnlock();
    return (temp % (_numBins));
}

template <class Lock>
uint32_t NewBoundedCache<Lock>::getBinOffset(uint32_t index, uint32_t fileIndex) {
    _localLock->readerLock();
    uint64_t temp = _fileMap[fileIndex].hash + index;
    _localLock->readerUnlock();
    return (temp % (_numBins)) * _associativity;
}

template <class Lock>
typename NewBoundedCache<Lock>::BlockEntry* NewBoundedCache<Lock>::getBlock(uint32_t index, uint32_t fileIndex, Request* req){
    req->trace()<<"searching for block: "<<index<<" "<<fileIndex<<std::endl;
    uint32_t binIndex = getBinIndex(index, fileIndex);
    auto blkEntries = readBin(binIndex);
    auto cmpBlk = getCompareBlkEntry(index, fileIndex);

    for (uint32_t i = 0; i < _associativity; i++) {
        if (sameBlk(blkEntries[i], cmpBlk.get())) {
            if (blkEntries[i]->status == BLK_AVAIL) {
                req->trace()<<"found block: "<<blockEntryStr(blkEntries[i])<<std::endl;
                return blkEntries[i];
            }
        }
    }
    req->trace()<<"block not found "<<std::endl;
    return NULL;
}

template <class Lock>
typename NewBoundedCache<Lock>::BlockEntry* NewBoundedCache<Lock>::oldestBlock(uint32_t index, uint32_t fileIndex, Request* req) {
    req->trace()<<"searching for oldest block: "<<index<<" "<<fileIndex<<std::endl;
    BlockEntry* blkEntry = NULL;
    uint32_t minTime = -1; //this is max uint32_t
    BlockEntry *minEntry = NULL;
    uint32_t minPrefetchTime = -1; //Prefetched block
    BlockEntry *minPrefetchEntry = NULL;
    uint32_t binIndex = getBinIndex(index, fileIndex);
    auto cmpBlk = getCompareBlkEntry(index, fileIndex);
    BlockEntry *entry = cmpBlk.get();
    auto blkEntries = readBin(binIndex);

    for (uint32_t i = 0; i < _associativity; i++) { // maybe we want to split this into two loops--first to check if any empty or if its here, then a lru pass, other wise we require checking the number of active users on every block which can be expensive for file backed caches
        //Find actual, empty, or oldest
        blkEntry = blkEntries[i];
        if (sameBlk(blkEntry, entry)) { //block is present
            req->trace()<<"found entry: "<<blockEntryStr(blkEntry)<<std::endl;
            return blkEntry;
        }
        if (blkEntry->status == BLK_EMPTY) { //The space is empty!!!
            req->trace()<<"found  empty entry: "<<blockEntryStr(blkEntry)<<std::endl;
            return blkEntry;
        }
        else if (blkEntry->status == BLK_AVAIL) {// we found an available block, deterimine if we evict it
            //LRU
            if (blkEntry->timeStamp < minTime) { //Look for an old one
                if (!anyUsers(blkEntry,req)) { //can optimize a read since we already have the blkEntry, pass the pointer to the entry?
                    minTime = blkEntry->timeStamp;
                    minEntry = blkEntry;
                }
            }
            //PrefetchEvict policy evicts prefetched blocks first
            if (Config::prefetchEvict && blkEntry->prefetched && blkEntry->timeStamp < minPrefetchTime) {
                if (!anyUsers(blkEntry,req)) {
                    minPrefetchTime = blkEntry->timeStamp;
                    minPrefetchEntry = blkEntry;
                }
            }
        }
    }

    //If a prefetched block is found, we evict it
    if (Config::prefetchEvict && minPrefetchTime != (uint32_t)-1 && minPrefetchEntry) {
        _prefetchCollisions++;
        trackBlock(_name, "[BLOCK_EVICTED]", fileIndex, index, 1);
        minPrefetchEntry->status = BLK_EVICT;
        req->trace()<<"evicting prefected entry: "<<blockEntryStr(minPrefetchEntry)<<std::endl;
        stats.addAmt(1, CacheStats::Metric::evictions, 1);
        return minPrefetchEntry;
    }
    if (minTime != (uint32_t)-1 && minEntry) { //Did we find a space
        _collisions++;
        trackBlock(_name, "[BLOCK_EVICTED]", fileIndex, index, 0);
        minEntry->status = BLK_EVICT;
        req->trace()<<"evicting  entry: "<<blockEntryStr(minEntry)<<std::endl;
        stats.addAmt(0, CacheStats::Metric::evictions, 1);
        return minEntry;
    }
    log(this)<< _name << " All space is reserved..." << std::endl;
    req->trace()<<"no entries found"<<std::endl;
    return NULL;
}

template <class Lock>
bool NewBoundedCache<Lock>::writeBlock(Request *req) {
    // if (_type == CacheType::boundedGlobalFile){
    //     req->printTrace=true;
    //     req->globalTrigger=true;
    // }
    // else{
    //     req->printTrace=false; 
    //     req->globalTrigger=false;
    // }
    req->trace()<<_name<<" WRITE BLOCK"<<std::endl;
    
    bool ret = false;
    if (req->reservedMap[this] > 0 || !_terminating) { //when terminating dont waste time trying to write orphan requests
        auto index = req->blkIndex;
        auto fileIndex = req->fileIndex;
        auto binIndex = getBinIndex(index, fileIndex);
        if (req->originating == this) {
            req->trace()<<"originating cache"<<std::endl;
            _binLock->writerLock(binIndex,req);
            BlockEntry* entry = getBlock(index, fileIndex, req);
            if (entry) {
                if (req->reservedMap[this] > 0) {
                    req->trace()<<"decrement "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    auto t_cnt = decBlkCnt(entry, req);
                    req->trace()<<" dec cnt "<<t_cnt<<std::endl;
                    if (t_cnt == 0) {
                        debug()<<req->str()<<std::endl;
                        log(this) << _name << " underflow in orig activecnt (" << t_cnt - 1 << ") for blkIndex: " <<  entry->id << " fileIndex: " << fileIndex << " index: " << index << std::endl;
                        exit(0);
                    }
                }
            }
            // the "else" would trigger in the case this cache was completely filled and active when the request was initiated so we didnt have a reservation for this block
            else {
                req->trace() << "writeblock should this even be possible?" << std::endl;
                entry = getBlockEntry(req->indexMap[this], req); 
                req->trace() <<" entry: "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                debug()<<req->str()<<std::endl;
                exit(0);
            }
            _binLock->writerUnlock(binIndex,req);
            cleanUpBlockData(req->data);
            req->trace()<<"deleting req"<<std::endl;  
            req->printTrace=false;           
            delete req;
            
            ret = true;
        }
        else {
            req->trace()<<"not originating cache ---"<<std::endl;

            DPRINTF("beg wb blk: %u out: %u\n", index, _outstanding.load());

            if (req->size <= _blockSize) {
                _binLock->writerLock(binIndex,req);
                
                BlockEntry* entry = oldestBlock(index, fileIndex, req);

                trackBlock(_name, "[BLOCK_WRITE]", fileIndex, index, 0);

                if (entry){ //a slot for the block is present in the cache       
                    if ((entry->status != BLK_WR && entry->status != BLK_AVAIL) || entry->status == BLK_EVICT) { //not found means we evicted some block
                        if(entry->status == BLK_EVICT && req->reservedMap[this] ){
                            req->trace()<<"evicted "<<blockEntryStr(entry)<<std::endl;
                            req->trace()<<"is this the problem?  blockIndex: "<< entry->id<<std::endl;
                            auto binBlocks = readBin(binIndex);
                            for( auto blk : binBlocks){
                                req->trace()<<blockEntryStr(blk)<<std::endl;
                            }
                        }
                        
                        //we done need this step since the bin is locked, no one else will access the block until data is written...
                        // req->trace()<<"update entry to writing "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                        // blockSet( entry, fileIndex, index, BLK_WR, req->originating->type(), entry->prefetched,0, req); //dont decrement...
                                                
                        req->trace()<<"writing data "<<blockEntryStr(entry)<<std::endl;
                        setBlockData(req->data,  entry->id, req->size);

                        req->trace()<<"update entry to avail "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                        blockSet( entry, fileIndex, index, BLK_AVAIL, req->originating->type(), entry->prefetched, -req->reservedMap[this], req); // there is a minus sign in front of req->reservedMap[this] so that we can appropriately decrement the block cnt, write the name of the originating cache so we can properly attribute stall time...
                        
                        if(entry->status == BLK_EVICT){
                             req->trace()<<"new entry/evict "<<" "<<(void*)entry<<std::endl;
                             req->trace()<<blockEntryStr( entry)<<" "<<(void*)entry<<std::endl;                             
                        }
                    }
                    else if (entry->status == BLK_AVAIL) {
                        req->trace()<<"update time "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                        blockSet( entry, fileIndex, index, BLK_AVAIL, req->originating->type(), entry->prefetched, -req->reservedMap[this], req);  // there is a minus sign in front of req->reservedMap[this] so that we can appropriately decrement the block cnt, update timestamp
                    }
                    else { //the writer will update the timestamp
                        req->trace()<<"other will update"<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    }
                    // if (req->reservedMap[this] > 0) {
                    //     req->trace()<<"decrememt "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    //     auto t_cnt = decBlkCnt(entry, req);
                    //     req->trace()<<" dec cnt "<<t_cnt<<std::endl;
                    //     if (t_cnt == 0) {
                    //         debug()<<req->str()<<std::endl;
                    //         log(this) << _name << " underflow in orig activecnt (" << t_cnt - 1 << ") for blkIndex: " <<  entry->id << " fileIndex: " << fileIndex << " index: " << index << std::endl;
                    //         exit(0);
                    //     }
                    // }
                    ret = true;
                }
                // else { }//no space in cache
                _binLock->writerUnlock(binIndex,req);
            }
            DPRINTF("end wb blk: %u out: %u\n", index, _outstanding.load());
             req->printTrace=false;    
            if (_nextLevel) {
                ret &= _nextLevel->writeBlock(req);
            }
            else{
                req->trace()<<"not sure how I got here"<<std::endl;
                delete req;
            }
        }
    }
    else { //we are terminating and this was an 'orphan request' (possibly from bypassing disk to goto network when resource balancing)
        if (req->originating == this) {
            cleanUpBlockData(req->data);
            delete req;
            ret = true;
        }
        if (_nextLevel) {
            ret &= _nextLevel->writeBlock(req);
        }
        else{
            req->trace()<<"not sure how I got here"<<std::endl;
            delete req;
        }
    }
    return ret;
}

template <class Lock>
void NewBoundedCache<Lock>::readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority) {
    stats.start(); //read
    stats.start(); //ovh
    bool prefetch = priority != 0;
    // if (_type == CacheType::boundedGlobalFile){
    //     req->printTrace=true;
    //     req->globalTrigger=true;
    // }
    // else{
    //     req->printTrace=false; 
    //     req->globalTrigger=false;
    // }
    req->trace()<<_name<<" READ BLOCK"<<std::endl;
    
    log(this) << _name << " entering read " << req->blkIndex << " " << req->fileIndex << " " << priority << " nl: " << _nextLevel->name() << std::endl;
    trackBlock(_name, (priority != 0 ? " [BLOCK_PREFETCH_REQUEST] " : " [BLOCK_REQUEST] "), req->fileIndex, req->blkIndex, priority);
    // if ((_nextLevel->name() == NETWORKCACHENAME && getRequestTime() > _nextLevel->getRequestTime()) && Timer::getCurrentTime() % 1000 < 999) {
    //     // if (getRequestTime() > _lastLevel->getRequestTime()) {
    //     // log(this) << _name << "time: " << getRequestTime()  << " "  << _lastLevel->name() << " time: " << _lastLevel->getRequestTime() << std::endl;
    //     // log(this) << " skipping: " << _name << std::endl;
    //     req->trace+=" skipping->";
    //     stats.end(prefetch, CacheStats::Metric::ovh);
    //     _lastLevel->readBlock(req, reads, priority);
    //     stats.end(prefetch, CacheStats::Metric::read);
    //     return;
    // }

    req->time = Timer::getCurrentTime();

    uint8_t *buff = nullptr;

    uint32_t binIndex = ~0;
    auto index = req->blkIndex;
    auto fileIndex = req->fileIndex;

    _localLock->readerLock(); //local lock
    int tsize = _fileMap[fileIndex].blockSize;
    _localLock->readerUnlock();
    binIndex = getBinIndex(index, fileIndex);

    if (!req->size) {
        req->size = tsize;
    }

    if (req->size <= _blockSize) {
        _binLock->writerLock(binIndex,req);
        BlockEntry *entry = oldestBlock(index, fileIndex, req); //int  entry->id = get entry->id(index, fileIndex, &entry);
        if (entry) { // an entry is available for this block
            req->trace()<<"found entry: "<<blockEntryStr(entry)<<" "<<" "<<(void*)entry<<std::endl;
            req->reservedMap[this] = 1; // indicate we will need to do a decrement in write
            if (entry->status == BLK_AVAIL){ //its a hit!
                trackBlock(_name, "[BLOCK_READ_HIT]", fileIndex, index, priority);
                req->trace()<<"hit "<<blockEntryStr(entry)<<std::endl;
                
                if (entry->prefetched > 0) {
                    stats.addAmt(prefetch, CacheStats::Metric::prefetches, 1);
                    entry->prefetched = prefetch;
                }
                else {
                    stats.addAmt(prefetch, CacheStats::Metric::hits, req->size);
                }
                req->trace()<<"update access time"<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                blockSet(entry, fileIndex, index, BLK_AVAIL, _type, entry->prefetched, 1, req); //increment is handled here
                stats.end(prefetch, CacheStats::Metric::ovh);
                stats.start(); // hits
                buff = getBlockData(entry->id);
                stats.end(prefetch, CacheStats::Metric::hits);
                stats.start(); // ovh
                req->data = buff;
                req->originating = this;
                req->ready = true;
                req->time = Timer::getCurrentTime() - req->time;
                req->indexMap[this]= entry->id;
                req->blkIndexMap[this]=entry->blockIndex;
                req->fileIndexMap[this]=entry->fileIndex;
                req->statusMap[this]=entry->status;
                updateRequestTime(req->time);
                req->trace()<<"done in hit "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                _binLock->writerUnlock(binIndex,req);
            }
            else{ // we have an entry but the data isnt there, either wait for someone else or grab ourselves
                req->trace()<<"miss "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                trackBlock(_name, "[BLOCK_READ_MISS_CLIENT]", fileIndex, index, priority);
                stats.addAmt(prefetch, CacheStats::Metric::misses, 1);
                
                if (entry->status == BLK_EMPTY || entry->status == BLK_EVICT){ // we need to get the data!
                    req->trace()<<"we reserve: "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    if (prefetch) {
                        blockSet(entry, fileIndex, index, BLK_PRE, _type, prefetch,1,req);//the incrememt happens here
                    }
                    else {
                        blockSet(entry, fileIndex, index, BLK_RES, _type, prefetch,1,req);//the increment happens here
                    }
                     _binLock->writerUnlock(binIndex,req);            
                    req->time = Timer::getCurrentTime() - req->time;
                    req->trace()<<"reserved go to next level "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    updateRequestTime(req->time);
                    stats.end(prefetch, CacheStats::Metric::ovh);
                    stats.start(); //miss
                    _nextLevel->readBlock(req, reads, priority);
                    stats.end(prefetch, CacheStats::Metric::misses);
                    stats.start(); //ovh
                }
                else { //BLK_WR || BLK_RES || BLK_PRE
                    
                    incBlkCnt(entry,req); // someone else has already reserved so lets incrememt
                    req->trace()<<"someone else reserved: "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    auto  blockIndex= entry->id;
                    _binLock->writerUnlock(binIndex,req);            
                    auto fut = std::async(std::launch::deferred, [this, req,  blockIndex, prefetch, entry] { 
                        uint64_t stime = Timer::getCurrentTime();
                        CacheType waitingCacheType = CacheType::empty;
                        uint64_t cnt = 0;
                        bool avail = false;
                        uint8_t *buff = NULL;
                        double curTime = (Timer::getCurrentTime() - stime) / 1000000000.0;
                        while (!avail && curTime < std::min(_lastLevel->getRequestTime() * 10,60.0) ) {                      // exit loop if request is 10x times longer than average network request or longer than 1 minute
                            avail = blockAvailable( blockIndex, req->fileIndex, true, cnt, &waitingCacheType); //maybe pass in a char* to capture the name of the originating cache?
                            sched_yield();
                            cnt++;
                            curTime = (Timer::getCurrentTime() - stime) / 1000000000.0;
                        }
                        if (avail) {
                            BlockEntry* entry2 = getBlockEntry(blockIndex, req);
                            req->trace()<<"received, entry: "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                            req->trace()<<"received, get data, (entry2):"<<blockEntryStr(entry2)<<" "<<(void*)entry2<<std::endl;
                            buff = getBlockData( blockIndex);
                            req->data = buff;
                            req->originating = this;
                            req->ready = true;
                            req->time = Timer::getCurrentTime() - req->time;
                            updateRequestTime(req->time);
                        }
                        else {
                            waitingCacheType = CacheType::empty;
                            std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> reads;
                            double reqTime = (Timer::getCurrentTime() - stime) / 1000000000.0;
                            req->retryTime = Timer::getCurrentTime();
                            req->trace()<<" retrying ("<<req->blkIndex<<","<<req->fileIndex<<")"<<" bi:"<< blockIndex<<std::endl;
                            _lastLevel->readBlock(req, reads, 0); //rerequest block from next level, note this updates the request data structure so we do not need to update it manually;
                            log(this) << Timer::printTime() << " " << _name << "timeout, rereqeusting block " << req->blkIndex <<" from "<<_lastLevel->name()<< " " << req->fileIndex << " " << getRequestTime() << " " << _lastLevel->getRequestTime() << " " << reqTime << " " << reads.size() << std::endl;
                            req->retryTime = Timer::getCurrentTime()-req->retryTime;
                            if (!req->ready){
                                reads[req->blkIndex].get().get();
                                waitingCacheType = req->waitingCache;
                            }
                            else{
                                waitingCacheType = _lastLevel->type();
                            }
                            req->trace()<<"recieved"<<std::endl;
                            log(this) <<"[TAZER] got block "<<req->blkIndex<<" after retrying! from: "<<req->originating->name()<<" waiting on cache type: "<<cacheTypeName(waitingCacheType)<<" "<<req->retryTime<<std::endl;
                        }

                        req->waitingCache = waitingCacheType;
                        std::promise<Request *> prom;
                        auto inner_fut = prom.get_future();
                        prom.set_value(req);
                        // log(this) << _name << " done read wait: blkIndex: " <<  entry->id << " fi: " << fileIndex << " i:" << index << std::endl;
                        return inner_fut.share();
                    });
                    reads[index] = fut.share();
                }

            } 
        }
        else{ // no space available
            _binLock->writerUnlock(binIndex,req);
        
            req->time = Timer::getCurrentTime() - req->time;
            req->trace()<<"no space got to next level ("<<req->blkIndex<<","<<req->fileIndex<<")"<<std::endl;
            updateRequestTime(req->time);
            stats.end(prefetch, CacheStats::Metric::ovh);
            stats.start(); //miss
            _nextLevel->readBlock(req, reads, priority);
            stats.end(prefetch, CacheStats::Metric::misses);
            stats.start(); //ovh
        }
    }
    else {
        *this /*std::cerr*/ << "[TAZER]"
                            << "shouldnt be here yet... need to handle" << std::endl;
        raise(SIGSEGV);
    }
    stats.end(prefetch, CacheStats::Metric::ovh);
    stats.end(prefetch, CacheStats::Metric::read);
}

//TODO: merge/reimplement from old cache structure...
template <class Lock>
void NewBoundedCache<Lock>::cleanReservation() {
}

template <class Lock>
void NewBoundedCache<Lock>::addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize) {
    // log(this) /*debug()*/<< "adding file: " << filename << " " << (void *)this << " " << (void *)_nextLevel << std::endl;
    // log(this) /*debug()*/ <<  _name << " " << filename << " " << fileSize << " " << blockSize << std::endl;
    trackBlock(_name, "[ADD_FILE]", index, blockSize, fileSize);

    _localLock->writerLock();
    if (_fileMap.count(index) == 0) {
        std::string hashstr(_name + filename); //should cause each level of the cache to have different indicies for a given file
        uint64_t hash = (uint64_t)XXH32(hashstr.c_str(), hashstr.size(), 0);

        _fileMap.emplace(index, FileEntry{filename, blockSize, fileSize, hash});

        //uint64_t temp = _fileMap[index];
    }
    _localLock->writerUnlock();
    // if (_nextLevel && _nextLevel->name() != NETWORKCACHENAME) { //quick hack to allow tasks to simulate unqiue files...
    if (_nextLevel) {
        _nextLevel->addFile(index, filename, blockSize, fileSize);
    }
}

// template class NewBoundedCache<ReaderWriterLock>;
template class NewBoundedCache<MultiReaderWriterLock>;
template class NewBoundedCache<FcntlBoundedReaderWriterLock>;
template class NewBoundedCache<FileLinkReaderWriterLock>;
