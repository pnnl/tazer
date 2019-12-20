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

#include "InputFile.h"
#include "BlockSizeTranslationCache.h"
#include "BoundedFilelockCache.h"
#include "Cache.h"
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "DeltaPrefetcher.h"
#include "FcntlCache.h"
#include "FileCache.h"
#include "FileCacheRegister.h"
#include "FilelockCache.h"
#include "LocalFileCache.h"
#include "MemoryCache.h"
#include "Message.h"
#include "NetworkCache.h"
#include "PerfectPrefetcher.h"
#include "Prefetcher.h"
#include "Request.h"
#include "SharedMemoryCache.h"
#include "Timer.h"
#include "UnixIO.h"
#include "lz4.h"
#include "xxhash.h"
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <thread>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

#define TIMEON(...) __VA_ARGS__
//#define TIMEON(...)

#define RESERVESIZE ((_compress) ? LZ4_compressBound(_blkSize) + _blkSize : _blkSize)

std::once_flag init_flag;

PriorityThreadPool<std::packaged_task<std::shared_future<Request *>()>> InputFile::_transferPool(Config::numClientTransThreads, "transfer pool");
PriorityThreadPool<std::packaged_task<Request *()>> InputFile::_decompressionPool(Config::numClientDecompThreads, "decompress pool");

Cache *InputFile::_cache = NULL; //(BASECACHENAME);

void /*__attribute__((constructor))*/ InputFile::cache_init(void) {
    int level = 0;
    Cache *c = NULL;

    if (Config::useMemoryCache) {
        Cache *c = MemoryCache::addNewMemoryCache(MEMORYCACHENAME, Config::memoryCacheSize, Config::memoryCacheBlocksize, Config::memoryCacheAssociativity);
        std::cerr << "[TAZER] "
                  << "mem cache: " << (void *)c << std::endl;
        InputFile::_cache->addCacheLevel(c, ++level);
    }

    if (Config::useSharedMemoryCache && Config::enableSharedMem) {
        c = SharedMemoryCache::addNewSharedMemoryCache(SHAREDMEMORYCACHENAME, Config::sharedMemoryCacheSize, Config::sharedMemoryCacheBlocksize, Config::sharedMemoryCacheAssociativity);
        std::cerr << "[TAZER] "
                  << "shared mem cache: " << (void *)c << std::endl;
        InputFile::_cache->addCacheLevel(c, ++level);
    }

    if (Config::useBurstBufferCache) {
        c = FileCache::addNewFileCache("burstbuffer", Config::burstBufferCacheSize, Config::burstBufferCacheBlocksize, Config::burstBufferCacheAssociativity, Config::burstBufferCacheFilePath);
        std::cerr << "[TAZER] "
                  << "bb cache: " << (void *)c << std::endl;
        InputFile::_cache->addCacheLevel(c, ++level);
    }

    if (Config::useFileCache) {
        c = FileCache::addNewFileCache(FILECACHENAME, Config::fileCacheSize, Config::fileCacheBlocksize, Config::fileCacheAssociativity, Config::fileCacheFilePath);
        std::cerr << "[TAZER] "
                  << "file cache: " << (void *)c << std::endl;
        InputFile::_cache->addCacheLevel(c, ++level);
    }

    if (Config::useBoundedFilelockCache) {
        c = BoundedFilelockCache::addNewBoundedFilelockCache(BOUNDEDFILELOCKCACHENAME, Config::boundedFilelockCacheSize, Config::boundedFilelockCacheBlocksize, Config::boundedFilelockCacheAssociativity, Config::boundedFilelockCacheFilePath);
        std::cerr << "[TAZER] "
                  << "bounded filelock cache: " << (void *)c << std::endl;
        InputFile::_cache->addCacheLevel(c, ++level);
    }

    // if (Config::useFilelockCache) {
    //     c = FilelockCache::addNewFilelockCache(FILELOCKCACHENAME, Config::filelockCacheBlocksize, Config::filelockCacheFilePath);
    //     std::cerr << "[TAZER] "
    //               << "filelock cache: " << (void *)c << std::endl;
    //     InputFile::_cache->addCacheLevel(c, ++level);
    // }

    // if (Config::useFilelockCache) {
    //     c = FcntlCache::addNewFcntlCache(FCNTLCACHENAME, Config::filelockCacheBlocksize, Config::filelockCacheFilePath);
    //     std::cerr << "[TAZER] "
    //               << "filelock cache: " << (void *)c << std::endl;
    //     InputFile::_cache->addCacheLevel(c, ++level);
    // }

    if (Config::useLocalFileCache) {
        c = LocalFileCache::addNewLocalFileCache(LOCALFILECACHENAME);
        std::cerr << "[TAZER] "
                  << "local file cache: " << (void *)c << std::endl;
        InputFile::_cache->addCacheLevel(c, ++level);
    }

    if (Config::useNetworkCache) {
        c = NetworkCache::addNewNetworkCache(NETWORKCACHENAME, InputFile::_transferPool, InputFile::_decompressionPool);
        std::cerr << "[TAZER] "
                  << "net cache: " << (void *)c << std::endl;
        InputFile::_cache->addCacheLevel(c, ++level);
    }

    //TODO: think about the right way to terminate these (do we even need to or just let the OS destroy when the application exits?)
    InputFile::_transferPool.initiate();
    InputFile::_decompressionPool.initiate();
}

InputFile::InputFile(std::string name, std::string metaName, int fd, bool openFile) : TazerFile(TazerFile::Type::Input, name, metaName, fd),
                                                                                      _fileSize(0),
                                                                                      _numBlks(0),
                                                                                      _regFileIndex(id()),
                                                                                      _prefetcher(NULL) { //This is if there is no file cache...
    std::call_once(init_flag, InputFile::cache_init);
    if (openFile) {
        open();
    }

    //TODO: Move to factory class?
    //IF prefetching is enabled per file (== PERFILE), we use the prefetcher policy specified in _prefetch.
    //Otherwise, we use the global policy specified in prefetcherType
    if (Config::prefetcherType != PERFILE) {
        _prefetch = Config::prefetcherType;
    }

    switch (_prefetch) {
    case NONE:
        //_prefetcher = NULL;
        log(this) << "[TAZER] "
                  << "No prefetcher" << std::endl;
        break;
    case DELTA:
        _prefetcher = new DeltaPrefetcher("DELTAPREFETCHER");
        log(this) << "[TAZER] "
                  << "DELTA prefetcher" << std::endl;
        break;
    case PERFECT:
        _prefetcher = new PerfectPrefetcher("PERFECTPREFETCHER", name);
        log(this) << "[TAZER] "
                  << "Perfect prefetcher" << std::endl;
        break;
    default:
        std::cerr << "[TAZER] "
                  << "Prefetcher doesn't exist" << std::endl;
        exit(-1);
    }
}

InputFile::~InputFile() {
    log(this) << "Destroying file " << _metaName << std::endl;
    close();
}

void InputFile::open() {
    //   std::cout << "[TAZER] "
    //             << "InputFile: " << _connections.size() << std::endl;
    if (_connections.size()) {
        std::unique_lock<std::mutex> lock(_openCloseLock);
        bool prev = false;
        if (_active.compare_exchange_strong(prev, true)) {

            bool created;
            NetworkCache *nc = (NetworkCache *)_cache->getCacheByName(NETWORKCACHENAME);
            // std::cout << "[TAZER] init meta time: " << _initMetaTime << std::endl;
            nc->stats.addTime(false, CacheStats::Metric::ovh, _initMetaTime);
            nc->stats.start();
            ConnectionPool *pool = ConnectionPool::addNewConnectionPool(_name, _compress, _connections, created);
            _fileSize = pool->openFileOnAllServers();
            nc->stats.end(false, CacheStats::Metric::ovh);
            if (_fileSize) {
                _blkSize = Config::memoryCacheBlocksize;
                if (_fileSize < _blkSize)
                    _blkSize = _fileSize;

                _numBlks = _fileSize / _blkSize;
                if (_fileSize % _blkSize != 0)
                    _numBlks++;

                FileCacheRegister *reg = FileCacheRegister::openFileCacheRegister();
                if (reg) {
                    _regFileIndex = reg->registerFile(_name);
                    _cache->addFile(_regFileIndex, _name, _blkSize, _fileSize);

                    if (nc) {
                        nc->setFileCompress(_regFileIndex, _compress);
                        nc->setFileConnectionPool(_regFileIndex, pool);
                    }
                }

                DPRINTF("REG: %p\n", reg);
            }
            else { //We failed to open the file
                _active.store(false);
                std::cout << "failed to open file" << _name << std::endl;
            }
        }
        lock.unlock();
    }
    else {
        log(this) << "ERROR: " << _name << " has no connections!" << std::endl;
    }
    // std::cout << "done open: " << _name << " " << servers_requested << " " << _transferPool.numTasks() << std::endl;
}

//Close doesn't really do much, we hedge our bet that we will likey reopen the file thus we keep our connections to the servers active...
void InputFile::close() {
    std::unique_lock<std::mutex> lock(_openCloseLock);
    bool prev = true;
    if (_active.compare_exchange_strong(prev, false)) {
        DPRINTF("CLOSE: _FC: %p _BC: %p\n", _fc, _bc);
    }
    lock.unlock();
    // std::cout << "Closing file " << _name << std::endl;
}

uint64_t InputFile::copyBlock(char *buf, char *blkBuf, uint32_t blk, uint32_t startBlock, uint32_t endBlock, uint32_t fpIndex, uint64_t count) {
    uint32_t blkOneSize = _blkSize - (_filePos[fpIndex] % _blkSize);
    uint32_t localStart = ((blk - startBlock - 1) * _blkSize) + blkOneSize;
    uint32_t startFP = 0;
    uint32_t localSize = _blkSize;

    if (blk == startBlock && blk + 1 == endBlock) {
        localStart = 0;
        startFP = (_filePos[fpIndex] % _blkSize);
        localSize = count;
    }
    else if (blk == startBlock) {
        localStart = 0;
        startFP = (_filePos[fpIndex] % _blkSize);
        localSize -= startFP;
    }
    else if (blk + 1 == endBlock) {
        uint32_t temp = (_filePos[fpIndex] + count) % _blkSize;
        if (temp)
            localSize = temp;
    }
    // std::cout << "localstart: " << localStart << " startFP: " << startFP << " localsize: " << localSize << std::endl;
    memcpy(&buf[localStart], &blkBuf[startFP], localSize);
    return localSize;
}

bool InputFile::trackRead(size_t count, uint32_t index, uint32_t startBlock, uint32_t endBlock) {
    if (Config::TrackReads) {
        unixopen_t unixopen = (unixopen_t)dlsym(RTLD_NEXT, "open");
        unixclose_t unixclose = (unixclose_t)dlsym(RTLD_NEXT, "close");
        unixwrite_t unixwrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");

        int fd = (*unixopen)("access_new.txt", O_WRONLY | O_APPEND | O_CREAT, 0660);
        if (fd != -1) {
            std::stringstream ss;
            ss << _name << " " << _filePos[index] << " " << count << " " << startBlock << " " << endBlock << std::endl;
            unixwrite(fd, ss.str().c_str(), ss.str().length());
            unixclose(fd);
            return true;
        }
    }
    return false;
}

ssize_t InputFile::read(void *buf, size_t count, uint32_t index) {
    if (_active.load() && _numBlks) {
        _cache->stats.start(); // "read" timer
        _cache->stats.start(); // "hit"  timer
        _cache->stats.start(); // "ovh" timer

        if (_filePos[index] >= _fileSize) {
            log(this) << "[TAZER] " << _name << " " << _filePos[index] << " " << _fileSize << " " << count << std::endl;
            _eof = true;
            _cache->stats.end(false, CacheStats::Metric::ovh);
            _cache->stats.end(false, CacheStats::Metric::hits);
            _cache->stats.end(false, CacheStats::Metric::read);
            return 0;
        }

        char *localPtr = (char *)buf;
        if ((uint64_t)count > _fileSize - _filePos[index]) {
            count = _fileSize - _filePos[index];
        }

        uint32_t startBlock = _filePos[index] / _blkSize;
        uint32_t endBlock = ((_filePos[index] + count) / _blkSize);

        if (((_filePos[index] + count) % _blkSize)) {
            endBlock++;
        }
        if (endBlock > _numBlks) {
            endBlock = _numBlks;
        }
        // std::cerr << "[TAZER] " << Timer::printTime() << _name << " " << _filePos[index] << " " << _fileSize << " " << count << " " << startBlock << " " << endBlock << std::endl;

        trackRead(count, index, startBlock, endBlock);
        bool error = false;
        std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> reads;
        std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> net_reads;
        std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> local_reads;
        uint64_t priority = 0;
        for (uint32_t blk = startBlock; blk < endBlock; blk++) {
            _cache->stats.end(false, CacheStats::Metric::ovh);
            auto request = _cache->requestBlock(blk, _blkSize, _regFileIndex, reads, priority);
            _cache->stats.start(); //ovh
            if (request->ready) {  //the block was in a client side cache!!
                // err(this) << "reading block "<<blk<<" from: "<<request->originating->name()<<std::endl;
                auto amt = copyBlock(localPtr, (char *)request->data, blk, startBlock, endBlock, index, count);
                request->originating->stats.addAmt(false, CacheStats::Metric::read, amt);
                _cache->bufferWrite(request);
            }
            else {
                if (request->originating->name() == NETWORKCACHENAME) {
                    net_reads.insert(std::make_pair(blk, reads[blk]));
                }
                else {
                    local_reads.insert(std::make_pair(blk, reads[blk]));
                }
            }
        }

        //If prefetching is enabled for this file
        if (_prefetcher != NULL) {
            //Get list of blocks to be prefetched
            std::vector<uint64_t> blocks = _prefetcher->getBlocks(index, startBlock, endBlock, Config::numPrefetchBlks, _blkSize, _fileSize.load());

            if (!blocks.empty()) {
                _cache->prefetchBlocks(index, blocks, _fileSize.load(), _blkSize, _regFileIndex);
            }
        }

        for (auto it = net_reads.begin(); it != net_reads.end(); ++it) {
            uint32_t blk = (*it).first;

            _cache->stats.end(false, CacheStats::Metric::ovh);
            auto stallTime = Timer::getCurrentTime();

            auto request = (*it).second.get().get(); //need to do two gets cause we cant chain futures properly yet (c++ 2x supposedly)

            _cache->getCacheByName(request->waitingCache)->stats.addTime(0, CacheStats::Metric::stalls,  (Timer::getCurrentTime() - stallTime) - request->retryTime, 1);
            request->originating->stats.addTime(0, CacheStats::Metric::stalled, Timer::getCurrentTime() - stallTime, 1);
            _cache->stats.start(); //ovh
            if (request->ready) {  // hmm what does it mean if this is NULL? do we need to catch and report this?
                // err(this) << "reading block "<<blk<<" from: "<<request->originating->name()<<std::endl;
                auto amt = copyBlock(localPtr, (char *)request->data, blk, startBlock, endBlock, index, count);
                _cache->getCacheByName(request->waitingCache)->stats.addAmt(0, CacheStats::Metric::stalls, amt);
                request->originating->stats.addAmt(false, CacheStats::Metric::stalled, amt);
                _cache->bufferWrite(request);
            }
        }
        for (auto it = local_reads.begin(); it != local_reads.end(); ++it) {
            uint32_t blk = (*it).first;
            _cache->stats.end(false, CacheStats::Metric::ovh);
            auto stallTime = Timer::getCurrentTime();

            auto request = (*it).second.get().get(); //need to do two gets cause we cant chain futures properly yet (c++ 2x supposedly)

            _cache->getCacheByName(request->waitingCache)->stats.addTime(false, CacheStats::Metric::stalls, (Timer::getCurrentTime() - stallTime) - request->retryTime, 1);
            request->originating->stats.addTime(false, CacheStats::Metric::stalled, Timer::getCurrentTime() - stallTime, 1);
            _cache->stats.start(); //ovh
            if (request->ready) {  // hmm what does it mean if this is NULL? do we need to catch and report this?
                // err(this) << "reading block "<<blk<<" from: "<<request->originating->name()<<std::endl;
                auto amt = copyBlock(localPtr, (char *)request->data, blk, startBlock, endBlock, index, count);
                _cache->getCacheByName(request->waitingCache)->stats.addAmt(false, CacheStats::Metric::stalls, amt);
                request->originating->stats.addAmt(false, CacheStats::Metric::stalled, amt);
                _cache->bufferWrite(request);
            }
        }

        _filePos[index] += count;

        _cache->stats.addAmt(false, CacheStats::Metric::hits, _blkSize);
        _cache->stats.addAmt(false, CacheStats::Metric::read, count);
        _cache->stats.end(false, CacheStats::Metric::ovh);
        _cache->stats.end(false, CacheStats::Metric::hits);
        _cache->stats.end(false, CacheStats::Metric::read);
        return count;
    }
    return 0;
}

ssize_t InputFile::write(const void *buf, size_t count, uint32_t index) {
    log(this) << "in InputFile write.... need to implement... exiting" << std::endl;
    exit(-1);
    return 0;
}

uint64_t InputFile::fileSizeFromServer() {
    uint64_t fileSize = 0;
    bool created;
    ConnectionPool *pool = ConnectionPool::addNewConnectionPool(_name, _compress, _connections, created);
    fileSize = pool->openFileOnAllServers();
    return fileSize;
}

uint64_t InputFile::fileSize() {
    if (_active.load() || _fileSize)
        return _fileSize;
    return fileSizeFromServer();
}

//TODO: handle for if there is the local file present...
off_t InputFile::seek(off_t offset, int whence, uint32_t index) {
    // std::cout << "seek: " << _name << " " << offset << " " << whence << " " << index << std::endl;
    switch (whence) {
    case SEEK_SET:
        _filePos[index] = offset;
        break;
    case SEEK_CUR:
        _filePos[index] += offset;
        if (_filePos[index] > _fileSize) {
            _filePos[index] = _fileSize;
        }
        break;
    case SEEK_END:
        _filePos[index] = _fileSize + offset;
        break;
    }

    _eof = false;
    return _filePos[index];
}

void InputFile::printHits() {
}
