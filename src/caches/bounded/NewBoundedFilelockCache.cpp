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

#include "NewBoundedFilelockCache.h"
#include "Config.h"

#include "FcntlReaderWriterLock.h"
#include "ReaderWriterLock.h"
#include "Timer.h"
#include "xxhash.h"
#include <ctime>
#include <experimental/filesystem>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)


//look into a link based locking mechanism... something where we have a lock file for each bin, forcing to be mutex, in the lock file we also mainiting the reader/writer block for each block, which should just be an added section in the _binFd entries...
// verify we only write to block while bin is locked, but we can have multiple readers to a blk, if so we can do the link trick on the blks as well using that as our counter
//TODO: create version that locks the file when reserved and only releases after it has written it...
NewBoundedFilelockCache::NewBoundedFilelockCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string cachePath) : NewBoundedCache(cacheName, type, cacheSize, blockSize, associativity),
                                                                                                                                                           _open((unixopen_t)dlsym(RTLD_NEXT, "open")),
                                                                                                                                                           _close((unixclose_t)dlsym(RTLD_NEXT, "close")),
                                                                                                                                                           _read((unixread_t)dlsym(RTLD_NEXT, "read")),
                                                                                                                                                           _write((unixwrite_t)dlsym(RTLD_NEXT, "write")),
                                                                                                                                                           _lseek((unixlseek_t)dlsym(RTLD_NEXT, "lseek")),
                                                                                                                                                           _fsync((unixfdatasync_t)dlsym(RTLD_NEXT, "fdatasync")),
                                                                                                                                                           _stat((unixxstat_t)dlsym(RTLD_NEXT, "stat")),
                                                                                                                                                           _cachePath(cachePath),
                                                                                                                                                           _myOutstandingWrites(0) {
    std::thread::id thread_id = std::this_thread::get_id();
    stats.checkThread(thread_id, true);
    stats.start(false, CacheStats::Metric::constructor, thread_id);
    std::error_code err;
    std::string shmPath("/" + Config::tazer_id + "_" + _name + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity));
    // int shmFd = shm_open(shmPath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
    // if(shmFd == -1){
    //     debug() <<_name<< " Reusing shared memory" << std::endl;
    //     int shmFd = shm_open(shmPath.c_str(), O_RDWR, 0644);
    //     if (shmFd != -1) {
    //         ftruncate(shmFd, sizeof(uint32_t) +  _numBlocks * sizeof(FileBlockEntry));
    //         void *ptr = mmap(NULL, sizeof(uint32_t) + _numBlocks * sizeof(FileBlockEntry), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    //         uint32_t *init = (uint32_t *)ptr;
    //         log(this) << "init: " << *init << std::endl;
    //         while (!*init) {
    //             sched_yield();
    //         }
    //         _cacheEntries = (FileBlockEntry*)(uint8_t *)init + sizeof(uint32_t);
    //         log(this) << "init: " << (uint32_t)*init << std::endl;
    //     }
    //     else {
    //         debug()<< "Error opening shared memory " << strerror(errno) << std::endl;
    //     }
    // }
    // else{
    //     debug()<< _name << " created shared memory" << std::endl;
    //     ftruncate(shmFd, sizeof(uint32_t)  + _numBlocks * sizeof(FileBlockEntry));
    //     void *ptr = mmap(NULL, sizeof(uint32_t)  + _numBlocks * sizeof(FileBlockEntry), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    //     uint32_t *init = (uint32_t *)ptr;
    //     log(this) << "init: " << *init << std::endl;
    //     *init = 0;
    //     log(this) << "init: " << *init << std::endl;
    //     _cacheEntries = (FileBlockEntry *)((uint8_t *)init + sizeof(uint32_t));
    //     for (uint32_t i=0;i<_numBlocks;i++){
    //         _cacheEntries[i].init(this,i);
    //     }
    //     *init = 1;
    //     log(this) << "init: " << *init << std::endl;
    // }
    _cacheEntries = new FileBlockEntry[_numBlocks];
    for (uint32_t i=0;i<_numBlocks;i++){
        _cacheEntries[i].init(this,i);
    }

    std::experimental::filesystem::create_directories(_cachePath, err);
    int ret = mkdir((_cachePath + "/init_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + "/").c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); //if -1 means another process has already reserved
    if (ret == 0) {
        //first create shared shadow cache...
        

        debug()<<"Creating bounded file lock cache"<<std::endl;
        int fd = (*_open)((_cachePath + "/lock_tmp").c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd == -1 ){
            debug()<<"[TAZER ERROR] "<<_name<<" lock creation open error "<<strerror(errno)<<std::endl;
            exit(1);
        }
        ret = ftruncate(fd, _numBlocks * sizeof(FileBlockEntry));
        if (ret == -1 ){
            debug()<<"[TAZER ERROR] "<<_name<<" lock creation ftruncate error "<<strerror(errno)<<std::endl;
        }
        uint8_t byte = 0;
        ret = (*_lseek)(fd, _numBlocks * sizeof(FileBlockEntry), SEEK_SET);
        if (ret == -1 ){
            if (errno == EBADF){
                while (errno == EBADF ){ //not sure why but sometimes this returns no file even though it was sucessfully created, maybe NFS related?
                    ret = (*_lseek)(fd, _numBlocks * sizeof(FileBlockEntry), SEEK_SET);
                }
            }
            else{
                debug()<<"[TAZER ERROR] "<<_name<<" lock creation seek error "<<strerror(errno)<<std::endl;
            }
            
        }
        ret = (*_write)(fd, &byte, sizeof(uint8_t));
        if (ret == -1 ){
            if (errno == EBADF){
                while (errno == EBADF ){ //not sure why but sometimes this returns no file even though it was sucessfully created, maybe NFS related?
                    ret = (*_write)(fd, &byte, sizeof(uint8_t));
                }
            }
            else{
                debug()<<"[TAZER ERROR] "<<_name<<" lock creation write error "<<strerror(errno)<<std::endl;
            }
        }
        pwriteToFile(fd,_numBlocks*sizeof(FileBlockEntry),(uint8_t*)_cacheEntries,0);
        ret = (*_fsync)(fd);
        if (ret == -1 ){
            debug()<<"[TAZER ERROR] "<<_name<<" lock creation fsync error "<<strerror(errno)<<std::endl;
        }
        ret = (*_close)(fd);
        if (ret == -1 ){
            debug()<<"[TAZER ERROR] "<<_name<<" lock creation close error "<<strerror(errno)<<std::endl;
        }
        int ret = rename((_cachePath + "/lock_tmp").c_str(), (_cachePath + "/lock" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity)).c_str());
        if (ret == -1 ) {
            debug() << _name << " ERROR: lock rename went wrong!!!" << strerror(errno) << std::endl;
            exit(1);
        }
        fd = (*_open)((_cachePath + "/data_tmp").c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
         if (fd < 0 ){
            debug()<<"[TAZER ERROR] "<<_name<<" data creation open error "<<strerror(errno)<<std::endl;
        }
        debug()<<"cache size: "<<_cacheSize<<" num blocks "<<_numBlocks<<" _blockSize "<<_blockSize <<" calc size "<<  _numBlocks * _blockSize * sizeof(uint8_t)<<std::endl;
        ret = ftruncate(fd, _numBlocks * _blockSize * sizeof(uint8_t));
        if (ret == -1 ){
            debug()<<"[TAZER ERROR] "<<_name<<" data creation ftruncate error "<<strerror(errno)<<std::endl;
        }
        ret = (*_lseek)(fd, _numBlocks * _blockSize * sizeof(uint8_t), SEEK_SET);
        if (ret == -1 ){
            debug()<<"[TAZER ERROR] "<<_name<<" data creation seek error "<<strerror(errno)<<std::endl;
        }
        ret = (*_write)(fd, &byte, sizeof(uint8_t));
        if (ret == -1 ){
            debug()<<"[TAZER ERROR] "<<_name<<" data creation write error "<<strerror(errno)<<std::endl;
        }
        ret = (*_fsync)(fd);
        if (ret == -1 ){
            debug()<<"[TAZER ERROR] "<<_name<<" data creation fsync error "<<strerror(errno)<<std::endl;
        }
        ret = (*_close)(fd);
        if (ret == -1 ){
            debug()<<"[TAZER ERROR] "<<_name<<" data creation close error "<<strerror(errno)<<std::endl;
        }
        ret = rename((_cachePath + "/data_tmp").c_str(), (_cachePath + "/data" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".gc").c_str());
        if (ret == -1 ) {
            debug() << _name << " ERROR: data rename went wrong!!!" << strerror(errno) << std::endl;
            exit(1);
        }
        // _shmLock->writerUnlock();
    }
    else {
        debug() << "reusing lock and data file" << std::endl;
        int cnt = 0;
        while (!std::experimental::filesystem::exists(_cachePath + "/lock" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity))) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (cnt++ > 100) {
                debug() << _name << " ERROR: waiting for lock to appear!!!" << strerror(errno) << _cachePath + "/data" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".gc" << std::endl;
                exit(1);
            }
        }
        cnt = 0;
        while (!std::experimental::filesystem::exists(_cachePath + "/data" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".gc")) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (cnt++ > 100) {
                debug() << _name << " ERROR: waiting for data to appear!!!" << strerror(errno) << _cachePath + "/data" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".gc" << std::endl;
                exit(1);
            }
        }
    }
    _lockPath = _cachePath + "/lock" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity);
    _cachePath = _cachePath + "/data" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".gc";

    _binLock = new FcntlBoundedReaderWriterLock(sizeof(FileBlockEntry) * _associativity, _numBins, _lockPath,"bin");

    // _blkLock = new FcntlBoundedReaderWriterLock(_blockSize, _numBlocks, _cachePath,"blk");

    _binFd = (*_open)(_lockPath.c_str(), O_RDWR| O_DIRECT| O_SYNC);
    _blkFd = (*_open)(_cachePath.c_str(), O_RDWR| O_DIRECT| O_SYNC);
    _shared = true;
    _pid = (uint32_t)::getpid();

    _writePool = new ThreadPool<std::function<void()>>(Config::numWriteBufferThreads, "bounded file lock cache write pool");
    _writePool->initiate();

    debug()<<"SHARED MEM ADDR: "<<(void*)&_cacheEntries[0]<<" "<<(void*)&_cacheEntries[_numBlocks]<<std::endl;


    // does implementing a lock around disk ops improve i/o performance?
    // std::string shmPath("/" + Config::tazer_id  + "_fcntlbnded_shm.lck");
    // int shmFd = shm_open(shmPath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
    // if (shmFd == -1) {
    //     shmFd = shm_open(shmPath.c_str(), O_RDWR, 0644);
    //     if (shmFd != -1) {
    //         debug() << "resusing fcntl shm lock" << std::endl;
    //         ftruncate(shmFd, sizeof(uint32_t) + sizeof(ReaderWriterLock));
    //         void *ptr = mmap(NULL, sizeof(uint32_t) + sizeof(ReaderWriterLock), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    //         uint32_t *init = (uint32_t *)ptr;
    //         while (!*init) {
    //             sched_yield();
    //         }
    //         _shmLock = (ReaderWriterLock *)((uint8_t *)init + sizeof(uint32_t));
    //     }
    //     else {
    //         std::cerr << "[TAZER]"
    //                   << "Error opening shared memory " << strerror(errno) << std::endl;
    //     }
    // }
    // else {
    //     debug() << "creating fcntl shm lock" << std::endl;
    //     ftruncate(shmFd, sizeof(uint32_t) + sizeof(ReaderWriterLock));
    //     void *ptr = mmap(NULL, sizeof(uint32_t) + sizeof(ReaderWriterLock), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);

    //     uint32_t *init = (uint32_t *)ptr;
    //     _shmLock = new ((uint8_t *)init + sizeof(uint32_t)) ReaderWriterLock();
    //     *init = 1;
    // }
    stats.end(false, CacheStats::Metric::constructor, thread_id);
}

NewBoundedFilelockCache::~NewBoundedFilelockCache() {
    _terminating = true;
    std::thread::id thread_id = std::this_thread::get_id();
    stats.checkThread(thread_id, true);
    stats.start(false, CacheStats::Metric::destructor, thread_id);
    while (_outstandingWrites.load()) {
        std::this_thread::yield();
    }
    while (_myOutstandingWrites.load()) {
        std::this_thread::yield();
    }

    debug() << _name << " cache collisions: " << _collisions << std::endl;
    (*close)(_binFd);
    (*close)(_blkFd);
    delete _binLock;
    delete[] _cacheEntries;
    // delete _blkLock;
    // std::string shmPath("/" + Config::tazer_id + "_fcntlbnded_shm.lck");
    // shm_unlink(shmPath.c_str());
    stats.end(false, CacheStats::Metric::destructor, thread_id);
    stats.print(_name);
    debug() << std::endl;
}



void NewBoundedFilelockCache::writeToFile(int fd, uint64_t size, uint8_t *buff) {
    uint8_t *local = buff;
    while (size) {
        int bytes = (*_write)(fd, local, size);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            debug() << "[TAZER ERROR] Failed a write " << fd << " " << size << std::endl;
        }
    }
}

void NewBoundedFilelockCache::readFromFile(int fd, uint64_t size, uint8_t *buff) {
    uint8_t *local = buff;
    while (size) {
        // debug() << "reading " << size << " more" << std::endl;
        int bytes = (*_read)(fd, local, size);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            debug() << "[TAZER ERROR] Failed a read " << fd << " " << size << std::endl;
        }
    }
}

void NewBoundedFilelockCache::pwriteToFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset) {
    uint8_t *local = buff;
    // auto origSize = size;
    // auto start = Timer::getCurrentTime();
    while (size) {
        // int bytes = (*_write)(fd, local, size);
        // debug() << "writing " << size << " from " << offset << std::endl;
        int bytes = pwrite(fd, local, size, offset);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
            offset += bytes;
        }
        else {
            debug() << "[TAZER ERROR] Failed a write " << fd <<" ("<<_binFd<<","<<_blkFd<<") "<<(void*)buff<< " " <<offset<<" "<< size <<" "<<strerror(errno)<< std::endl;
            exit(1);
        }
    }
    // auto elapsed = Timer::getCurrentTime() - start;
    // updateIoRate(elapsed, size);
}

void NewBoundedFilelockCache::preadFromFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset) {
    uint8_t *local = buff;
    // auto origSize = size;
    // auto start = Timer::getCurrentTime();
    
    while (size) {
        // debug() << "reading " << size << " from " << offset << std::endl;
        // debug() << "reading" << fd <<" ("<<_binFd<<","<<_blkFd<<") "<<(void*)buff<<" "<<(void*)local<< " " <<offset<<" "<< size <<" "<<strerror(errno)<< std::endl;

        int bytes = pread(fd, local, size, offset);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
            offset += bytes;
        }
        else {
            debug() << "[TAZER ERROR] Failed a read " << fd <<" ("<<_binFd<<","<<_blkFd<<") "<<(void*)buff<< " " <<offset<<" "<< size <<" "<<strerror(errno)<< std::endl;
            exit(1);
        }
    }
    // auto elapsed = Timer::getCurrentTime() - start;
    // updateIoRate(elapsed, origSize);
}

uint8_t *NewBoundedFilelockCache::getBlockData(unsigned int blockIndex) {
    uint8_t *buff = NULL;
    buff = new uint8_t[_blockSize]; //we could make a pre allocated buff for this...
    int fd = _blkFd;
    // debug() <<   " reading block: " << blockIndex << " size: " << _blockSize << "starting from: " << (uint64_t)blockIndex * _blockSize << std::endl;
    preadFromFile(fd, _blockSize, buff, blockIndex * _blockSize);
    // debug() <<   " getting block: " << blockIndex << " size: " << _blockSize << "hash: " << XXH64(buff, _blockSize, 0) << "!" << std::endl;
    return buff;
}

void NewBoundedFilelockCache::setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size) {
    // debug() <<   " setting block: " << blockIndex << " size: " << size << "hash: " << XXH64(data, size, 0) << "!" << std::endl;
    if (size > _blockSize){
        debug()<<"[ERROR]: trying to write larger blocksize than possible "<<blockIndex<<" "<<size<<std::endl;
    }
    int fd = _blkFd;
    pwriteToFile(fd, size, data, blockIndex * _blockSize);
    auto ret = (*_fsync)(fd); //flush changes
    if (ret != 0 ){
        debug()<<"[TAZER ERROR] "<<_name<<" fsync error"<<std::endl;
        exit(0);
    }
}

void NewBoundedFilelockCache::readFileBlockEntry(FileBlockEntry *entry,Request* req) {
    int fd = _binFd;
    // if((uint8_t *)entry + sizeof(FileBlockEntry)> (uint8_t *)&_cacheEntries[_numBlocks]){
    //     debug()<<"ERROR overflow!!! "<<req->str()<<std::endl;
    // }
    preadFromFile(fd, sizeof(FileBlockEntry), (uint8_t *)entry, entry->id * sizeof(FileBlockEntry));
}

void NewBoundedFilelockCache::writeFileBlockEntry(FileBlockEntry *entry) {
    int fd = _binFd;
    pwriteToFile(fd, sizeof(FileBlockEntry), (uint8_t *)entry, entry->id * sizeof(FileBlockEntry));
    auto ret = (*_fsync)(fd); //flush changes
    if (ret != 0 ){
        debug()<<"[TAZER ERROR] "<<_name<<" fsync error"<<std::endl;
        exit(0);
    }
}

NewBoundedCache<FcntlBoundedReaderWriterLock>::BlockEntry* NewBoundedFilelockCache::getBlockEntry(uint32_t entryIndex, Request* req){
    req->trace(_name)<<"getting entry before"<<blockEntryStr(&_cacheEntries[entryIndex])<<std::endl;
    FileBlockEntry* entry = &_cacheEntries[entryIndex];
    req->trace(_name)<<"getting entry after"<<blockEntryStr(&_cacheEntries[entryIndex])<<std::endl;
    readFileBlockEntry(entry,req);
    return entry;
}



std::string NewBoundedFilelockCache::blockEntryStr(BlockEntry *entry) {
    return entry->str() + " ac: "+ std::to_string(((FileBlockEntry*)entry)->activeCnt)+" pid: "+std::to_string(((FileBlockEntry*)entry)->pid)+" "+((FileBlockEntry*)entry)->fileName;

}

std::vector<NewBoundedCache<FcntlBoundedReaderWriterLock>::BlockEntry*> NewBoundedFilelockCache::readBin(uint32_t binIndex) {
    
    int fd = _binFd;
    int startIndex = binIndex * _associativity;
    preadFromFile(fd, _associativity * (sizeof(FileBlockEntry) ), (uint8_t *)&_cacheEntries[startIndex], startIndex * sizeof(FileBlockEntry));
    std::vector<BlockEntry*> entries;
    for (uint32_t i = 0; i < _associativity; i++) {
        entries.push_back((BlockEntry *)&_cacheEntries[i + startIndex]);
    }
    return entries;
}


// blockAvailable calls are not protected by locks
// we know that when calling blockAvailable the actual data cannot change during the read (due to refernce counting protections)
// what can change is the meta data associated with the last access of the data (and which cache it originated from)
// in certain cases, we want to capture the originating cache and return it for performance reasons
// there is a potential race between when a block is available and when we capture the originating cache
// having origCache be atomic ensures we always get a valid ID (even though it may not always be 100% acurate)
// we are willing to accept some small error in attribution of stats
bool NewBoundedFilelockCache::blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs, uint32_t cnt, CacheType *origCache) {

    bool avail = false;
    if (checkFs && !avail) {
        std::string lockPath = _cachePath + "/lock";

        int fd = _binFd;
        FileBlockEntry * entry = &_cacheEntries[index];
        auto start = Timer::getCurrentTime();
        preadFromFile(fd, sizeof(FileBlockEntry), (uint8_t *)entry, entry->id * sizeof(FileBlockEntry));
        auto elapsed = Timer::getCurrentTime() - start;
        if (cnt % 200000 == 0) {
            log(this) << "going to wait for " << elapsed / 1000000000.0 << " fi: " << fileIndex << " i:" << index << " wait status: " << entry->status << " " << std::string(entry->fileName) << " " << entry->blockIndex << " " << cnt << std::endl;
            log(this) << _name << "rate: " << getRequestTime() << " " << _nextLevel->name() << " rate: " << _nextLevel->getRequestTime() << std::endl;
        }

        if (entry->status == BLK_AVAIL) {
            avail = true;
            if (origCache) {
                *origCache = CacheType::empty;
                *origCache = entry->origCache.load();
                // for better or for worse we allow unlocked access to check if a block avail, 
                // there is potential for a race here when some over thread updates the block (not changing the data, just the metadata)
                while(*origCache == CacheType::empty){ 
                    start = Timer::getCurrentTime();
                    preadFromFile(fd, sizeof(FileBlockEntry), (uint8_t *)&entry, index * sizeof(FileBlockEntry));
                    elapsed += Timer::getCurrentTime() - start;
                    *origCache = entry->origCache.load();
                }
            }
        }

        if (!avail) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(elapsed));
        }
    }
    return avail;
}

std::shared_ptr<typename NewBoundedCache<FcntlBoundedReaderWriterLock>::BlockEntry> NewBoundedFilelockCache::getCompareBlkEntry(uint32_t index, uint32_t fileIndex) {
    std::shared_ptr<BlockEntry> entry(new FileBlockEntry(this,-1));
    FileBlockEntry *fEntry = (FileBlockEntry *)entry.get();
    fEntry->blockIndex = index;
    fEntry->fileIndex = fileIndex;
    _localLock->readerLock();
    std::string name = _fileMap[fileIndex].name;
    _localLock->readerUnlock();
    int len = name.length();
    int start = len < BFL_FILENAME_LEN ? 0 : len - BFL_FILENAME_LEN;
    auto fileName = name.substr(start, BFL_FILENAME_LEN);
    memset(fEntry->fileName, 0, BFL_FILENAME_LEN);
    memcpy(fEntry->fileName, fileName.c_str(), fileName.length());
    return entry;
}

bool NewBoundedFilelockCache::sameBlk(BlockEntry *blk1, BlockEntry *blk2) {
    FileBlockEntry *fblk1 = (FileBlockEntry *)blk1;
    FileBlockEntry *fblk2 = (FileBlockEntry *)blk2;
    bool sameFile = strcmp(fblk1->fileName, fblk2->fileName) == 0;
    bool sameBlk = blk1->blockIndex == blk2->blockIndex;
    return sameFile && sameBlk;
}


//this assumes the passed in entry is valid, which is should be if the ecompasssing bin is locked (which it should be)
int NewBoundedFilelockCache::incBlkCnt(BlockEntry * entry, Request* req) {
    req->trace(_name)<<"incrementing"<<std::endl;
    req->trace(_name)<<blockEntryStr(entry)<<std::endl;
    int curCnt = ((FileBlockEntry*) entry)->activeCnt;
    ((FileBlockEntry*) entry)->activeCnt+=1;
    ((FileBlockEntry*) entry)->pid = _pid;
    writeFileBlockEntry(((FileBlockEntry*) entry));
    req->trace(_name)<<blockEntryStr(entry)<<std::endl;
    return curCnt; //equivalent to what the memcaches do (fetch_add)
}

int NewBoundedFilelockCache::decBlkCnt(BlockEntry * entry, Request* req) {
    req->trace(_name)<<"decrementing"<<std::endl;
    req->trace(_name)<<blockEntryStr(entry)<<std::endl;
    int curCnt = ((FileBlockEntry*) entry)->activeCnt;
    ((FileBlockEntry*) entry)->activeCnt-=1;
    ((FileBlockEntry*) entry)->pid = _pid;
    writeFileBlockEntry(((FileBlockEntry*) entry));
    req->trace(_name)<<blockEntryStr(entry)<<std::endl;
    return curCnt; //equivalent to what the memcaches do(fetch_sub)
}

bool NewBoundedFilelockCache::anyUsers(BlockEntry * entry, Request* req) {
    FileBlockEntry *fEntry = (FileBlockEntry*) entry;
    readFileBlockEntry(fEntry,req);
    return fEntry->activeCnt;
}

void NewBoundedFilelockCache::blockSet(BlockEntry* blk, uint32_t fileIndex, uint32_t blockIndex, uint8_t status, CacheType type, int32_t prefetched, int activeUpdate,Request* req) {
    req->trace(_name)<<"setting block: "<<blockEntryStr(blk)<<std::endl;
    FileBlockEntry* entry = (FileBlockEntry*)blk;
    _localLock->readerLock();
    std::string name = _fileMap[fileIndex].name;
    _localLock->readerUnlock();
    int len = name.length();
    int start = len < BFL_FILENAME_LEN ? 0 : len - BFL_FILENAME_LEN;
    auto fileName = name.substr(start, BFL_FILENAME_LEN);
    memset(entry->fileName, 0, BFL_FILENAME_LEN);
    memcpy(entry->fileName, fileName.c_str(), fileName.length());
    entry->fileIndex = 0;
    entry->blockIndex = blockIndex;
    entry->timeStamp = Timer::getTimestamp();
    entry->origCache.store(type);
    entry->status = status;
    if (prefetched >= 0) {
        entry->prefetched = prefetched;
    }
    if (activeUpdate >0){
        req->trace(_name)<<"incrementing"<<std::endl;
        entry->activeCnt+=1; // we do this explicity for files to avoid multiple writes...
    }
    else if (activeUpdate < 0){
        req->trace(_name)<<"decrementing"<<std::endl;
        entry->activeCnt-=1; // we do this explicity for files to avoid multiple writes...
    }
    entry->pid = _pid;
    // log(this) << "blkSet: " << entry.fileIndex << " " << entry.blockIndex << " " << entry.fileName << " " << entry.status << std::endl;
    writeFileBlockEntry(entry);
    req->trace(_name)<<"blockset: "<<blockEntryStr(entry)<<std::endl;

    // FileBlockEntry tempentry;
    // tempentry.id = entry->id;
    // readFileBlockEntry(&tempentry, req);
    // if (!sameBlk(entry,&tempentry)){
    //     debug() <<"[ERROR] blocks are not the same!!!"<<std::endl;
    //     debug() <<blockEntryStr(entry)<<std::endl;
    //     debug() <<blockEntryStr(&tempentry)<<std::endl;
    //     exit(0);
    // }

}

bool NewBoundedFilelockCache::writeBlock(Request *req) {
    if (req->reservedMap[this] > 0 || _myOutstandingWrites.load() < 1000 || req->originating == this) {
        _myOutstandingWrites.fetch_add(1);
        // std::cout<<"[TAZER] " << "bounded filelock write req id:" << req->id <<" ow: "<<_myOutstandingWrites.load()<< std::endl;

        _writePool->addTask([this, req] {
            //debug()<<"[TAZER] " << "buffered write: " << (void *)originating << std::endl;
            if (!NewBoundedCache::writeBlock(req)) {
                DPRINTF("FAILED WRITE...\n");
            }
            _myOutstandingWrites.fetch_sub(1);
        });
    }
    else if (_nextLevel) {
        _nextLevel->writeBlock(req);
    }
    return true;
}

void NewBoundedFilelockCache::readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority) {
    
    if (_myOutstandingWrites.load() < 1000){ 
        NewBoundedCache::readBlock(req, reads, priority);
    }
    else if (_nextLevel) { //so many outstanding requests that we should reduce pressure to the file cache...
        _nextLevel->readBlock(req, reads, priority);
    }
}

void NewBoundedFilelockCache::cleanUpBlockData(uint8_t *data) {
    // debug()<<_name<<" delete data"<<std::endl;
    delete[] data;
}

Cache *NewBoundedFilelockCache::addNewBoundedFilelockCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string cachePath) {
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [=]() -> Cache * {
            Cache *temp = new NewBoundedFilelockCache(cacheName, type, cacheSize, blockSize, associativity, cachePath);
            if (temp)
                return temp;
            delete temp;
            return NULL;
        });
}
