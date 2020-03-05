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

#include "LocalFile.h"
#include "BlockSizeTranslationCache.h"
#include "BoundedFilelockCache.h"
#include "Cache.h"
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "FcntlCache.h"
#include "FileCache.h"
#include "FileCacheRegister.h"
#include "FilelockCache.h"
#include "MemoryCache.h"
#include "Message.h"
#include "NetworkCache.h"
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

Cache *LocalFile::_cache = NULL; //(BASECACHENAME);

void /*__attribute__((constructor))*/ LocalFile::cache_init(void) {
    int level = 0;
    Cache *c = NULL;

    if (Config::useMemoryCache) {
        Cache *c = MemoryCache::addNewMemoryCache(MEMORYCACHENAME, Config::memoryCacheSize, Config::memoryCacheBlocksize, Config::memoryCacheAssociativity);
        std::cerr << "[TAZER] "
                  << "mem cache: " << (void *)c << std::endl;
        LocalFile::_cache->addCacheLevel(c, ++level);
    }

    if (Config::useSharedMemoryCache) {
        c = SharedMemoryCache::addNewSharedMemoryCache(SHAREDMEMORYCACHENAME, Config::sharedMemoryCacheSize, Config::sharedMemoryCacheBlocksize, Config::sharedMemoryCacheAssociativity);
        std::cerr << "[TAZER] "
                  << "shared mem cache: " << (void *)c << std::endl;
        LocalFile::_cache->addCacheLevel(c, ++level);
    }

    if (Config::LocalFileCache) {
        c = BoundedFilelockCache::addNewLocalFileCache(LOCALFILECACHENAME);
        std::cerr << "[TAZER] "
                  << "filelock cache: " << (void *)c << std::endl;
        LocalFile::_cache->addCacheLevel(c, ++level);
    }

}

LocalFile::LocalFile(std::string fileName, int fd, bool openFile) : TazerFile(TazerFile::Type::Local, fileName, fd),
                                                                    _fileSize(0),
                                                                    _numBlks(0),
                                                                    _regFileIndex(id()) { //This is if there is no file cache...
    std::call_once(init_flag, LocalFile::cache_init);
    if (openFile) {
        open();
    }
}

LocalFile::~LocalFile() {
    *this << "Destroying file " << _metaName << std::endl;
    close();
}

void LocalFile::open() {
    //   std::cout << "[TAZER] "
    //             << "LocalFile: " << _connections.size() << std::endl;
    std::unique_lock<std::mutex> lock(_openCloseLock);
    bool prev = false;
    if (_active.compare_exchange_strong(prev, true)) {
        struct stat sbuf;
        if (stat(_name.c_str(), &sbuf) == 0){
            _fileSize = sbuf.st_size;
            _blkSize = Config::memoryCacheBlocksize;
            if (_fileSize < _blkSize){
                _blkSize = _fileSize;
            }

            _numBlks = _fileSize / _blkSize;
            if (_fileSize % _blkSize != 0){
                _numBlks++;
            }

            FileCacheRegister *reg = FileCacheRegister::openFileCacheRegister();
            if (reg) {
                _regFileIndex = reg->registerFile(_name);
                _cache->addFile(_regFileIndex, _name, _blkSize, _fileSize);                
            }
        }
        else { //We failed to open the file
            _active.store(false);
            std::cout << "failed to open file" << _name << std::endl;
        }
    }
    lock.unlock();
    
    // std::cout << "done open: " << _name << " " << servers_requested << " " << _transferPool.numTasks() << std::endl;
}

//Close doesn't really do much, we hedge our bet that we will likey reopen the file thus we keep our connections to the servers active...
void LocalFile::close() {
    std::unique_lock<std::mutex> lock(_openCloseLock);
    bool prev = true;
    if (_active.compare_exchange_strong(prev, false)) {
        DPRINTF("CLOSE: _FC: %p _BC: %p\n", _fc, _bc);
    }
    if (Config::useLocalFileCache) {
        ((LocalFileCache*)_cache->getCacheByName(LOCALFILECACHENAME))->removeFile(_regFileIndex);
    }
    lock.unlock();
    // std::cout << "Closing file " << _name << std::endl;
}

uint64_t LocalFile::copyBlock(char *buf, char *blkBuf, uint32_t blk, uint32_t startBlock, uint32_t endBlock, uint32_t fpIndex, uint64_t count) {
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

bool LocalFile::trackRead(size_t count, uint32_t index, uint32_t startBlock, uint32_t endBlock) {
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

ssize_t LocalFile::read(void *buf, size_t count, uint32_t index) {
    if (_active.load() && _numBlks) {
        uint64_t otime = Timer::getCurrentTime();
        uint64_t rtime = otime;
        if (_filePos[index] >= _fileSize) {
            std::cerr << "[TAZER]" << _name << " " << _filePos[index] << " " << _fileSize << " " << count << std::endl;
            _eof = true;
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
        uint64_t priority = 0;
        for (uint32_t blk = startBlock; blk < endBlock; blk++) {
            _cache->updateReadCnt(1);
            uint32_t dataSize = (blk + 1 == _numBlks) ? _fileSize - (blk * _blkSize) : _blkSize;
            _cache->updateOvhTime(Timer::getCurrentTime() - otime);

            auto request = _cache->requestBlock(blk, _blkSize, _regFileIndex, reads, priority);
            otime = Timer::getCurrentTime();
            // std::cout << "request blk " << blk << " from: " << request->originating->name() << request->ready << std::endl;

            if (request->ready) { //the block was in a client side cache!! --as this is local this should always happen...
                // std::cout << "blk " << blk << " done from: " << request->originating->name() << std::endl;
                // request->originating->getRequestTime();
                uint64_t tmp = 0;
                uint64_t dtime = Timer::getCurrentTime();
                auto amt = copyBlock(localPtr, (char *)request->data, blk, startBlock, endBlock, index, count);
                _cache->updateDataTime(Timer::getCurrentTime() - dtime);
                request->originating->updateHitAmt(amt);
                //c->updateReadTime(Timer::getCurrentTime() - start_t);
                _cache->bufferWrite(request);
            }
        }

        // std::cout<<"[TAZER] " << "read size" << reads.size() << std::endl;

        //If prefetching is enabled globally or for this specific file, apply specific prefetching policy
        if (Config::prefetchGlobal || _prefetch) {
            //Default: Prefetch next n (numPrefetchBlks) blocks
            //Prefetch gap means that we skip a few blocks: e.g. if Gap=2 and N=5, we will prefetch blocks {3..5}
            if (Config::prefetchNextBlks) {
                _cache->prefetchBlocks(index, endBlock + Config::prefetchGap, endBlock + Config::numPrefetchBlks, _numBlks - Config::prefetchGap, _fileSize.load(), _blkSize, _regFileIndex);
            }
            else if (Config::prefetchAllBlks) {
                _cache->prefetchBlocks(index, 0, _numBlks, _numBlks, _fileSize.load(), _blkSize, _regFileIndex);
            }
        }
        
        _filePos[index] += count;
        _cache->updateReadTime(Timer::getCurrentTime() - rtime);
        _cache->updateDataAmt(count);
        _cache->updateOvhTime(Timer::getCurrentTime() - otime);
        return count;
    }
    return 0;
}

ssize_t LocalFile::write(const void *buf, size_t count, uint32_t index) {
    *this << "in LocalFile write.... need to implement... exiting" << std::endl;
    exit(-1);
    return 0;
}



uint64_t LocalFile::fileSize() {
        return _fileSize;
}

//TODO: handle for if there is the local file present...
off_t LocalFile::seek(off_t offset, int whence, uint32_t index) {
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


