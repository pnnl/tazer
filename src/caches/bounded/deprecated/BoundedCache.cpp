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
BoundedCache<Lock>::BoundedCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity) : Cache(cacheName,type),
                                                                                                                          _cacheSize(cacheSize),
                                                                                                                          _blockSize(blockSize),
                                                                                                                          _associativity(associativity),
                                                                                                                          _numBlocks(_cacheSize / _blockSize),
                                                                                                                          _collisions(0),
                                                                                                                          _prefetchCollisions(0),
                                                                                                                          _outstanding(0) {

    // log(this) /*debug()*/<< "Constructing " << _name << " in Boundedcache" << std::endl;
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
BoundedCache<Lock>::~BoundedCache() {
    _terminating = true;
    log(this) << _name << " cache collisions: " << _collisions.load() << " prefetch collisions: " << _prefetchCollisions.load() << std::endl;
    // for (uint32_t i = 0; i < _numBlocks; i++) {
    //     log(this) /*debug()*/<< i << std::endl;
    //     if (_blkIndex[i].activeCnt > 0) {
    //         log(this) /*debug()*/<< i << " " << _numBlocks << " " << _blkIndex[i].activeCnt.load() << " " << _blkIndex[i].status << std::endl;
    //     }
    // }

    log(this) << "deleting " << _name << " in Boundedcache" << std::endl;
    delete _localLock;
}
template <class Lock>
std::string BoundedCache<Lock>::blockEntryStr(uint32_t index){
    return _name+" block entry str";
}

template <class Lock>
void BoundedCache<Lock>::getCompareBlkEntry(uint32_t index, uint32_t fileIndex, BlockEntry *entry) {
    entry->blockIndex = index + 1;
    entry->fileIndex = fileIndex + 1;
}

template <class Lock>
std::shared_ptr<typename BoundedCache<Lock>::BlockEntry> BoundedCache<Lock>::getCompareBlkEntry(uint32_t index, uint32_t fileIndex) {
    std::shared_ptr<BlockEntry> entry = std::make_shared<BlockEntry>();
    entry->blockIndex = index + 1;
    entry->fileIndex = fileIndex + 1;
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
int BoundedCache<Lock>::getBlockIndex(uint32_t index, uint32_t fileIndex, BlockEntry *entry) {

    uint32_t binOffset = getBinOffset(index, fileIndex); //index % _numBins;
    uint32_t binIndex = getBinIndex(index, fileIndex);
    auto blkEntries = readBin(binIndex);
    auto cmpBlk = getCompareBlkEntry(index, fileIndex);

    for (uint32_t i = 0; i < _associativity; i++) {
        if (sameBlk(blkEntries[i].get(), cmpBlk.get())) {
            if (blkEntries[i]->status == BLK_AVAIL) {
                if (entry != NULL) {
                    entry->timeStamp = blkEntries[i]->timeStamp;
                    entry->prefetched = blkEntries[i]->prefetched;
                }
                return i + binOffset;
            }
        }
    }
    return -1;
}

template <class Lock>
int BoundedCache<Lock>::oldestBlockIndex(uint32_t index, uint32_t fileIndex, bool &found, Request* req) {
    found = false;
    uint32_t minTime = -1; //this is max uint32_t
    uint32_t minIndex = -1;
    uint32_t minPrefetchTime = -1; //Prefetched block
    uint32_t minPrefetchIndex = -1;
    uint32_t binIndex = getBinIndex(index, fileIndex);
    uint32_t binOffset = getBinOffset(index, fileIndex);
    auto cmpBlk = getCompareBlkEntry(index, fileIndex);
    BlockEntry *entry = cmpBlk.get();
    auto blkEntries = readBin(binIndex);

    for (uint32_t i = 0; i < _associativity; i++) { // maybe we want to split this into two loops--first to check if any empty or if its here, then a lru pass, other wise we require checking the number of active users on every block which can be expensive for file backed caches
        //Find actual, empty, or oldest
        if (blkEntries[i]->status == BLK_EMPTY) { //The space is empty!!!
            found = false;
            return i + binOffset;
        }
        else if (blkEntries[i]->status == BLK_RES || blkEntries[i]->status == BLK_PRE) { //The space is reserved..
            if (sameBlk(blkEntries[i].get(), entry)) {                                   //Reserved for us?
                found = true;
                return i + binOffset;
            }
        }
        else if (blkEntries[i]->status == BLK_WR) {
            if (sameBlk(blkEntries[i].get(), entry)) { //someone is currently writing this block...
                found = true;
                return i + binOffset;
            }
        }
        else if (blkEntries[i]->status == BLK_AVAIL) {
            //TODO: Implement other policies apart from LRU - evict based on PRIORITY
            if (sameBlk(blkEntries[i].get(), entry)) { //Well this block is already here
                found = true;
                return -1;
            }
            //LRU
            if (blkEntries[i]->timeStamp < minTime) { //Look for an old one
                if (!anyUsers(i + binOffset,req)) { //can optimize a read since we already have the blkEntry, pass the pointer to the entry?
                    minTime = blkEntries[i]->timeStamp;
                    minIndex = i + binOffset;
                }
            }

            //PrefetchEvict policy evicts prefetched blocks first
            if (Config::prefetchEvict && blkEntries[i]->prefetched && blkEntries[i]->timeStamp < minPrefetchTime) {
                if (!anyUsers(i + binOffset,req)) {
                    minPrefetchTime = blkEntries[i]->timeStamp;
                    minPrefetchIndex = i + binOffset;
                }
            }
        }
    }

    //If a prefetched block is found, we evict it
    if (Config::prefetchEvict && minPrefetchTime != (uint32_t)-1 && minPrefetchIndex < _numBlocks) {
        _prefetchCollisions++;
        trackBlock(_name, "[BLOCK_EVICTED]", fileIndex, index, 1);

        return minPrefetchIndex;
    }
    if (minTime != (uint32_t)-1 && minIndex < _numBlocks) { //Did we find a space
        _collisions++;
        trackBlock(_name, "[BLOCK_EVICTED]", fileIndex, index, 0);

        // log(this) /*debug()*/<< _name << " evicting: " << minIndex << " " << _blkIndex[minIndex].blockIndex - 1 << " (" << _blkIndex[minIndex].activeCnt.load() << ") "
        //           << " for " << index << std::endl;
        return minIndex;
    }
    log(this)<< _name << " All space is reserved..." << std::endl;
    return -1;
}

//Must lock first!
template <class Lock>
bool BoundedCache<Lock>::blockReserve(uint32_t index, uint32_t fileIndex, bool &found, int &reservedIndex, Request* req, bool prefetch) {
    bool ret = false;
    DPRINTF("beg br blk: %u out: %u ret %u fi %u Full: %u < %u\n", index, _outstanding.load(), ret, fileIndex, _fullFlag->load(), _numBlocks);
    reservedIndex = oldestBlockIndex(index, fileIndex, found, req);
    // log(this) /*debug()*/ << " " << _name << "reserving: " << index << " " << reservedIndex << " " << found << " p: " << prefetch << std::endl;
    if (!found && reservedIndex > -1) {
        if (prefetch) {
            blockSet(reservedIndex, fileIndex, index, BLK_PRE, _type, prefetch);
        }
        else {
            blockSet(reservedIndex, fileIndex, index, BLK_RES, _type, prefetch);
        }
        ret = true;
    }
    DPRINTF("blk_i: %u\n", reservedIndex);
    DPRINTF("end br blk: %u out: %u ret %u found: %u\n", index, _outstanding.load(), ret, found);
    return ret;
}

template <class Lock>
bool BoundedCache<Lock>::writeBlock(Request *req) {
    req->trace(_name)<<_name<<" WRITE BLOCK"<<std::endl;
    // log(this) /*debug()*/<< _name << " entering write: " << index << " " << size << " " << _blockSize << " " << (void *)buffer << " " << (void *)originating << " " << (void *)_nextLevel << std::endl;
    // log(this) << "write " << _name << " fi: " << req->fileIndex << " i: " << req->blkIndex << " orig: " << req->originating->name() <<" "<<(uint32_t)req->reservedMap[this]<< std::endl;
    
    bool ret = false;
    if (req->reservedMap[this] > 0 || !_terminating) { //when terminating dont waste time trying to write orphan requests
        auto index = req->blkIndex;
        auto fileIndex = req->fileIndex;
        auto binIndex = getBinIndex(index, fileIndex);
        if (req->originating == this) {
            req->trace(_name)<<"originating cache"<<std::endl;
            
            req->trace(_type == CacheType::boundedGlobalFile)<<"wlocking bin "<<binIndex<<std::endl;
            _binLock->writerLock(binIndex,req);
            req->trace(_type == CacheType::boundedGlobalFile)<<"wlocked bin "<<binIndex<<std::endl;

            req->trace(_type == CacheType::boundedGlobalFile)<<"find allocated entry ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<std::endl;
            int blockIndex = getBlockIndex(index, fileIndex);
            req->trace(_type == CacheType::boundedGlobalFile)<<"blockindex returned: "<<blockIndex<<::getpid()<<std::endl;
            // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocking bin "<<binIndex<<std::endl;
            // _binLock->writerUnlock(binIndex,req);
            // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocked bin "<<binIndex<<std::endl;
            if (blockIndex >= 0) {
                req->trace(_type == CacheType::boundedGlobalFile)<<"entry found ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                if (req->reservedMap[this] > 0) {
                    req->trace(_type == CacheType::boundedGlobalFile)<<"decrement "<<blockIndex<<" ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                    auto t_cnt = decBlkCnt(blockIndex, req);
                    if (t_cnt == 0) {
                        log(this) << _name << " underflow in orig activecnt (" << t_cnt - 1 << ") for blkIndex: " << blockIndex << " fileIndex: " << fileIndex << " index: " << index << std::endl;
                    }
                }
            }
            // the "else" would trigger in the case this cache was completely filled and active when the request was initiated so we didnt have a reservation for this block
            else {
                req->trace(_name) << "entry not found ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                req->trace(_name) << "writeblock should this even be possible?" << std::endl;
                req->trace(_name) << blockEntryStr(req->indexMap[this]) << std::endl;
            }
            req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocking bin "<<binIndex<<std::endl;
            _binLock->writerUnlock(binIndex,req);
            req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocked bin "<<binIndex<<std::endl;
            req->trace(_name)<<"deleting req"<<std::endl;            
            delete req;
            
            ret = true;
        }
        else {
            req->trace(_name)<<"not originating cache"<<std::endl;
            bool found = false;

            DPRINTF("beg wb blk: %u out: %u\n", index, _outstanding.load());

            if (req->size <= _blockSize) {
                 req->trace(_type == CacheType::boundedGlobalFile)<<"wlocking bin "<<binIndex<<std::endl;
                _binLock->writerLock(binIndex,req);
                req->trace(_type == CacheType::boundedGlobalFile)<<"wlocked bin "<<binIndex<<std::endl;
                
                req->trace(_type == CacheType::boundedGlobalFile)<<"find oldest ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<std::endl;
                int blockIndex = oldestBlockIndex(index, fileIndex, found, req);
                req->trace(_type == CacheType::boundedGlobalFile)<<"find oldest result: bi: "<<blockIndex<<", found"<<found<<::getpid()<<std::endl;

                trackBlock(_name, "[BLOCK_WRITE]", fileIndex, index, 0);

                if (blockIndex >= 0) { //a slot for the block is present in the cache and has not been marked present
                    
                    req->trace(_type == CacheType::boundedGlobalFile)<<"slot avail ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                    
                    BlockEntry entry;
                    readBlockEntry(blockIndex, &entry);
                    
                    if (entry.status != BLK_WR || entry.status != BLK_AVAIL || !found) { //not found means we evicted some block
                        if(!found){
                            req->trace(_type == CacheType::boundedGlobalFile)<<"evicted ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                            req->trace(_type == CacheType::boundedGlobalFile)<<"is this the problem? blockIndex: "<<blockIndex<<" entry"<<std::endl;
                            req->trace(_type == CacheType::boundedGlobalFile)<<blockEntryStr(blockIndex);
                        }
                        
                        req->trace(_type == CacheType::boundedGlobalFile)<<"update entry to writing ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                        blockSet(blockIndex, fileIndex, index, BLK_WR, req->originating->type(), entry.prefetched);
                        
                        // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocking bin "<<binIndex<<std::endl;
                        // _binLock->writerUnlock(binIndex,req);
                        // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocked bin "<<binIndex<<std::endl;
                        
                        req->trace(_type == CacheType::boundedGlobalFile)<<"writing data ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                        setBlockData(req->data, blockIndex, req->size);

                        // req->trace(_type == CacheType::boundedGlobalFile)<<"wlocking bin "<<binIndex<<std::endl;                        
                        // _binLock->writerLock(binIndex,req);
                        // req->trace(_type == CacheType::boundedGlobalFile)<<"wlockedbin "<<binIndex<<std::endl;

                        req->trace(_type == CacheType::boundedGlobalFile)<<"update entry to avail ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                        blockSet(blockIndex, fileIndex, index, BLK_AVAIL, req->originating->type(), entry.prefetched); //write the name of the originating cache so we can properly attribute stall time...
                        
                        // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocking bin "<<binIndex<<std::endl;
                        // _binLock->writerUnlock(binIndex,req);
                        // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocked bin "<<binIndex<<std::endl;

                        if(!found){
                             req->trace(_type == CacheType::boundedGlobalFile)<<"new entry "<<std::endl;
                             req->trace(_type == CacheType::boundedGlobalFile)<<blockEntryStr(blockIndex);                             
                        }
                    }
                    else if (entry.status == BLK_AVAIL) {//is this possible?
                        req->trace(_type == CacheType::boundedGlobalFile)<<"update time ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                        blockSet(blockIndex, fileIndex, index, BLK_AVAIL, req->originating->type(), entry.prefetched); //update timestamp

                        // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocking bin "<<binIndex<<std::endl;
                        // _binLock->writerUnlock(binIndex,req);
                        // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocked bin "<<binIndex<<std::endl;
                    }
                    else { //the writer will update the timestamp
                        req->trace(_type == CacheType::boundedGlobalFile)<<"other will update ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                        // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocking bin "<<binIndex<<std::endl;                        
                        // _binLock->writerUnlock(binIndex,req);
                        // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocked bin "<<binIndex<<std::endl;
                    }
                    if (found) {
                        req->trace(_type == CacheType::boundedGlobalFile)<<"found entry("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                        
                        if (req->reservedMap[this] > 0) {
                            req->trace(_type == CacheType::boundedGlobalFile)<<"decrememt ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                            auto t_cnt = decBlkCnt(blockIndex, req);
                            // auto t_cnt = _blkIndex[blockIndex].activeCnt.fetch_sub(1);
                            if (t_cnt == 0) {
                                log(this) << _name << " underflow in write activecnt (" << t_cnt - 1 << ") for blkIndex: " << blockIndex << " fileIndex: " << fileIndex << " index: " << index << std::endl;
                            }
                        }

                        // debug() << "[TAZER]" << _name << " write: blkIndex: " << blockIndex << " fi: " << fileIndex << " i:" << index << " cnt: " << t_cnt - 1 << std::endl;
                    }
                    // else { //this would happen if there wasnt a cache entry avail when we first requested, but there is now
                    //     //log(this) /*debug()*/ << "[TAZER]" << _name << " possible? blkIndex : " << blockIndex << " fi: " << fileIndex << " i:" << index << " cnt : " << _blkIndex[blockIndex].activeCnt.load() << std::endl;
                    // }
                    ret = true;
                }
                else { //either no space in cache or block is already present
                    req->trace(_type == CacheType::boundedGlobalFile)<<"no new space, looking for block ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                    blockIndex = getBlockIndex(index, fileIndex);
                    req->trace(_type == CacheType::boundedGlobalFile)<<"block index: "<<blockIndex<<std::endl;

                    // req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocking bin "<<binIndex<<std::endl;
                    // _binLock->writerUnlock(binIndex,req);
                    // req->trace(_type == CacheType::boundedGlobalFile)<<" wunlocked bin "<<binIndex<<std::endl;

                    if (blockIndex >= 0 && found) {
                        req->trace(_type == CacheType::boundedGlobalFile)<<" found old ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                        if (req->reservedMap[this] > 0) {
                            req->trace(_type == CacheType::boundedGlobalFile)<<" decrement "<<blockIndex<<" ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                            auto t_cnt = decBlkCnt(blockIndex, req);
                            // log(this) /*debug()*/ <<   _name << " nowrite: blkIndex: " << blockIndex << " fi: (" << fileIndex + 1 << "," << _blkIndex[blockIndex].fileIndex << ") i: (" << index + 1 << "," << _blkIndex[blockIndex].blockIndex << ") prev cnt: " << t_cnt << " cur cnt: " << _blkIndex[blockIndex].activeCnt.load() << std::endl;
                            if (t_cnt == 0) {
                                log(this) << _name << " underflow nowrite: blkIndex: " << blockIndex << " fi: (" << fileIndex + 1 << ","
                                          << ") i: (" << index + 1 << ","
                                          << ") prev cnt: " << t_cnt << std::endl;
                            }
                        }
                    }
                    else{
                        req->trace(_type == CacheType::boundedGlobalFile)<<"  no space, not found ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                    }

                    // else{} we probably didnt have space in the cache so no need to decrement the block
                }
                req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocking bin "<<binIndex<<std::endl;
                _binLock->writerUnlock(binIndex,req);
                req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocked bin "<<binIndex<<std::endl;
            }
            DPRINTF("end wb blk: %u out: %u\n", index, _outstanding.load());
            if (_nextLevel) {
                ret &= _nextLevel->writeBlock(req);
            }
            else{
                req->trace(_type == CacheType::boundedGlobalFile)<<"not sure how I got here"<<std::endl;
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
            ret &= _nextLevel->writeBlock(req);
        }
        else{
            req->trace(_type == CacheType::boundedGlobalFile)<<"not sure how I got here"<<std::endl;
            delete req;
        }
    }
    return ret;
}

template <class Lock>
void BoundedCache<Lock>::readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority) {
    stats.start(); //read
    stats.start(); //ovh
    bool prefetch = priority != 0;
    req->trace(_name)<<_name<<" READ BLOCK"<<std::endl;
    if (_type == CacheType::boundedGlobalFile){
        req->printTrace=false;
    }
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
        req->trace(_type == CacheType::boundedGlobalFile)<<"wlocking bin "<<binIndex<<std::endl;
        _binLock->writerLock(binIndex,req);
        req->trace(_type == CacheType::boundedGlobalFile)<<"wlocked bin "<<binIndex<<std::endl;

        BlockEntry entry;
        req->trace(_type == CacheType::boundedGlobalFile)<<"check if present ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<std::endl;
        int blockIndex = getBlockIndex(index, fileIndex, &entry);
        req->trace(_type == CacheType::boundedGlobalFile)<<"entry: "<<blockIndex<<std::endl;
        if (blockIndex >= 0) { //block is present in cache HIT
            req->trace(_type == CacheType::boundedGlobalFile)<<"hit ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<std::endl;
            req->trace(_type == CacheType::boundedGlobalFile)<<"increment "<<blockIndex<<std::endl;
            incBlkCnt(blockIndex, req);
            // log(this) << " " << _name << " read hit: blkIndex: " << blockIndex << " fi: " << fileIndex << " i:" << index << " prev cnt: " << t_cnt << std::endl;
            trackBlock(_name, "[BLOCK_READ_HIT]", fileIndex, index, priority);

            if (entry.prefetched > 0) {
                stats.addAmt(prefetch, CacheStats::Metric::prefetches, 1);
                entry.prefetched = prefetch;
            }
            else {
                stats.addAmt(prefetch, CacheStats::Metric::hits, req->size);
            }
            req->trace(_type == CacheType::boundedGlobalFile)<<"update access time"<<std::endl;
            blockSet(blockIndex, fileIndex, index, BLK_AVAIL, _type, entry.prefetched); // add an additional param for updating the active count...
            stats.end(prefetch, CacheStats::Metric::ovh);
            stats.start(); // hits
            buff = getBlockData(blockIndex);
            stats.end(prefetch, CacheStats::Metric::hits);
            stats.start(); // ovh
            req->data = buff;
            req->originating = this;
            req->reservedMap[this] = 1;
            req->ready = true;
            req->time = Timer::getCurrentTime() - req->time;
            readBlockEntry(blockIndex,&entry);
            req->indexMap[this]=blockIndex;
            req->blkIndexMap[this]=entry.blockIndex;
            req->fileIndexMap[this]=entry.fileIndex;
            req->statusMap[this]=entry.status;
            updateRequestTime(req->time);
            req->trace(_type == CacheType::boundedGlobalFile)<<"done in hit ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
            req->trace(_type == CacheType::boundedGlobalFile)<<"wlocking bin "<<binIndex<<std::endl;
            _binLock->writerUnlock(binIndex,req);
            req->trace(_type == CacheType::boundedGlobalFile)<<"wlocked bin "<<binIndex<<std::endl;  
        }    

        if (!buff) { // data not currently present //miss
            req->trace(_type == CacheType::boundedGlobalFile)<<"miss ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<std::endl;
            trackBlock(_name, "[BLOCK_READ_MISS_CLIENT]", fileIndex, index, priority);

            stats.addAmt(prefetch, CacheStats::Metric::misses, 1);
            bool found = false;
            // req->trace(_type == CacheType::boundedGlobalFile)<<"wlocking bin "<<binIndex<<std::endl;
            // _binLock->writerLock(binIndex,req);
            // req->trace(_type == CacheType::boundedGlobalFile)<<"wlocked bin "<<binIndex<<std::endl;

            
            req->trace(_type == CacheType::boundedGlobalFile)<<"trying reserve ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<std::endl;
            bool reserved = blockReserve(index, fileIndex, found, blockIndex, req, prefetch);
            req->trace(_type == CacheType::boundedGlobalFile)<<"reserve results: "<<reserved<<" found: "<<found<<" bi: "<<blockIndex<<std::endl;
            if (blockIndex == -1 && found) {
                req->trace(_type == CacheType::boundedGlobalFile)<<"found, getting index ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<std::endl;
                blockIndex = getBlockIndex(index, fileIndex);
                req->trace(_type == CacheType::boundedGlobalFile)<<"found ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                
            }
            if (blockIndex >= 0) {
                req->trace(_type == CacheType::boundedGlobalFile)<<"entry avail ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                req->trace(_type == CacheType::boundedGlobalFile)<<"incrememt "<<blockIndex<< " ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi: "<<blockIndex<<std::endl;
                incBlkCnt(blockIndex, req);
                req->indexMap[this]=blockIndex;
                BlockEntry entry;
                readBlockEntry(blockIndex,&entry);
                req->blkIndexMap[this]=entry.blockIndex;
                req->fileIndexMap[this]=entry.fileIndex;
                req->statusMap[this]=entry.status;
                req->reservedMap[this] = 1; //we will need to decrement active count on the cache entry when we write back
            }
            else {
                req->trace(_type == CacheType::boundedGlobalFile)<<"no entry ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<std::endl;
                req->reservedMap[this] = 0;
            }

            req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocking bin "<<binIndex<<std::endl;
            _binLock->writerUnlock(binIndex,req);
            req->trace(_type == CacheType::boundedGlobalFile)<<"wunlocked bin "<<binIndex<<std::endl;
            

            if (reserved) { // we reserved a space, now we are responsible for getting the block from a higher level...
                req->time = Timer::getCurrentTime() - req->time;
                req->trace(_type == CacheType::boundedGlobalFile)<<"reserved go to next level ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<std::endl;
                updateRequestTime(req->time);
                stats.end(prefetch, CacheStats::Metric::ovh);
                stats.start(); //miss
                _nextLevel->readBlock(req, reads, priority);
                stats.end(prefetch, CacheStats::Metric::misses);
                stats.start(); //ovh
            }
            else {            //someone else has reserved or we did not have space in the cache
                if (!found) { //didnt have space at this level so try at the next;
                    req->time = Timer::getCurrentTime() - req->time;
                    req->trace(_type == CacheType::boundedGlobalFile)<<"no space got to next level ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<std::endl;
                    updateRequestTime(req->time);
                    stats.end(prefetch, CacheStats::Metric::ovh);
                    stats.start(); //miss
                    _nextLevel->readBlock(req, reads, priority);
                    stats.end(prefetch, CacheStats::Metric::misses);
                    stats.start(); //ovh
                }
                else { // some else has reserved the block (meaning they are responsible for writing to the cache), we can wait for it to showup
                    req->trace(_type == CacheType::boundedGlobalFile)<<"wait for someone else to fetch block ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi:"<<blockIndex<<std::endl;
                    auto fut = std::async(std::launch::deferred, [this, req, blockIndex, prefetch] {
                        // log(this) << _name << " read wait: blkIndex: " << blockIndex << " fi: " << fileIndex << " i:" << index << std::endl;
                        uint64_t stime = Timer::getCurrentTime();
                        // char waitingCacheName[MAX_CACHE_NAME_LEN];
                        // waitingCacheName[0] = '\0';
                        CacheType waitingCacheType = CacheType::empty;
                        uint64_t cnt = 0;
                        bool avail = false;
                        uint8_t *buff = NULL;
                        double curTime = (Timer::getCurrentTime() - stime) / 1000000000.0;
                        while (!avail && curTime < std::min(_lastLevel->getRequestTime() * 10,60.0) ) {                      // exit loop if request is 10x times longer than average network request or longer than 1 minute
                            avail = blockAvailable(blockIndex, req->fileIndex, true, cnt, &waitingCacheType); //maybe pass in a char* to capture the name of the originating cache?
                            sched_yield();
                            cnt++;
                            curTime = (Timer::getCurrentTime() - stime) / 1000000000.0;
                        }
                        if (avail) {
                            req->trace(_type == CacheType::boundedGlobalFile)<<"received, get data ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi:"<<blockIndex<<std::endl;
                            buff = getBlockData(blockIndex);
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
                            req->trace(_type == CacheType::boundedGlobalFile)<<" retrying ("<<req->blkIndex<<","<<req->fileIndex<<")"<<::getpid()<<" bi:"<<blockIndex<<std::endl;
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
                            req->trace(_type == CacheType::boundedGlobalFile)<<"recieved"<<std::endl;
                            log(this) <<"[TAZER] got block "<<req->blkIndex<<" after retrying! from: "<<req->originating->name()<<" waiting on cache type: "<<cacheTypeName(waitingCacheType)<<" "<<req->retryTime<<std::endl;
                                

                        }

                        req->waitingCache = waitingCacheType;

                        std::promise<Request *> prom;
                        auto inner_fut = prom.get_future();
                        prom.set_value(req);
                        // log(this) << _name << " done read wait: blkIndex: " << blockIndex << " fi: " << fileIndex << " i:" << index << std::endl;
                        return inner_fut.share();
                    });
                    reads[index] = fut.share();
                }
            }
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
// template <class Lock>
// void BoundedCache<Lock>::cleanReservation() {
// }

template <class Lock>
void BoundedCache<Lock>::addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize) {
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

// template class BoundedCache<ReaderWriterLock>;
template class BoundedCache<MultiReaderWriterLock>;
template class BoundedCache<FcntlBoundedReaderWriterLock>;

template <class Lock>
void BoundedCache<Lock>::trackBlock(std::string cacheName, std::string action, uint32_t fileIndex, uint32_t blockIndex, uint64_t priority) {
    if (Config::TrackBlockStats) {
        auto fut = std::async(std::launch::async, [cacheName, action, fileIndex, blockIndex, priority] {
            unixopen_t unixopen = (unixopen_t)dlsym(RTLD_NEXT, "open");
            unixclose_t unixclose = (unixclose_t)dlsym(RTLD_NEXT, "close");
            unixwrite_t unixwrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");

            int fd = (*unixopen)("block_stats.txt", O_WRONLY | O_APPEND | O_CREAT, 0660);
            if (fd != -1) {
                std::stringstream ss;
                ss << cacheName << " " << action << " " << fileIndex << " " << blockIndex << " " << priority << std::endl;
                unixwrite(fd, ss.str().c_str(), ss.str().length());
                unixclose(fd);
            }
        });
    }
}
