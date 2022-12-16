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

#include "BoundedCache.h"
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
#include <cmath>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)
//#define PPRINTF(...) fprintf(stdout, __VA_ARGS__); fflush(stdout)
//#define PPRINTFB(...) fprintf(stdout, __VA_ARGS__); fflush(stdout)
#define PPRINTF(...)
#define PPRINTFB(...)
#define TPRINTF(...) fprintf(stdout, __VA_ARGS__); fflush(stdout)
//#define TPRINTF(...)

template <class Lock>
BoundedCache<Lock>::BoundedCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, Cache * scalableCache) : Cache(cacheName,type),
                                                                                                                          _cacheSize(cacheSize),
                                                                                                                          _blockSize(blockSize),
                                                                                                                          _associativity(associativity),
                                                                                                                          _numBlocks(_cacheSize / _blockSize),
                                                                                                                          _collisions(0),
                                                                                                                          _prefetchCollisions(0),
                                                                                                                          _outstanding(0),
                                                                                                                          evictHisto(100),
                                                                                                                          _scalableCache(scalableCache) {

    // log(this) /*debug()*/<< "Constructing " << _name << " in BoundedCache" << std::endl;
    stats.start(false, CacheStats::Metric::constructor);
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
    debug()<< _name << " BB " << _cacheSize << " " << _blockSize << " " << _numBlocks << " " << _associativity << " " << _numBins << std::endl;

    _localLock = new ReaderWriterLock();
    stats.end(false, CacheStats::Metric::constructor);
    PPRINTF("*********************%s SETTING SCALABLE CACHE HERE %p blockSize %lu\n", _name.c_str(), _scalableCache, _blockSize);
}

template <class Lock>
BoundedCache<Lock>::~BoundedCache() {
    _terminating = true;
    log(this) << _name << " cache collisions: " << _collisions.load() << " prefetch collisions: " << _prefetchCollisions.load() << std::endl;
    // for (uint32_t i = 0; i < _numBlocks; i++) {
    //     log(this) /*debug()*/<< i << std::endl;
    //     if (_blkIndex[i].activeCnt > 0) {
    //         log(this) /*debug()*/<< i << " " << _numBlocks << " " << _blkIndex[i].activeCnt.load() << " " << _blkIndex[i].status << std::endl;
    //     }
    // }
    
    evictHisto.printBins();

    log(this) << "deleting " << _name << " in BoundedCache" << std::endl;
    delete _localLock;
}


template <class Lock>
std::shared_ptr<typename BoundedCache<Lock>::BlockEntry> BoundedCache<Lock>::getCompareBlkEntry(uint32_t index, uint32_t fileIndex) {
    std::shared_ptr<BlockEntry> entry = std::make_shared<BlockEntry>();
    entry->blockIndex= index;
    entry->fileIndex = fileIndex;
    return entry;
}

template <class Lock>
bool BoundedCache<Lock>::sameBlk(BlockEntry *blk1, BlockEntry *blk2) {
    return blk1->fileIndex == blk2->fileIndex && blk1->blockIndex == blk2->blockIndex;
}

template <class Lock>
uint32_t BoundedCache<Lock>::getBinIndex(uint32_t index, uint32_t fileIndex) {
    _localLock->readerLock();
    uint64_t temp = _fileMap[fileIndex].hash + index;
    _localLock->readerUnlock();
    return (temp % (_numBins));
}

template <class Lock>
uint32_t BoundedCache<Lock>::getBinOffset(uint32_t index, uint32_t fileIndex) {
    _localLock->readerLock();
    uint64_t temp = _fileMap[fileIndex].hash + index;
    _localLock->readerUnlock();
    return (temp % (_numBins)) * _associativity;
}

template <class Lock>
typename BoundedCache<Lock>::BlockEntry* BoundedCache<Lock>::getBlock(uint32_t index, uint32_t fileIndex, Request* req){
    req->trace(_name)<<"searching for block: "<<index<<" "<<fileIndex<<std::endl;
    uint32_t binIndex = getBinIndex(index, fileIndex);
    auto blkEntries = readBin(binIndex);
    auto cmpBlk = getCompareBlkEntry(index, fileIndex);

    for (uint32_t i = 0; i < _associativity; i++) {
        if (sameBlk(blkEntries[i], cmpBlk.get())) {
            if (blkEntries[i]->status == BLK_AVAIL) {
                req->trace(_name)<<"found block: "<<blockEntryStr(blkEntries[i])<<std::endl;
                return blkEntries[i];
            }
        }
    }
    req->trace(_name)<<"block not found "<<std::endl;
    return NULL;
}

template <class Lock>
typename BoundedCache<Lock>::BlockEntry* BoundedCache<Lock>::oldestBlock(uint32_t index, uint32_t fileIndex, Request* req) {
    req->trace(_name)<<"searching for oldest block: "<<index<<" "<<fileIndex<<std::endl;
    BlockEntry* blkEntry = NULL;
    uint64_t minTime = -1; //this is max uint64_t
    BlockEntry *minEntry = NULL;
    uint64_t minPrefetchTime = -1; //Prefetched block
    BlockEntry *minPrefetchEntry = NULL;
    uint32_t binIndex = getBinIndex(index, fileIndex);
    auto cmpBlk = getCompareBlkEntry(index, fileIndex);
    BlockEntry *entry = cmpBlk.get();
    req->trace(_name)<<"bin index: "<<binIndex<<std::endl;
    auto blkEntries = readBin(binIndex);

    BlockEntry *victimEntry = NULL;
    uint64_t victimTime = -1;
    double victimMinUMB = std::numeric_limits<double>::max();
    double minUMBInCache = std::numeric_limits<double>::max();
    for (uint32_t i = 0; i < _associativity; i++) { // maybe we want to split this into two loops--first to check if any empty or if its here, then a lru pass, other wise we require checking the number of active users on every block which can be expensive for file backed caches
        //Find actual, empty, or oldest
        blkEntry = blkEntries[i];
        
        if (sameBlk(blkEntry, entry)) { //block is present
            req->trace(_name)<<"found entry: "<<blockEntryStr(blkEntry)<<std::endl;
            return blkEntry;
        }
        if (blkEntry->status == BLK_EMPTY) { //The space is empty!!!
            req->trace(_name)<<"found  empty entry: "<<blockEntryStr(blkEntry)<<std::endl;
            return blkEntry;
        }
    }

    //JS: Update my UMB list here
    PPRINTF("############################ oldestBlock %s %p ############################\n", _name.c_str(), _scalableCache);
    double askingUMB;
    
    if(_scalableCache) {
        auto umbs = ((ScalableCache*)_scalableCache)->getLastUMB(static_cast<Cache*>(this));
        PPRINTF("%s UMB SIZE %u\n", _name.c_str(), umbs.size());
        if(umbs.size()) {
            setLastUMB(umbs);
        }
        askingUMB = getLastUMB(fileIndex);

        //first loop to find the min umb in the cache
        
        for (uint32_t i = 0; i < _associativity; i++) {
            blkEntry = blkEntries[i];
            auto umbblock = getLastUMB(blkEntry->fileIndex);
            if(umbblock < minUMBInCache){
                minUMBInCache = umbblock;
            }
        }
    }
    
    double sigmoidThreshold = Config::UMBThreshold;
    //std::cout<<"sigmoid threshold: "<<sigmoidThreshold<<std::endl;

    for (uint32_t i = 0; i < _associativity; i++) { //loop to find oldest block and block with lowest umb(if scalable piggyback is on)
        blkEntry = blkEntries[i];
        if (blkEntry->status == BLK_AVAIL) {// we found an available block, deterimine if we evict it
            if(_scalableCache) {
                auto umbblock = getLastUMB(blkEntry->fileIndex);
                PPRINTFB("%d:%d, INCOMING file:%d umb:%.10lf, BLOCK file:%d umb:%.10lf time:%lu\n",
                    binIndex,i, fileIndex, askingUMB, blkEntry->fileIndex,umbblock, blkEntry->timeStamp);

                if(umbblock <= minUMBInCache + sigmoidThreshold){ //we found a possible victim block
                    if (!anyUsers(blkEntry,req)  &&  (blkEntry->timeStamp < victimTime)){ //pick the oldest among minUMB + threshold
                        victimTime = blkEntry->timeStamp;
                        victimMinUMB = umbblock;
                        victimEntry = blkEntry;
                    }
                }
                // else if(umbblock == victimMinUMB){ //check if this block is older, then select this block as victim 
                //     if (!anyUsers(blkEntry,req) &&  (blkEntry->timeStamp < victimTime)) {
                //             victimTime = blkEntry->timeStamp;
                //             victimMinUMB = umbblock;
                //             victimEntry = blkEntry;
                //     }
                // }
            }

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
    //gone through all options in the bin 
    // we have LRU block kept in minEntry
    // we have lowest UMB block kept in victimEntry 
    PPRINTFB("-LRU in bin: file: %d, time:%lu\n", minEntry->fileIndex, minTime);
    PPRINTFB("-minUMB    : file: %d, time:%lu, umb::%.10lf\n", victimEntry->fileIndex, victimTime, victimMinUMB);

    std::thread::id thread_id = req->threadId;
    //If a prefetched block is found, we evict it
    if (Config::prefetchEvict && minPrefetchTime != (uint64_t)-1 && minPrefetchEntry) {
        _prefetchCollisions++;
        trackBlock(_name, "[BLOCK_EVICTED]", fileIndex, index, 1);
        evictHisto.addData((double) fileIndex, (double) 1);
        minPrefetchEntry->status = BLK_EVICT;

        req->trace(_name)<<"evicting prefected entry: "<<blockEntryStr(minPrefetchEntry)<<std::endl;
        stats.addAmt(1, CacheStats::Metric::evictions, 1, thread_id);
        return minPrefetchEntry;
    }
    
    //here we decide on the right action for scalable piggyback 
    if(_scalableCache){
        //Did we find a space --  it's possible none of the blocks were available, that case we skip this level for caching
        if (victimTime != (uint64_t)-1 && victimEntry && minTime != (uint64_t)-1 && minEntry) { 

            if(askingUMB + sigmoidThreshold >= victimMinUMB ){
                //evict lowest umb 
                minTime = victimTime;
                minEntry = victimEntry;
                PPRINTF("%s SCALE METRIC PIGGYBACK %lf\n", _name.c_str(), victimMinUMB);
                PPRINTFB("%.10lf, is kicking out %.10lf, with id:%d\n" , askingUMB, victimMinUMB, -1);
                _collisions++;
                trackBlock(_name, "[BLOCK_EVICTED]", fileIndex, index, 0);
                evictHisto.addData((double) fileIndex, (double) 1);
                minEntry->status = BLK_EVICT;

                req->trace(_name)<<"evicting  entry: "<<blockEntryStr(minEntry)<<std::endl;
                stats.addAmt(0, CacheStats::Metric::evictions, 1, thread_id);
                return minEntry;
            }
            else{
                //skip caching for this block 
                log(this)<< _name << " No available spots for this UMB value " << std::endl;
                req->trace(_name)<<"no entries found for UMB"<<std::endl;
                PPRINTFB("skipping %lf for this cache\n" , askingUMB);
                return NULL;
            }
        }
        log(this)<< _name << " No available spots for this UMB value " << std::endl;
        req->trace(_name)<<"no entries found for UMB"<<std::endl;
        PPRINTFB("skipping %lf for this cache\n" , askingUMB);
        return NULL;
    } //end of scalable piggyback section

    if (minTime != (uint64_t)-1 && minEntry) { //LRU version -- Did we find a space
        _collisions++;
        trackBlock(_name, "[BLOCK_EVICTED]", fileIndex, index, 0);
        evictHisto.addData((double) fileIndex, (double) 1);
        minEntry->status = BLK_EVICT;

        req->trace(_name)<<"evicting  entry: "<<blockEntryStr(minEntry)<<std::endl;
        stats.addAmt(0, CacheStats::Metric::evictions, 1, thread_id);
        return minEntry;
    }
    log(this)<< _name << " All space is reserved..." << std::endl;
    req->trace(_name)<<"no entries found"<<std::endl;
    return NULL;
}

template <class Lock>
void BoundedCache<Lock>::cleanUpBlockData(uint8_t *data) {
    // debug()<<_name<<" (not) delete data"<<std::endl;
}

template <class Lock>
bool BoundedCache<Lock>::writeBlock(Request *req) {
    if (_type == CacheType::globalFileLock){
        req->printTrace=false;
        req->globalTrigger=false;
    }
    else{
        req->printTrace=false; 
        req->globalTrigger=false;
    }
    req->trace(_name)<<_name<<" WRITE BLOCK"<<std::endl;
    
    bool ret = false;
    if (req->reservedMap[this] > 0 || !_terminating) { //when terminating dont waste time trying to write orphan requests
        auto index = req->blkIndex;
        auto fileIndex = req->fileIndex;
        auto binIndex = getBinIndex(index, fileIndex);
        if (req->originating == this) {
            req->trace(_name)<<"originating cache"<<std::endl;
            _binLock->writerLock(binIndex,req);
            BlockEntry* entry = getBlock(index, fileIndex, req);
            if (entry) {
                if (req->reservedMap[this] > 0) {
                    req->trace(_name)<<"decrement "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    auto t_cnt = decBlkCnt(entry, req);
                    req->trace(_name)<<" dec cnt "<<t_cnt<<std::endl;
                    if (t_cnt == 0) {
                        debug()<<req->str()<<std::endl;
                        log(this) << _name << " underflow in orig activecnt (" << t_cnt - 1 << ") for blkIndex: " <<  entry->id << " fileIndex: " << fileIndex << " index: " << index << std::endl;
                        debug()<<"EXITING!!!!"<<__FILE__<<" "<<__LINE__<<std::endl;
                        exit(0);
                    }
                }
            }
            // the "else" would trigger in the case this cache was completely filled and active when the request was initiated so we didnt have a reservation for this block
            else {
                req->trace(_name) << "writeblock should this even be possible?" << std::endl;
                entry = getBlockEntry(req->indexMap[this], req); 
                req->trace(_name) <<" entry: "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                debug()<<req->str()<<std::endl;
                // debug()<<"EXITING!!!!"<<__FILE__<<" "<<__LINE__<<<<std::endl;
                // exit(0);
            }
            _binLock->writerUnlock(binIndex,req);
            // debug()<<_type<<" deleting data "<<req->id<<std::endl;
            req->trace(_name)<<"deleting req"<<std::endl;  
            req->printTrace=false;           
            delete req;
            
            ret = true;
        }
        else {

            req->trace(_name)<<"not originating cache"<<std::endl;

            DPRINTF("beg wb blk: %u out: %u\n", index, _outstanding.load());

            if (req->size <= _blockSize) {
                _binLock->writerLock(binIndex,req);
                
                BlockEntry* entry = oldestBlock(index, fileIndex, req);
                trackBlock(_name, "[BLOCK_WRITE]", fileIndex, index, 0);

                if (entry){ //a slot for the block is present in the cache       
                    if ((entry->status != BLK_WR && entry->status != BLK_AVAIL) || entry->status == BLK_EVICT) { //not found means we evicted some block
                        if(entry->status == BLK_EVICT && req->reservedMap[this] ){
                            req->trace(_name)<<"evicted "<<blockEntryStr(entry)<<std::endl;
                            req->trace(_name)<<"is this the problem?  blockIndex: "<< entry->id<<std::endl;
                            auto binBlocks = readBin(binIndex);
                            for( auto blk : binBlocks){
                                req->trace(_name)<<blockEntryStr(blk)<<std::endl;
                            }
                        }
                        
                        //we done need this step since the bin is locked, no one else will access the block until data is written...
                        // req->trace(_name)<<"update entry to writing "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                        // blockSet( entry, fileIndex, index, BLK_WR, req->originating->type(), entry->prefetched,0, req); //dont decrement...
                                                
                        req->trace(_name)<<"writing data "<<blockEntryStr(entry)<<std::endl;
                        setBlockData(req->data,  entry->id, req->size);


                        req->trace(_name)<<"update entry to avail "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                        blockSet( entry, fileIndex, index, BLK_AVAIL, req->originating->type(), entry->prefetched, -req->reservedMap[this], req); // -req->reservedMap to do decrement -- write the name of the originating cache so we can properly attribute stall time...
                        
                        if(entry->status == BLK_EVICT){
                             req->trace(_name)<<"new entry/evict "<<" "<<(void*)entry<<std::endl;
                             req->trace(_name)<<blockEntryStr( entry)<<" "<<(void*)entry<<std::endl;                             
                        }
                    }
                    else if (entry->status == BLK_AVAIL) {

                        req->trace(_name)<<"update time "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                        blockSet( entry, fileIndex, index, BLK_AVAIL, req->originating->type(), entry->prefetched, -req->reservedMap[this], req); // -req->reservedMap to do decrement update timestamp
                    }
                    else { //the writer will update the timestamp
                        req->trace(_name)<<"other will update"<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    }
                    // if (req->reservedMap[this] > 0) {
                    //     req->trace(_name)<<"decrememt "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    //     auto t_cnt = decBlkCnt(entry, req);
                    //     req->trace(_name)<<" dec cnt "<<t_cnt<<std::endl;
                    //     if (t_cnt == 0) {
                    //         debug()<<req->str()<<std::endl;
                    //         log(this) << _name << " underflow in orig activecnt (" << t_cnt - 1 << ") for blkIndex: " <<  entry->id << " fileIndex: " << fileIndex << " index: " << index << std::endl;
                    //         exit(0);
                    //     }
                    // }
                    ret = true;
                }
                // if (_type == CacheType::privateMemory){
                //     req->trace(_name) <<" entry: "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                //     // debug()<<req->str()<<std::endl;
                //     req->printTrace=false;  
                // }
                // else { }//no space in cache
                _binLock->writerUnlock(binIndex,req);
            }
            DPRINTF("end wb blk: %u out: %u\n", index, _outstanding.load());
            //  req->printTrace=false;    
             
            if (_nextLevel) {
                ret &= _nextLevel->writeBlock(req);
            }
            else{
                req->trace(_name)<<"not sure how I got here"<<std::endl;
                delete req;
            }
        }
    }
    else { //we are terminating and this was an 'orphan request' (possibly from bypassing disk to goto network when resource balancing)
        if (req->originating == this) {
            delete req;
            ret = true;
        }
        if (_nextLevel) {
            ret |= _nextLevel->writeBlock(req); //or = here because this was an orphan request so we werent on the hook to actually write the block, but we do want to report if the above level fails
        }
        else{
            req->trace(_name)<<"not sure how I got here"<<std::endl;
            delete req;
        }
    }
    return ret;
}

template <class Lock>
void BoundedCache<Lock>::readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority) {
    std::thread::id thread_id = req->threadId;
    stats.start((priority != 0), CacheStats::Metric::read, thread_id); //read
    stats.start((priority != 0), CacheStats::Metric::ovh, thread_id); //ovh

    bool prefetch = priority != 0;

    if (_type == CacheType::globalFileLock){
        req->printTrace=false;
        req->globalTrigger=false;
    }
    else{
        req->printTrace=false; 
        req->globalTrigger=false;
    }
    req->trace(_name)<<" READ BLOCK"<<std::endl;
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
            req->trace(_name)<<"found entry: "<<blockEntryStr(entry)<<" "<<" "<<(void*)entry<<std::endl;
            req->reservedMap[this] = 1; // indicate we will need to do a decrement in write
            req->indexMap[this]= entry->id;
            req->blkIndexMap[this]=entry->blockIndex;
            req->fileIndexMap[this]=entry->fileIndex;
            req->statusMap[this]=entry->status;
            if (entry->status == BLK_AVAIL){ //its a hit!
                trackBlock(_name, "[BLOCK_READ_HIT]", fileIndex, index, priority);
                req->trace(_name)<<"hit "<<blockEntryStr(entry)<<std::endl;
                
                if (entry->prefetched > 0) {
                    stats.addAmt(prefetch, CacheStats::Metric::prefetches, 1, thread_id);
                    entry->prefetched = prefetch;
                }
                else {
                    stats.addAmt(prefetch, CacheStats::Metric::hits, req->size, thread_id);
                }
                req->trace(_name)<<"update access time"<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                blockSet(entry, fileIndex, index, BLK_AVAIL, _type, entry->prefetched, 1, req); //increment is handled here
                stats.end(prefetch, CacheStats::Metric::ovh, thread_id);
                stats.start(prefetch, CacheStats::Metric::hits, thread_id); // hits
                buff = getBlockData(entry->id);
                stats.end(prefetch, CacheStats::Metric::hits, thread_id);
                stats.start(prefetch, CacheStats::Metric::ovh, thread_id); // ovh
                req->data = buff;
                req->originating = this;
                req->ready = true;
                req->time = Timer::getCurrentTime() - req->time;
                updateRequestTime(req->time);
                req->trace(_name)<<"done in hit "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                _binLock->writerUnlock(binIndex,req);
            }
            else{ // we have an entry but the data isnt there, either wait for someone else or grab ourselves
                req->trace(_name)<<"miss "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                trackBlock(_name, "[BLOCK_READ_MISS_CLIENT]", fileIndex, index, priority);
                stats.addAmt(prefetch, CacheStats::Metric::misses, 1, thread_id);
                
                if (entry->status == BLK_EMPTY || entry->status == BLK_EVICT){ // we need to get the data!
                    req->trace(_name)<<"we reserve: "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    if (prefetch) {
                        blockSet(entry, fileIndex, index, BLK_PRE, _type, prefetch,1,req);//the incrememt happens here
                    }
                    else {
                        blockSet(entry, fileIndex, index, BLK_RES, _type, prefetch,1,req);//the increment happens here
                    }
                     _binLock->writerUnlock(binIndex,req);            
                    req->time = Timer::getCurrentTime() - req->time;
                    req->trace(_name)<<"reserved go to next level "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    updateRequestTime(req->time);
                    stats.end(prefetch, CacheStats::Metric::ovh, thread_id);
                    stats.start(prefetch, CacheStats::Metric::misses, thread_id); //miss
                    _nextLevel->readBlock(req, reads, priority);
                    stats.end(prefetch, CacheStats::Metric::misses, thread_id);
                    stats.start(prefetch, CacheStats::Metric::ovh, thread_id); //ovh
                }
                else { //BLK_WR || BLK_RES || BLK_PRE

                    stats.end(prefetch, CacheStats::Metric::ovh, thread_id);
                    stats.start(prefetch, CacheStats::Metric::misses, thread_id);
                    incBlkCnt(entry,req); // someone else has already reserved so lets incrememt
                    req->trace(_name)<<"someone else reserved: "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
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
                            req->trace(_name)<<"received, entry: "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                            req->trace(_name)<<"received, get data, (entry2):"<<blockEntryStr(entry2)<<" "<<(void*)entry2<<std::endl;
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
                            req->trace(_name)<<" retrying ("<<req->blkIndex<<","<<req->fileIndex<<")"<<" bi:"<< blockIndex<<std::endl;
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
                            req->trace(_name)<<"recieved"<<std::endl;
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
                    stats.end(prefetch, CacheStats::Metric::misses, thread_id);
                    stats.start(prefetch, CacheStats::Metric::ovh, thread_id); 
                }

            } 
        }
        else{ // no space available
            _binLock->writerUnlock(binIndex,req);
            trackBlock(_name, "[BLOCK_READ_MISS_CLIENT]", fileIndex, index, priority);
            stats.addAmt(prefetch, CacheStats::Metric::misses, 1, thread_id);
            req->time = Timer::getCurrentTime() - req->time;
            req->trace(_name)<<"no space got to next level ("<<req->blkIndex<<","<<req->fileIndex<<")"<<std::endl;
            updateRequestTime(req->time);
            stats.end(prefetch, CacheStats::Metric::ovh, thread_id);
            stats.start(prefetch, CacheStats::Metric::misses, thread_id); //miss
            _nextLevel->readBlock(req, reads, priority);
            stats.end(prefetch, CacheStats::Metric::misses, thread_id);
            stats.start(prefetch, CacheStats::Metric::ovh, thread_id); //ovh
        }
    }
    else {
        *this /*std::cerr*/ << "[TAZER]"
                            << "shouldnt be here yet... need to handle" << std::endl;
        debug()<<"EXITING!!!!"<<__FILE__<<" "<<__LINE__<<std::endl;
        raise(SIGSEGV);
    }
    stats.end(prefetch, CacheStats::Metric::ovh, thread_id);
    stats.end(prefetch, CacheStats::Metric::read, thread_id);
}

//TODO: merge/reimplement from old cache structure...
template <class Lock>
void BoundedCache<Lock>::cleanReservation() {
}

template <class Lock>
void BoundedCache<Lock>::addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize) {
    // log(this) /*debug()*/<< "adding file: " << filename << " " << (void *)this << " " << (void *)_nextLevel << std::endl;
    // log(this) /*debug()*/ <<  _name << " " << filename << " " << fileSize << " " << blockSize << std::endl;
    trackBlock(_name, "[ADD_FILE]", index, blockSize, fileSize);
    std::cout<<"ADDFILE in"<<_name<<" "<<index<<":"<<filename<<std::endl;
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


template <class Lock>
double BoundedCache<Lock>::getLastUMB(uint32_t fileIndex) { 
    PPRINTF("********************* %s getLastUMB Nothing Here*********************\n", _name.c_str()); 
    return std::numeric_limits<double>::max();
}

template <class Lock>
void BoundedCache<Lock>::setLastUMB(std::vector<std::tuple<uint32_t, double>> &UMBList) {
    PPRINTF("********************* %s setLastUMB Nothing Here*********************\n", _name.c_str()); 
    return;
}

template <class Lock>
     void BoundedCache<Lock>::addBlocktoFileCacheCount(uint32_t index){return;}

    template <class Lock>
     void BoundedCache<Lock>::decBlocktoFileCacheCount(uint32_t index){return;}

    template <class Lock>
     uint32_t BoundedCache<Lock>::getFileCacheCount(uint32_t index){return -5;}

// template class BoundedCache<ReaderWriterLock>;
template class BoundedCache<MultiReaderWriterLock>;
template class BoundedCache<FcntlBoundedReaderWriterLock>;
template class BoundedCache<FileLinkReaderWriterLock>;

// template <class Lock>
// void BoundedCache<Lock>::trackBlock(std::string cacheName, std::string action, uint32_t fileIndex, uint32_t  blockIndex, uint64_t priority) {
//     if (Config::TrackBlockStats) {
//         auto fut = std::async(std::launch::async, [cacheName, action, fileIndex,  blockIndex, priority] {
//             unixopen_t unixopen = (unixopen_t)dlsym(RTLD_NEXT, "open");
//             unixclose_t unixclose = (unixclose_t)dlsym(RTLD_NEXT, "close");
//             unixwrite_t unixwrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");

//             int fd = (*unixopen)("block_stats.txt", O_WRONLY | O_APPEND | O_CREAT, 0660);
//             if (fd != -1) {
//                 std::stringstream ss;
//                 ss << cacheName << " " << action << " " << fileIndex << " " <<  blockIndex << " " << priority << std::endl;
//                 unixwrite(fd, ss.str().c_str(), ss.str().length());
//                 unixclose(fd);
//             }
//         });
//     }
// }
