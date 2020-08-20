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

#include "BoundedFilelockCache.h"
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

//TODO: create version that locks the file when reserved and only releases after it has written it...
BoundedFilelockCache::BoundedFilelockCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string cachePath) : BoundedCache(cacheName, type, cacheSize, blockSize, associativity),
                                                                                                                                                           _open((unixopen_t)dlsym(RTLD_NEXT, "open")),
                                                                                                                                                           _close((unixclose_t)dlsym(RTLD_NEXT, "close")),
                                                                                                                                                           _read((unixread_t)dlsym(RTLD_NEXT, "read")),
                                                                                                                                                           _write((unixwrite_t)dlsym(RTLD_NEXT, "write")),
                                                                                                                                                           _lseek((unixlseek_t)dlsym(RTLD_NEXT, "lseek")),
                                                                                                                                                           _fsync((unixfdatasync_t)dlsym(RTLD_NEXT, "fdatasync")),
                                                                                                                                                           _stat((unixxstat_t)dlsym(RTLD_NEXT, "stat")),
                                                                                                                                                           _cachePath(cachePath),
                                                                                                                                                           _myOutstandingWrites(0) {
    stats.start();
    std::error_code err;
    std::experimental::filesystem::create_directories(_cachePath, err);
    int ret = mkdir((_cachePath + "/init_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + "/").c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); //if -1 means another process has already reserved
    if (ret == 0) {
        // _shmLock->writerLock();
        int fd = (*_open)((_cachePath + "/lock_tmp").c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        ftruncate(fd, _numBlocks * sizeof(FileBlockEntry));
        uint8_t byte = 0;
        (*_lseek)(fd, _numBlocks * sizeof(FileBlockEntry), SEEK_SET);
        (*_write)(fd, &byte, sizeof(uint8_t));
        (*_fsync)(fd);
        (*_close)(fd);
        int ret = rename((_cachePath + "/lock_tmp").c_str(), (_cachePath + "/lock" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity)).c_str());
        if (ret != 0) {
            std::cout << _name << " ERROR: lock rename went wrong!!!" << strerror(errno) << std::endl;
            exit(1);
        }
        fd = (*_open)((_cachePath + "/data_tmp").c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        ftruncate(fd, _numBlocks * _blockSize * sizeof(uint8_t));
        (*_lseek)(fd, _numBlocks * _blockSize * sizeof(uint8_t), SEEK_SET);
        (*_write)(fd, &byte, sizeof(uint8_t));
        (*_fsync)(fd);
        (*_close)(fd);
        ret = rename((_cachePath + "/data_tmp").c_str(), (_cachePath + "/data" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".gc").c_str());
        if (ret != 0) {
            std::cout << _name << " ERROR: data rename went wrong!!!" << strerror(errno) << std::endl;
            exit(1);
        }
        // _shmLock->writerUnlock();
    }
    else {
        std::cout << "reusing lock and data file" << std::endl;
        int cnt = 0;
        while (!std::experimental::filesystem::exists(_cachePath + "/lock" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity))) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (cnt++ > 100) {
                std::cout << _name << " ERROR: waiting for lock to appear!!!" << strerror(errno) << _cachePath + "/data" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".gc" << std::endl;
                exit(1);
            }
        }
        cnt = 0;
        while (!std::experimental::filesystem::exists(_cachePath + "/data" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".gc")) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (cnt++ > 100) {
                std::cout << _name << " ERROR: waiting for data to appear!!!" << strerror(errno) << _cachePath + "/data" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".gc" << std::endl;
                exit(1);
            }
        }
    }
    _lockPath = _cachePath + "/lock" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity);
    _cachePath = _cachePath + "/data" + "_" + std::to_string(_cacheSize) + "_" + std::to_string(_blockSize) + "_" + std::to_string(_associativity) + ".gc";

    _binLock = new FcntlBoundedReaderWriterLock(sizeof(FileBlockEntry) * _associativity, _numBins, _lockPath);
    _blkLock = new FcntlBoundedReaderWriterLock(_blockSize, _numBlocks, _cachePath);

    _binFd = (*_open)(_lockPath.c_str(), O_RDWR);
    _blkFd = (*_open)(_cachePath.c_str(), O_RDWR);

    _shared = true;

    _writePool = new ThreadPool<std::function<void()>>(Config::numWriteBufferThreads, "bounded file lock cache write pool");
    _writePool->initiate();

    // does implementing a lock around disk ops improve i/o performance?
    // std::string shmPath("/" + Config::tazer_id  + "_fcntlbnded_shm.lck");
    // int shmFd = shm_open(shmPath.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
    // if (shmFd == -1) {
    //     shmFd = shm_open(shmPath.c_str(), O_RDWR, 0644);
    //     if (shmFd != -1) {
    //         std::cout << "resusing fcntl shm lock" << std::endl;
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
    //     std::cout << "creating fcntl shm lock" << std::endl;
    //     ftruncate(shmFd, sizeof(uint32_t) + sizeof(ReaderWriterLock));
    //     void *ptr = mmap(NULL, sizeof(uint32_t) + sizeof(ReaderWriterLock), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);

    //     uint32_t *init = (uint32_t *)ptr;
    //     _shmLock = new ((uint8_t *)init + sizeof(uint32_t)) ReaderWriterLock();
    //     *init = 1;
    // }
    stats.end(false, CacheStats::Metric::constructor);
}

BoundedFilelockCache::~BoundedFilelockCache() {
    _terminating = true;
    stats.start();
    while (_outstandingWrites.load()) {
        std::this_thread::yield();
    }
    while (_myOutstandingWrites.load()) {
        std::this_thread::yield();
    }

    std::cout << _name << " cache collisions: " << _collisions << std::endl;
    (*close)(_binFd);
    (*close)(_blkFd);
    delete _binLock;
    delete _blkLock;
    std::string shmPath("/" + Config::tazer_id + "_fcntlbnded_shm.lck");
    shm_unlink(shmPath.c_str());
    stats.end(false, CacheStats::Metric::destructor);
    stats.print(_name);
    std::cout << std::endl;
}

void BoundedFilelockCache::writeToFile(int fd, uint64_t size, uint8_t *buff) {
    uint8_t *local = buff;
    while (size) {
        int bytes = (*_write)(fd, local, size);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            log(this) << "Failed a write " << fd << " " << size << std::endl;
        }
    }
}

void BoundedFilelockCache::readFromFile(int fd, uint64_t size, uint8_t *buff) {
    uint8_t *local = buff;
    while (size) {
        // std::cout << "reading " << size << " more" << std::endl;
        int bytes = (*_read)(fd, local, size);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            log(this) << "Failed a read " << fd << " " << size << std::endl;
        }
    }
}

void BoundedFilelockCache::pwriteToFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset) {
    uint8_t *local = buff;
    // auto origSize = size;
    // auto start = Timer::getCurrentTime();
    while (size) {
        // int bytes = (*_write)(fd, local, size);
        // std::cout << "writing " << size << " from " << offset << std::endl;
        int bytes = pwrite(fd, local, size, offset);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
            offset += bytes;
        }
        else {
            log(this) << "Failed a write " << fd << " " << size << std::endl;
        }
    }
    // auto elapsed = Timer::getCurrentTime() - start;
    // updateIoRate(elapsed, size);
}

void BoundedFilelockCache::preadFromFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset) {
    uint8_t *local = buff;
    // auto origSize = size;
    // auto start = Timer::getCurrentTime();
    while (size) {
        // std::cout << "reading " << size << " from " << offset << std::endl;
        int bytes = pread(fd, local, size, offset);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
            offset += bytes;
        }
        else {
            log(this) << "Failed a read " << fd << " " << size << std::endl;
        }
    }
    // auto elapsed = Timer::getCurrentTime() - start;
    // updateIoRate(elapsed, origSize);
}

uint8_t *BoundedFilelockCache::getBlockData(unsigned int blockIndex) {
    uint8_t *buff = NULL;
    buff = new uint8_t[_blockSize]; //we could make a pre allocated buff for this...
    int fd = _blkFd;
    // std::cout <<   " reading block: " << blockIndex << " size: " << _blockSize << "starting from: " << (uint64_t)blockIndex * _blockSize << std::endl;
    preadFromFile(fd, _blockSize, buff, blockIndex * _blockSize);
    // std::cout <<   " getting block: " << blockIndex << " size: " << _blockSize << "hash: " << XXH64(buff, _blockSize, 0) << "!" << std::endl;
    return buff;
}

void BoundedFilelockCache::setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size) {
    // std::cout <<   " setting block: " << blockIndex << " size: " << size << "hash: " << XXH64(data, size, 0) << "!" << std::endl;
    int fd = _blkFd;
    pwriteToFile(fd, size, data, blockIndex * _blockSize);
    (*_fsync)(fd); //flush changes
}

void BoundedFilelockCache::readFileBlockEntry(uint32_t blockIndex, FileBlockEntry *entry) {
    int fd = _binFd;
    preadFromFile(fd, sizeof(*entry), (uint8_t *)entry, blockIndex * sizeof(*entry));
}

void BoundedFilelockCache::writeFileBlockEntry(uint32_t blockIndex, FileBlockEntry *entry) {

    int fd = _binFd;
    pwriteToFile(fd, sizeof(*entry), (uint8_t *)entry, blockIndex * sizeof(*entry));
    (*_fsync)(fd);
}

void BoundedFilelockCache::readBlockEntry(uint32_t blockIndex, BlockEntry *entry) {
    int fd = _binFd;
    FileBlockEntry fEntry(this);
    preadFromFile(fd, sizeof(FileBlockEntry), (uint8_t *)&fEntry, blockIndex * sizeof(FileBlockEntry));
    // log(this) << fEntry.fileIndex << " " << fEntry.blockIndex << " " << fEntry.status << " " << fEntry.fileName << std::endl;
    *entry = *(BlockEntry *)&fEntry;
    // log(this) << entry->fileIndex << " " << entry->blockIndex << " " << entry->status << std::endl;
}

void BoundedFilelockCache::writeBlockEntry(uint32_t blockIndex, BlockEntry *entry) {

    int fd = _binFd;
    FileBlockEntry fEntry(this);
    *(BlockEntry *)&fEntry = *entry;
    _localLock->readerLock();
    std::string name = _fileMap[fEntry.fileIndex].name;
    _localLock->readerUnlock();
    int len = name.length();
    int start = len < 100 ? 0 : len - 100;
    auto fileName = name.substr(start, 100);
    memset(fEntry.fileName, 0, 100);
    memcpy(fEntry.fileName, fileName.c_str(), fileName.length());

    pwriteToFile(fd, sizeof(FileBlockEntry), (uint8_t *)&fEntry, blockIndex * sizeof(FileBlockEntry));
    (*_fsync)(fd);
}

void BoundedFilelockCache::readBin(uint32_t binIndex, BlockEntry *entries) {
    int fd = _binFd;
    FileBlockEntry* fEntries = new FileBlockEntry[_associativity];
    for (uint32_t i =0; i< _associativity;++i){
        fEntries[i].init(this);
    }
    preadFromFile(fd, (sizeof(FileBlockEntry) * _associativity), (uint8_t *)fEntries, binIndex * (sizeof(FileBlockEntry) * _associativity));

    // int startIndex = binIndex * _associativity;
    for (uint32_t i = 0; i < _associativity; i++) {
        entries[i] = *(BlockEntry *)&fEntries[i];
    }
    delete[] fEntries;
}

std::vector<std::shared_ptr<BoundedCache<FcntlBoundedReaderWriterLock>::BlockEntry>> BoundedFilelockCache::readBin(uint32_t binIndex) {
    int fd = _binFd;
    // FileBlockEntry fEntries[_associativity];
    FileBlockEntry* fEntries = new FileBlockEntry[_associativity];
    for (uint32_t i =0; i< _associativity;++i){
        fEntries[i].init(this);
    }
    preadFromFile(fd, (sizeof(FileBlockEntry) * _associativity), (uint8_t *)fEntries, binIndex * (sizeof(FileBlockEntry) * _associativity));

    std::vector<std::shared_ptr<BlockEntry>> entries;
    // int startIndex = binIndex * _associativity;
    for (uint32_t i = 0; i < _associativity; i++) {
        entries.emplace_back(new FileBlockEntry(&fEntries[i]));
    }
    delete[] fEntries;
    return entries;
}


// blockAvailable calls are not protected by locks
// we know that when calling blockAvailable the actual data cannot change during the read (due to refernce counting protections)
// what can change is the meta data associated with the last access of the data (and which cache it originated from)
// in certain cases, we want to capture the originating cache and return it for performance reasons
// there is a potential race between when a block is available and when we capture the originating cache
// having origCache be atomic ensures we always get a valid ID (even though it may not always be 100% acurate)
// we are willing to accept some small error in attribution of stats
bool BoundedFilelockCache::blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs, uint32_t cnt, CacheType *origCache) {

    bool avail = false;
    if (checkFs && !avail) {
        std::string lockPath = _cachePath + "/lock";

        int fd = _binFd;
        FileBlockEntry entry(this);
        auto start = Timer::getCurrentTime();
        preadFromFile(fd, sizeof(entry), (uint8_t *)&entry, index * sizeof(entry));
        auto elapsed = Timer::getCurrentTime() - start;
        if (cnt % 200000 == 0) {
            log(this) << "going to wait for " << elapsed / 1000000000.0 << " fi: " << fileIndex << " i:" << index << " wait status: " << entry.status << " " << std::string(entry.fileName) << " " << entry.blockIndex << " " << cnt << std::endl;
            log(this) << _name << "rate: " << getRequestTime() << " " << _nextLevel->name() << " rate: " << _nextLevel->getRequestTime() << std::endl;
        }

        if (entry.status == BLK_AVAIL) {
            avail = true;
            if (origCache) {
                *origCache = CacheType::empty;
                *origCache = entry.origCache.load();
                // for better or for worse we allow unlocked access to check if a block avail, 
                // there is potential for a race here when some over thread updates the block (not changing the data, just the metadata)
                while(*origCache == CacheType::empty){ 
                    start = Timer::getCurrentTime();
                    preadFromFile(fd, sizeof(entry), (uint8_t *)&entry, index * sizeof(entry));
                    elapsed += Timer::getCurrentTime() - start;
                    *origCache = entry.origCache.load();
                }
            }
        }

        if (!avail) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(elapsed));
        }
    }
    return avail;
}

std::shared_ptr<typename BoundedCache<FcntlBoundedReaderWriterLock>::BlockEntry> BoundedFilelockCache::getCompareBlkEntry(uint32_t index, uint32_t fileIndex) {
    std::shared_ptr<BlockEntry> entry(new FileBlockEntry(this));
    FileBlockEntry *fEntry = (FileBlockEntry *)entry.get();
    fEntry->blockIndex = index + 1;
    fEntry->fileIndex = fileIndex + 1;
    _localLock->readerLock();
    std::string name = _fileMap[fileIndex].name;
    _localLock->readerUnlock();
    int len = name.length();
    int start = len < 100 ? 0 : len - 100;
    auto fileName = name.substr(start, 100);
    memset(fEntry->fileName, 0, 100);
    memcpy(fEntry->fileName, fileName.c_str(), fileName.length());
    return entry;
}

bool BoundedFilelockCache::sameBlk(BlockEntry *blk1, BlockEntry *blk2) {
    FileBlockEntry *fblk1 = (FileBlockEntry *)blk1;
    FileBlockEntry *fblk2 = (FileBlockEntry *)blk2;
    bool sameFile = strcmp(fblk1->fileName, fblk2->fileName) == 0;
    bool sameBlk = blk1->blockIndex == blk2->blockIndex;
    return sameFile && sameBlk;
}

int BoundedFilelockCache::incBlkCnt(uint32_t blk) {
    _blkLock->readerLock(blk);
    return -1;
}

int BoundedFilelockCache::decBlkCnt(uint32_t blk) {
    _blkLock->readerUnlock(blk);
    return -1;
}

bool BoundedFilelockCache::anyUsers(uint32_t blk) {
    return _blkLock->lockAvail(blk) == 1;
}

void BoundedFilelockCache::blockSet(uint32_t index, uint32_t fileIndex, uint32_t blockIndex, uint8_t status, CacheType type, int32_t prefetched) {
    FileBlockEntry entry(this);
    _localLock->readerLock();
    std::string name = _fileMap[fileIndex].name;
    _localLock->readerUnlock();
    int len = name.length();
    int start = len < 100 ? 0 : len - 100;
    auto fileName = name.substr(start, 100);
    memset(entry.fileName, 0, 100);
    memcpy(entry.fileName, fileName.c_str(), fileName.length());
    entry.fileIndex = 0;
    entry.blockIndex = blockIndex + 1;
    entry.status = status;
    entry.timeStamp = Timer::getTimestamp();
    entry.origCache.store(type);
    if (prefetched >= 0) {
        entry.prefetched = prefetched;
    }
    // log(this) << "blkSet: " << entry.fileIndex << " " << entry.blockIndex << " " << entry.fileName << " " << entry.status << std::endl;
    writeFileBlockEntry(index, &entry);
}

bool BoundedFilelockCache::writeBlock(Request *req) {
    _myOutstandingWrites.fetch_add(1);
    _writePool->addTask([this, req] {
        //std::cout<<"[TAZER] " << "buffered write: " << (void *)originating << std::endl;
        if (!BoundedCache::writeBlock(req)) {
            DPRINTF("FAILED WRITE...\n");
        }
        _myOutstandingWrites.fetch_sub(1);
    });
    return true;
}

void BoundedFilelockCache::cleanUpBlockData(uint8_t *data) {
    delete[] data;
}

Cache *BoundedFilelockCache::addNewBoundedFilelockCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string cachePath) {
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [=]() -> Cache * {
            Cache *temp = new BoundedFilelockCache(cacheName, type, cacheSize, blockSize, associativity, cachePath);
            if (temp)
                return temp;
            delete temp;
            return NULL;
        });
}
