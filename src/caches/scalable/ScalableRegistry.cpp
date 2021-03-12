#include "ScalableRegistry.h"
#include <unistd.h>
#include <memory>
#include <random>
#include <algorithm>
#include <string.h>
#include "ScalableMemoryCache.h"


ScalableRegistry::ScalableRegistry(uint64_t maxCacheSize, uint64_t blockSize) {

    _maxCacheSize = maxCacheSize;
    _blockSize = blockSize;
    _maxBlocks = _maxCacheSize/_blockSize;
    _allocatedBlocks = 0;
    _fileBlockLimit = 10; //for now a constant predetermined limit will be set, future work: decide on the pattern and adjust the block limit per file
}

ScalableRegistry* ScalableRegistry::addNewScalableRegistry(uint64_t maxCacheSize, uint64_t blockSize) {
    ScalableRegistry* res = new ScalableRegistry(maxCacheSize, blockSize);
    return res;
}


//registers a new cache object to the registry, returns the max allowed number of blocks for that cache
// TODO: we can create a cache struct, 
// - ptr to cache
// - cache status (active, victim, empty)
// - max block size
// - cur block size
// -list of blocks?
uint32_t ScalableRegistry::registerCache(Cache* cache) {
    _cacheMap[cache] = {};
    _cachesInUse.push_back(cache);
    return _fileBlockLimit; // returns the max number of cache blocks for that cache ( this should be changeable per-cache (per-file) )
}

//availabe should be in lock
//we won't have available any more, instead we will have a victim list to steal from (Call stealblock with input)
uint8_t* ScalableRegistry::allocateBlock(Cache* cache) {
    
    //change this to allocated blocks of the cache, not overall
    if (_allocatedBlocks < _maxCacheSize ) {
        uint8_t * tmpBlock = new uint8_t[_blockSize];
        memset(tmpBlock, 0, _blockSize);
        _cacheMap[cache].push_back(tmpBlock);
        _allocatedBlocks++;

        return tmpBlock;
    }
    else {
        uint8_t * tmpBlock = stealBlock(3);
        if (tmpBlock) {
            _cacheMap[cache].push_back(tmpBlock); 
        }
        return tmpBlock;
    }
}


uint8_t * ScalableRegistry::stealBlock(int n=3) {
    
    // uint8_t * tmpBlock = new uint8_t[_blockSize];
    // return tmpBlock;
    Cache* victimCache;
    bool volunteer = false;
    //select a victim cache
    if (_victims.size() > 0 ) {
        victimCache = _victims.at(std::rand() % _victims.size());
        volunteer = true;
    }
    else {
        victimCache = _cachesInUse.at(std::rand() % _cachesInUse.size());
    }
    //ask a block from victim
    uint8_t* block = ((ScalableMemoryCache*)victimCache)->sendBlock();
    
    //check if a block is sent back
    if (block == NULL) { 
        n = n-1;
        if (n==0) { //if we got null n times, return null
            return NULL;
        }
        else { //else try again to steal
            return stealBlock(n);
        }
    }   
    else { //remove block from victim's list 
        _cacheMap[victimCache].erase(std::find(_cacheMap[victimCache].begin(), _cacheMap[victimCache].end(), block));
        if (_cacheMap[victimCache].size() == 0) {
            if(volunteer){
                _victims.erase(std::find(_victims.begin(), _victims.end(), victimCache));
            }
            else {
                _cachesInUse.erase(std::find(_cachesInUse.begin(), _cachesInUse.end(), victimCache));
            }
        } 
        return block;
    }
}

void ScalableRegistry::fileOpened (Cache* cache) {
    std::cerr << "registry open" << std::endl;
    // change status of the file
    // add file to caches in use
    // remove it from victims list
    auto position = std::find(_victims.begin(), _victims.end(),cache);
    if (position != _victims.end()) {
        _victims.erase(position);
        std::cerr << "removed file from victims" << std::endl;
    }
    _cachesInUse.push_back(cache);
    std::cerr << "added file to actives" << std::endl;

    std::cerr << "registry open --- exit" << std::endl;
}

void ScalableRegistry::fileClosed (Cache* cache) {
    std::cerr << "registry close" << std::endl;
    
    _victims.push_back(cache);
    std::cerr << "added file to victims" << std::endl;
    auto position = std::find(_cachesInUse.begin(), _cachesInUse.end(), cache);
    if (position != _cachesInUse.end()) {
        _cachesInUse.erase(position);
        std::cerr << "removed file from active caches" << std::endl;
    }
    else{
        std::cerr <<" [BURCU] why don't we have that cache in actives?" << std::endl;
    }
    // change status of the file
    // remove from active list
    // add to victims list 
    std::cerr << "registry close -- exit" << std::endl;
}
