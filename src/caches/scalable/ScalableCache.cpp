//TODO
#include "ScalableCache.h"
#include <signal.h>
#include "xxhash.h"
#include <fcntl.h>
#include <future>

#define DPRINTF(...)


template <class Lock>
ScalableCache<Lock>::ScalableCache(std::string cacheName, CacheType type, uint64_t blockSize, ScalableRegistry *registry) : Cache(cacheName, type), 
                                                                                                                    _blockSize(blockSize), 
                                                                                                                    _registry(registry) {
    // required initializations
    stats.start();
    _maxNumBlocks = _registry->registerCache(this);
    log(this) << _name << " " << _blockSize << " " << _maxNumBlocks << std::endl;
    _localLock = new ReaderWriterLock();
    stats.end(false, CacheStats::Metric::constructor);
}

template <class Lock>
ScalableCache<Lock>::~ScalableCache() {
    //deconstructor here
    //call regisrty to be a victim?
    _terminating = true;
    log(this) << _name << " cache collisions: " << _collisions.load() << " prefetch collisions: " << _prefetchCollisions.load() << std::endl;
    log(this) << "deleting " << _name << " in ScalableCache" << std::endl;
    delete _localLock;
}

template <class Lock>
bool ScalableCache<Lock>::writeBlock(Request *req){

    if (_type == CacheType::boundedGlobalFile){
        req->printTrace=true;
        req->globalTrigger=true;
    }
    else{
        req->printTrace=false; 
        req->globalTrigger=false;
    }
    req->trace()<<_name<<" WRITE BLOCK"<<std::endl;

    bool ret = false;
    if (req->reservedMap[this] > 0 || !_terminating) { //when terminating dont waste time trying to write orphan requests
        if (req->originating == this) {
            req->trace()<<"originating cache"<<std::endl;
//            _binLock->writerLock(binIndex,req); //LOCK ME
            BlockEntry* entry = getBlock(req->blkIndex, req->fileIndex, req);
            if (entry) {
                if (req->reservedMap[this] > 0) {
                    req->trace()<<"decrement "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    auto t_cnt = decBlkCnt(entry, req);
                    req->trace()<<" dec cnt "<<t_cnt<<std::endl;
                    if (t_cnt == 0) {
                        debug()<<req->str()<<std::endl;
                        log(this) << _name << " underflow in orig activecnt (" << t_cnt - 1 << ") for blkIndex: " <<  entry->blockIndex << " fileIndex: " << entry->fileIndex << " index: " << entry->blockIndex << std::endl;
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
            //_binLock->writerUnlock(binIndex,req); //UNLOCK ME
            cleanUpBlockData(req->data);
            req->trace()<<"deleting req"<<std::endl;  
            req->printTrace=false;           
            delete req;
            
            ret = true;
        }
        else {
            req->trace()<<"not originating cache"<<std::endl;

//            DPRINTF("beg wb blk: %u out: %u\n", index, _outstanding.load());
            DPRINTF("beg wb blk: %u out: %u\n", index,0);

            if (req->size <= _blockSize) {
        //        _binLock->writerLock(binIndex,req); //LOCK ME
                BlockEntry* entry = oldestBlock(req->blkIndex, req->fileIndex, req);

                trackBlock(_name, "[BLOCK_WRITE]", req->fileIndex, req->blkIndex, 0);

                if (entry){ //a slot for the block is present in the cache       
                    if ((entry->status != BLK_WR && entry->status != BLK_AVAIL) || entry->status == BLK_EVICT) { //not found means we evicted some block
                        if(entry->status == BLK_EVICT && req->reservedMap[this] ){
                            req->trace()<<"evicted "<<blockEntryStr(entry)<<std::endl;
                            req->trace()<<"is this the problem?  blockIndex: "<< entry->id<<std::endl;
                            //BURCU WHAT IS THIS PART?
                            // auto binBlocks = readBin(binIndex);
                            // for( auto blk : binBlocks){
                            //     req->trace()<<blockEntryStr(blk)<<std::endl;
                            // }
                        }
                        //we done need this step since the bin is locked, no one else will access the block until data is written...
                        // req->trace()<<"update entry to writing "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                        // blockSet( entry, fileIndex, index, BLK_WR, req->originating->type(), entry->prefetched,0, req); //dont decrement...
                                                
                        req->trace()<<"writing data "<<blockEntryStr(entry)<<std::endl;
                        setBlockData(req->data,  entry->id, req->size);

                        req->trace()<<"update entry to avail "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                        blockSet( entry, req->fileIndex, req->blkIndex, BLK_AVAIL, req->originating->type(), entry->prefetched, req->reservedMap[this], req); //write the name of the originating cache so we can properly attribute stall time...
                        if(entry->status == BLK_EVICT){
                             req->trace()<<"new entry/evict "<<" "<<(void*)entry<<std::endl;
                             req->trace()<<blockEntryStr( entry)<<" "<<(void*)entry<<std::endl;                             
                        }
                    }
                    else if (entry->status == BLK_AVAIL) {
                        req->trace()<<"update time "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                        blockSet( entry, req->fileIndex, req->blkIndex, BLK_AVAIL, req->originating->type(), entry->prefetched,req->reservedMap[this], req); //update timestamp
                    }
                    else { //the writer will update the timestamp
                        req->trace()<<"other will update"<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                    }
                    ret = true;
                }
//                _binLock->writerUnlock(binIndex,req); // UNLOCK ME
            }
//            DPRINTF("end wb blk: %u out: %u\n", index, _outstanding.load());
            DPRINTF("end wb blk: %u out: %u\n", req->blkIndex, 0);
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
void ScalableCache<Lock>::readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority){
    stats.start(); //read
    stats.start(); //ovh
    bool prefetch = priority != 0;
    if (_type == CacheType::boundedGlobalFile){
        req->printTrace=true;
        req->globalTrigger=true;
    }
    else{
        req->printTrace=false; 
        req->globalTrigger=false;
    }
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
    auto index = req->blkIndex;
    auto fileIndex = req->fileIndex;

    // find if we have the block

    //WE NEED LOCKS
    //PREFETCH,RES, etc isn't handled right now
     _localLock->readerLock(); //local lock
    int tsize = _fileMap[fileIndex].blockSize;
    _localLock->readerUnlock();
    if (!req->size) {
        req->size = tsize;
    }

    if (req->size <= _blockSize) {
        //_binLock->writerLock(binIndex,req); //LOCK ME
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
                buff = entry->blkAddr;
                req->originating = this;
                req->ready = true;
                req->time = Timer::getCurrentTime() - req->time;
                req->indexMap[this]= entry->id;
                req->blkIndexMap[this]=entry->blockIndex;
                req->fileIndexMap[this]=entry->fileIndex;
                req->statusMap[this]=entry->status;
                updateRequestTime(req->time);
                req->trace()<<"done in hit "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
               // _binLock->writerUnlock(binIndex,req); //UNLOCK ME
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
                    //_binLock->writerUnlock(binIndex,req);   //UNLOCK ME          
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
                    auto  blockId= entry->id;
                    //_binLock->writerUnlock(binIndex,req); //UNLOCK ME  
                    auto fut = std::async(std::launch::deferred, [this, req,  blockId, prefetch, entry] { 
                        uint64_t stime = Timer::getCurrentTime();
                        CacheType waitingCacheType = CacheType::empty;
                        uint64_t cnt = 0;
                        bool avail = false;
                        uint8_t *buff = NULL;
                        double curTime = (Timer::getCurrentTime() - stime) / 1000000000.0;
                        while (!avail && curTime < std::min(_lastLevel->getRequestTime() * 10,60.0) ) {                      // exit loop if request is 10x times longer than average network request or longer than 1 minute
                            avail = blockAvailable( blockId, req->fileIndex, true, cnt, &waitingCacheType); //maybe pass in a char* to capture the name of the originating cache?
                            sched_yield();
                            cnt++;
                            curTime = (Timer::getCurrentTime() - stime) / 1000000000.0;
                        }
                        if (avail) {
                            BlockEntry* entry2 = getBlockEntry(blockId, req);
                            req->trace()<<"received, entry: "<<blockEntryStr(entry)<<" "<<(void*)entry<<std::endl;
                            req->trace()<<"received, get data, (entry2):"<<blockEntryStr(entry2)<<" "<<(void*)entry2<<std::endl;
                            buff = entry2->blkAddr;
                            //buff = getBlockData(blockIndex);
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
                            req->trace()<<" retrying ("<<req->blkIndex<<","<<req->fileIndex<<")"<<" bi:"<< blockId<<std::endl;
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
           // _binLock->writerUnlock(binIndex,req); //UNLOCK ME
        
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




//     BlockEntry* blockRef = getBlock(req->blkIndex, req->fileIndex, req); 

//     if( blockRef == NULL) {
//         req->time = Timer::getCurrentTime() - req->time;
//         req->trace()<<"no space got to next level ("<<req->blkIndex<<","<<req->fileIndex<<")"<<std::endl;
//         updateRequestTime(req->time);
//         stats.end(prefetch, CacheStats::Metric::ovh);
//         stats.start(); //miss
//         _nextLevel->readBlock(req, reads, priority);
//         stats.end(prefetch, CacheStats::Metric::misses);
//         stats.start(); //ovh
//         //we don't have the block
//     }
//     else {
//         uint8_t *buff = nullptr;
//         buff = blockRef->blkAddr;
//         req->data = buff;
//         req->originating = this;
//         req->ready = true;
//     }
//     // if(blockRef < 0)
//     // ask next level
//     // else 
//     // update request with blockData
// }

template <class Lock>
void ScalableCache<Lock>::addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize){
    //trackBlock(_name, "[ADD_FILE]", index, blockSize, fileSize);
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
typename ScalableCache<Lock>::BlockEntry* ScalableCache<Lock>::getBlock(uint32_t index, uint32_t fileIndex, Request* req){
    //req->trace()<<"searching for block: "<<index<<" "<<fileIndex<<std::endl;
    std::string hashstr(std::to_string(fileIndex) + std::to_string(index)); 
    uint64_t key = (uint64_t)XXH32(hashstr.c_str(), hashstr.size(), 0);
    if(_blkMap.count(key) > 0 ) {
        if(_blkMap[key]->status == BLK_AVAIL){
            //req->trace()<<"found block: "<<blockEntryStr(blkEntries[i])<<std::endl;
            return (_blkMap[key]);
        }
    }
    //    req->trace()<<"block not found "<<std::endl;
    return NULL;
}


template <class Lock>
void ScalableCache<Lock>::cleanReservation() {
    // todo 
}

template <class Lock>
typename ScalableCache<Lock>::BlockEntry* ScalableCache<Lock>::oldestBlock(uint32_t index, uint32_t fileIndex, Request* req) {

    //check if block exists 
    std::string hashstr(std::to_string(fileIndex) + std::to_string(index)); 
    uint64_t key = (uint64_t)XXH32(hashstr.c_str(), hashstr.size(), 0);
    if(_blkMap.count(key) > 0 ) {
        req->trace()<<"found entry: "<<blockEntryStr(_blkMap[key])<<std::endl;
        return _blkMap[key];
    }
    else{
        if(_curNumBlocks < _maxNumBlocks){
            uint8_t* addr = _registry->allocateBlock(this);//ask for a block from registry
            if( addr == NULL){} //registry didn't send a block -- go to else (look for LRU)
            else {
                BlockEntry* newBlk = new BlockEntry();
                newBlk->init(this, key);
                newBlk->blkAddr = addr;
                _blkMap.emplace(key, newBlk);
                _curNumBlocks++;
                return newBlk;
            }
        }
        else{ //max number of blocks is reached, try to use one of the blocks we have 
            BlockEntry* blk = NULL;
            uint32_t minTime = -1; //this is max uint32_t
            BlockEntry *minEntry = NULL;
            uint32_t minPrefetchTime = -1; //Prefetched block
            BlockEntry *minPrefetchEntry = NULL;
            
            for( auto entry : _blkMap) {
                if(entry.second->status == BLK_AVAIL){
                    if (!anyUsers(entry.second,req)) { 
                        if (entry.second->timeStamp < minTime) {
                            minTime = entry.second->timeStamp;
                            minEntry = entry.second;
                        }
                        if (Config::prefetchEvict && entry.second->prefetched && entry.second->timeStamp < minPrefetchTime) {
                            minPrefetchTime = entry.second->timeStamp;
                            minPrefetchEntry = entry.second;
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
                return minPrefetchEntry;
            }
            if (minTime != (uint32_t)-1 && minEntry) { //Did we find a space
                _collisions++;
                trackBlock(_name, "[BLOCK_EVICTED]", fileIndex, index, 0);
                minEntry->status = BLK_EVICT;
                req->trace()<<"evicting  entry: "<<blockEntryStr(minEntry)<<std::endl;
                return minEntry;
            }
            log(this)<< _name << " All space is reserved..." << std::endl;
            req->trace()<<"no entries found"<<std::endl;
            return NULL;
        }
    }
}

template <class Lock>
void ScalableCache<Lock>::sendCloseSignal(uint32_t index) {
    _registry->fileClosed(this);
}

template <class Lock>
void ScalableCache<Lock>::sendOpenSignal(uint32_t index) {
    _registry->fileOpened(this);
}

template <class Lock>
uint8_t* ScalableCache<Lock>::sendBlock() {
    if(_curNumBlocks == 0) {
        return NULL;
    }
    else { // look for a block to evict
        for( auto entry : _blkMap) {
            if((entry.second)->status == BLK_AVAIL && (entry.second)->activeCnt.load() == 0 ) {
                BlockEntry* item = entry.second;
                _blkMap.erase(entry.first);
                return item->blkAddr;
            }
        }
        return NULL;
    }
}

template <class Lock>
void ScalableCache<Lock>::trackBlock(std::string cacheName, std::string action, uint32_t fileIndex, uint32_t  blockIndex, uint64_t priority) {
    if (Config::TrackBlockStats) {
        auto fut = std::async(std::launch::async, [cacheName, action, fileIndex,  blockIndex, priority] {
            unixopen_t unixopen = (unixopen_t)dlsym(RTLD_NEXT, "open");
            unixclose_t unixclose = (unixclose_t)dlsym(RTLD_NEXT, "close");
            unixwrite_t unixwrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");

            int fd = (*unixopen)("block_stats.txt", O_WRONLY | O_APPEND | O_CREAT, 0660);
            if (fd != -1) {
                std::stringstream ss;
                ss << cacheName << " " << action << " " << fileIndex << " " <<  blockIndex << " " << priority << std::endl;
                unixwrite(fd, ss.str().c_str(), ss.str().length());
                unixclose(fd);
            }
        });
    }
}

// template class NewBoundedCache<ReaderWriterLock>;
template class ScalableCache<MultiReaderWriterLock>;
//template class ScalableCache<FcntlBoundedReaderWriterLock>;
//template class ScalableCache<FileLinkReaderWriterLock>;
