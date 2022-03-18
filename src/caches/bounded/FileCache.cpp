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

#include "FileCache.h"
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "Message.h"
#include "ReaderWriterLock.h"
#include "Timer.h"
#include "lz4.h"

#include <chrono>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <future>
#include <experimental/filesystem>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

FileCache::FileCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string filePath) : BoundedCache(cacheName, type, cacheSize, blockSize, associativity),
                                                                                                                                                            _open((unixopen_t)dlsym(RTLD_NEXT, "open")),
                                                                                                                                                            _close((unixclose_t)dlsym(RTLD_NEXT, "close")),
                                                                                                                                                            _read((unixread_t)dlsym(RTLD_NEXT, "read")),
                                                                                                                                                            _write((unixwrite_t)dlsym(RTLD_NEXT, "write")),
                                                                                                                                                            _lseek((unixlseek_t)dlsym(RTLD_NEXT, "lseek")),
                                                                                                                                                            _fsync((unixfdatasync_t)dlsym(RTLD_NEXT, "fdatasync"))
                                                                                                                                                            {
    // std::cout<<"[TAZER] " << "Constructing " << _name << " in shared memory cache" << std::endl;
    stats.start(false, CacheStats::Metric::constructor);
    _blocksfd = -1;
    _cachePath = filePath + "/" + Config::tazer_id + "_" + _name + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".tzr";
    std::string indexPath("/" + Config::tazer_id + "_" + _name + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".idx");
    // bool *indexInit;

    int fd = shm_open(indexPath.c_str(), O_CREAT | O_EXCL | O_RDWR,  0666 );
    if (fd == -1) {
        DPRINTF("Reusing shared memory\n");
        debug() << "Reusing filecache shared memory" << std::endl;
        fd = shm_open(indexPath.c_str(), O_RDWR, 0666);
        if (fd != -1) {
            ftruncate(fd, sizeof(uint32_t) +  _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins));
            void *ptr = mmap(NULL, sizeof(uint32_t) + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            uint32_t *init = (uint32_t *)ptr;
            log(this) << "init: " << *init << std::endl;
            std::cout<<std::endl;
            while (!*init) {
                sched_yield();
            }
            std::cout<<std::endl;
            _blkIndex = (MemBlockEntry *)((uint8_t *)init + sizeof(uint32_t));
            auto binLockDataAddr = (uint8_t *)_blkIndex + _numBlocks * sizeof(MemBlockEntry);
            _binLock = new MultiReaderWriterLock(_numBins, binLockDataAddr);

            log(this) << "init: " << (uint32_t)*init << std::endl;
        }
        else {
            std::cerr << "[TAZER] "
                      << "Error opening shared memory " << strerror(errno) << std::endl;
        }
    }
    else {
        DPRINTF("Created shared memory\n");
        debug()  << _name << "created filecache shared memory" << std::endl;
        ftruncate(fd, sizeof(uint32_t) + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins));
        void *ptr = mmap(NULL, sizeof(uint32_t) + _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        uint32_t *init = (uint32_t *)ptr;
        std::cout<<std::endl;
        log(this) << "init: " << *init << std::endl;
        *init = 0;
        log(this) << "init: " << *init << std::endl;
        _blkIndex = (MemBlockEntry *)((uint8_t *)init + sizeof(uint32_t));
        auto binLockDataAddr = (uint8_t *)_blkIndex + _numBlocks * sizeof(MemBlockEntry);
        _binLock = new MultiReaderWriterLock(_numBins, binLockDataAddr, true);
        _binLock->writerLock(0);
        for (uint32_t i=0;i<_numBlocks;i++){
            _blkIndex[i].init(this,i);
        }
        _binLock->writerUnlock(0);
        *init = 1;
        std::cout<<std::endl;
        log(this) << "init: " << *init << std::endl;
    }

    // for (uint32_t i=0;i<100; i++){
    //     std::cout <<"[TAZER]" << _name << " " << blockEntryStr((BlockEntry*)&_blkIndex[i]) << std::endl;
    // }

    uint8_t byte = '\0';
    std::error_code err;
    std::experimental::filesystem::create_directories(filePath, err);
    int ret = mkdir((filePath + "/init_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + "/").c_str(), S_IRWXU | S_IRWXG | S_IRWXO); //if -1 means another process has already reserved
    if (ret == 0) {
        //first process... create data file...
        fd = (*_open)((filePath + "/data_tmp").c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR| S_IWUSR | S_IRGRP| S_IWGRP | S_IROTH | S_IWOTH );
        if (fd < 0 ){
            debug()<<"[TAZER ERROR] "<<_name<<" data creation open error "<<strerror(errno)<<std::endl;
            exit(1);
        }
        ftruncate(fd, _numBlocks * _blockSize * sizeof(uint8_t));
        (*_lseek)(fd, _numBlocks * _blockSize * sizeof(uint8_t), SEEK_SET);
        (*_write)(fd, &byte, sizeof(uint8_t));
        (*_fsync)(fd);
        (*_close)(fd);
        rename((filePath + "/data_tmp").c_str(),_cachePath.c_str());
    }
    else{//reusing data file;
        int cnt = 0;
        while (!std::experimental::filesystem::exists(_cachePath)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (cnt++ > 100) {
                debug() << _name << " ERROR: waiting for data to appear!!!" << strerror(errno) << _cachePath  << std::endl;
                exit(1);
            }
        }
    }
    debug() <<"filecache ready"<<std::endl;
    _blocksfd = (*_open)(_cachePath.c_str(), O_RDWR| O_SYNC);
    _pid = (uint32_t)::getpid();

    _shared = true;
    stats.end(false, CacheStats::Metric::constructor);
}

FileCache::~FileCache() {
    //std::cout<<"[TAZER] " << "deleting " << _name << " in shared memory cache, collisions: " << _collisions << std::endl;
    //std::cout<<"[TAZER] " << "numBlks: " << _numBlocks << " numBins: " << _numBins << " cacheSize: " << _cacheSize << std::endl;
    stats.start(false, CacheStats::Metric::destructor);
    if (false) {
        //code from FileCacheRegister...
    }
    uint32_t numEmpty = 0;
    for (uint32_t i = 0; i < _numBlocks; i++) {
        if (_blkIndex[i].activeCnt > 0) {
            // std::cout << "[TAZER] " << _name << " " << i << " " << _numBlocks << " " << _blkIndex[i].activeCnt << " " << _blkIndex[i].fileIndex - 1 << " " << _blkIndex[i].blockIndex - 1 << " "
            //           << "prefetched" << _blkIndex[i].prefetched << std::endl;
            std::cout <<"[TAZER]" << _name << " " << blockEntryStr((BlockEntry*)&_blkIndex[i]) << std::endl;
        }
        if(_blkIndex[i].status == BLK_EMPTY){
            numEmpty+=1;
        }
    }
    std::cout<<_name<<" number of empty blocks: "<<numEmpty<<std::endl;
    stats.end(false, CacheStats::Metric::destructor);
    stats.print(_name);
    std::cout << std::endl;

    delete _binLock;
}



void FileCache::writeToFile(int fd, uint64_t size, uint8_t *buff) {
    uint8_t *local = buff;
    while (size) {
        int bytes = (*_write)(fd, local, size);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            debug() << "[TAZER ERROR] Failed a write " << fd <<" "<<(void*)buff<< " " <<" "<< size <<" "<<strerror(errno)<< std::endl;
            exit(1);
        }
    }
}

void FileCache::readFromFile(int fd, uint64_t size, uint8_t *buff) {
    uint8_t *local = buff;
    while (size) {
        // debug() << "reading " << size << " more" << std::endl;
        int bytes = (*_read)(fd, local, size);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            debug() << "[TAZER ERROR] Failed a read " << fd <<" "<<(void*)buff<< " " <<" "<< size <<" "<<strerror(errno)<< std::endl;
            exit(1);
        }
    }
}

void FileCache::pwriteToFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset) {
    uint8_t *local = buff;
    while (size) {
        int bytes = pwrite(fd, local, size, offset);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
            offset += bytes;
        }
        else {
            debug() << "[TAZER ERROR] Failed a pwrite " << fd <<" "<<(void*)buff<< " " <<offset<<" "<< size <<" "<<strerror(errno)<< std::endl;
            exit(1);
        }
    }
}

void FileCache::preadFromFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset) {
    uint8_t *local = buff;
    
    while (size) {
        int bytes = pread(fd, local, size, offset);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
            offset += bytes;
        }
        else {
            debug() << "[TAZER ERROR] Failed a pread " << fd <<" "<<(void*)buff<< " " <<offset<<" "<< size <<" "<<strerror(errno)<< std::endl;
            exit(1);
        }
    }
}

void FileCache::setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size) {
    

    if (size > _blockSize){
        debug()<<"[ERROR]: trying to write larger blocksize than possible "<<blockIndex<<" "<<size<<std::endl;
    }
    int fd = _blocksfd;
    pwriteToFile(fd, size, data, blockIndex * _blockSize);
    auto ret = (*_fsync)(fd); //flush changes
    if (ret != 0 ){
        debug()<<"[TAZER ERROR] "<<_name<<" fsync error"<<" "<<strerror(errno)<<std::endl;
        exit(0);
    }
}
uint8_t *FileCache::getBlockData(unsigned int blockIndex) {
    uint8_t *buff = NULL;
    buff = new uint8_t[_blockSize]; //we could make a pre allocated buff for this...
    int fd = _blocksfd;
    preadFromFile(fd, _blockSize, buff, blockIndex * _blockSize);
    return buff;
}

void FileCache::cleanUpBlockData(uint8_t *data) {
    delete[] data;
}

void FileCache::blockSet(BlockEntry* blk, uint32_t fileIndex, uint32_t blockIndex, uint8_t status, CacheType type, int32_t prefetched, int activeUpdate, Request* req) {
    req->trace(_name)<<"setting block: "<<blockEntryStr(blk)<<std::endl;
    blk->fileIndex = fileIndex;
    blk->blockIndex = blockIndex;
    blk->timeStamp = Timer::getCurrentTime();
    blk->origCache.store(type);
    blk->status = status;
    if (prefetched >= 0) {
        blk->prefetched = prefetched;
    }
    if (activeUpdate >0){
        incBlkCnt(blk,req);
    }
    else if (activeUpdate < 0){
        decBlkCnt(blk,req);
    }
    req->trace(_name)<<"blockset: "<<blockEntryStr(blk)<<std::endl;
}

typename BoundedCache<MultiReaderWriterLock>::BlockEntry* FileCache::getBlockEntry(uint32_t blockIndex, Request* req){
    return (BlockEntry*)&_blkIndex[blockIndex];
}


// blockAvailable calls are not protected by locks
// we know that when calling blockAvailable the actual data cannot change during the read (due to refernce counting protections)
// what can change is the meta data associated with the last access of the data (and which cache it originated from)
// in certain cases, we want to capture the originating cache and return it for performance reasons
// there is a potential race between when a block is available and when we capture the originating cache
// having origCache be atomic ensures we always get a valid ID (even though it may not always be 100% acurate)
// we are willing to accept some small error in attribution of stats
bool FileCache::blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs, uint32_t cnt, CacheType *origCache) {
    bool avail = _blkIndex[index].status == BLK_AVAIL;
    if (origCache && avail) {
        *origCache = CacheType::empty;
        *origCache = _blkIndex[index].origCache.load();
        // for better or for worse we allow unlocked access to check if a block avail, 
        // there is potential for a race here when some over thread updates the block (not changing the data, just the metadata)
        while(*origCache == CacheType::empty){ 
            *origCache = _blkIndex[index].origCache.load();
        }
        return true;
    }
    return avail;
}


std::vector<BoundedCache<MultiReaderWriterLock>::BlockEntry*> FileCache::readBin(uint32_t binIndex) {
    std::vector<BlockEntry*> entries;
    int startIndex = binIndex * _associativity;
    for (uint32_t i = 0; i < _associativity; i++) {
        entries.push_back((BlockEntry *)&_blkIndex[i + startIndex]);
    }
    return entries;
}

std::string  FileCache::blockEntryStr(BlockEntry *entry){
    return entry->str() + " ac: "+ std::to_string(((MemBlockEntry*)entry)->activeCnt);
}

int FileCache::incBlkCnt(BlockEntry * entry, Request* req) {
    req->trace(_name)<<"incrementing"<<std::endl;
    // debug()<<"incrementing "<<blockEntryStr(entry)<<std::endl;
    return ((MemBlockEntry*)entry)->activeCnt.fetch_add(1);
}
int FileCache::decBlkCnt(BlockEntry * entry, Request* req) {
    req->trace(_name)<<"decrementing"<<std::endl;
    // debug()<<"decrementing "<<blockEntryStr(entry)<<std::endl;
    return ((MemBlockEntry*)entry)->activeCnt.fetch_sub(1);
}

bool FileCache::anyUsers(BlockEntry * entry, Request* req) {
    return ((MemBlockEntry*)entry)->activeCnt;
}

Cache *FileCache::addFileCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string cachePath) {
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [&]() -> Cache * {
            Cache *temp = new FileCache(cacheName, type, cacheSize, blockSize, associativity,cachePath);
            return temp;
        });
}
