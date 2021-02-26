#include "ScalableMemoryCache.h"
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "Message.h"
#include "ReaderWriterLock.h"
#include "Timer.h"
#include "lz4.h"
#include "xxhash.h"

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


ScalableMemoryCache::ScalableMemoryCache(std::string cacheName, CacheType type, uint64_t blockSize, ScalableRegistry *registry) : ScalableCache(cacheName, type, blockSize, registry) {
    std::cout << "[TAZER] "
              << "Constructing " << _name << " in scalable memory cache" << std::endl;
    stats.start();
    //init lock(s) ??
    
    stats.end(false, CacheStats::Metric::constructor);
}

ScalableMemoryCache::~ScalableMemoryCache(){
    
    stats.start();
    uint32_t numEmpty = 0;
    // for (uint32_t i = 0; i < _curNumBlocks; i++) {
    for( auto i : _blkMap) {
        BlockEntry* item = i.second;
        if (item->activeCnt > 0) { // this typically means we reserved a block via prefetching but never honored the reservation, so try to prefetch again... this should probably be read?
            std::cout
//                << "[TAZER] " << _name << " " << item->id << " " << _numBlocks << " " << item->activeCnt << " " << item->fileIndex << " " << item->blockIndex << " prefetched: " << item->prefetched << std::endl;
                << "[TAZER] " << _name << " " << item->id << " " << item->activeCnt << " " << item->fileIndex << " " << item->blockIndex << " prefetched: " << item->prefetched << std::endl;

        }
        if(item->status == BLK_EMPTY){
            numEmpty+=1;
        }
    }
    std::cout<<_name<<" number of empty blocks: "<<numEmpty<<std::endl;

    stats.end(false, CacheStats::Metric::destructor);
    stats.print(_name);
    std::cout << std::endl;
}

Cache* ScalableMemoryCache::addScalableMemoryCache(std::string cacheName, CacheType type, uint64_t blockSize, ScalableRegistry *registry) {
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [&]() -> Cache * {
            Cache *temp = new ScalableMemoryCache(cacheName, type, blockSize, registry);
            return temp;
        });
}

//can we move these 3 functions to ScalableCache, now activecount is a part of scalable blockentry
int ScalableMemoryCache::incBlkCnt(BlockEntry * entry, Request* req) {
    return entry->activeCnt.fetch_add(1);
}

int ScalableMemoryCache::decBlkCnt(BlockEntry * entry, Request* req) {
    return entry->activeCnt.fetch_sub(1);
}

bool ScalableMemoryCache::anyUsers(BlockEntry * entry, Request* req) {
    return entry->activeCnt;
}

void ScalableMemoryCache::setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size) {
    //memcpy(&_blocks[blockIndex * _blockSize], data, size);
    //blockindex is the block id on our hashmap 
    //set the data using the blockentry data pointer
    if (_blkMap.count(blockIndex) > 0 ) {
        BlockEntry* temp = _blkMap[blockIndex];
        memcpy(temp->blkAddr, data, size);
    }
    else{
        //we don't have that block?? 
    }
}

ScalableMemoryCache::BlockEntry* ScalableMemoryCache::getBlockEntry(uint32_t entryId, Request* req){
    //return (BlockEntry*)&_blkIndex[blockIndex];
    //return pointer to the blockindex ID'd blockentry from hashmap of blocks 
    return (BlockEntry*)_blkMap[entryId]; 
}

//move to scalablecache with three activecount functions 
std::string ScalableMemoryCache::blockEntryStr(BlockEntry *entry){
    return entry->str() + " ac: "+ std::to_string((entry)->activeCnt);
}

//Must lock first!
//This uses the actual index (it does not do a search)
void ScalableMemoryCache::blockSet(BlockEntry* blk, uint32_t fileIndex, uint32_t blockIndex, uint8_t status, CacheType type, int32_t prefetched, int activeUpdate,Request* req) {
    //LOCK ME

    std::string hashstr(std::to_string(fileIndex) + std::to_string(blockIndex)); 
    uint64_t key = (uint64_t)XXH32(hashstr.c_str(), hashstr.size(), 0);
    
    //if key(ID) is the same, update rest, if key is different use extract to change the element of the unordered map
    //this part works with c++17 (https://en.cppreference.com/w/cpp/container/unordered_map/extract)
    /*
    if(blk->id != key){
        auto item = _blkMap.extract(blk->id);
        item->id = key;
        _blkMap.insert(move(item));
        blk = item;
    }
    */
    
    if(blk->id != key) {
        _blkMap.erase(blk->id);
        blk->id = key;
        _blkMap.insert({key, blk});
    }

    blk->fileIndex = fileIndex;
    blk->blockIndex = blockIndex;
    blk->timeStamp = Timer::getCurrentTime();
    if (prefetched >= 0) {
        blk->prefetched = prefetched;
    }
    blk->origCache.store(type);
    blk->status = status;
    if (activeUpdate >0){
        incBlkCnt(blk,NULL);
    }
    else if (activeUpdate < 0){
        decBlkCnt(blk,NULL);
    }
    //UNLOCK ME
}

bool ScalableMemoryCache::blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs, uint32_t cnt, CacheType *origCache) {
    //bool avail = _blkIndex[index].status == BLK_AVAIL;
    bool avail = ((_blkMap.count(index) > 0 ) && (_blkMap[index]->status == BLK_AVAIL));

    if (origCache && avail) {
        *origCache = CacheType::empty;
    //  *origCache = _blkIndex[index].origCache.load();
        *origCache = _blkMap[index]->origCache.load();
    //     // for better or for worse we allow unlocked access to check if a block avail, 
    //     // there is potential for a race here when some over thread updates the block (not changing the data, just the metadata)
         while(*origCache == CacheType::empty){ 
    //      *origCache = _blkIndex[index].origCache.load();
            *origCache = _blkMap[index]->origCache.load();
         }
         return true;
    }
    return avail;
}