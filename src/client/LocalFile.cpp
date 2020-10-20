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
#include "LocalFileCache.h"
#include "MemoryCache.h"
#include "Message.h"
#include "NetworkCache.h"
#include "Request.h"
#include "SharedMemoryCache.h"
#include "Timer.h"
#include "UnixIO.h"
#include "lz4.h"
#include "xxhash.h"
#include "UrlDownload.h"
#include "UrlFileCache.h"
#include "UrlCache.h"
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

std::once_flag local_init_flag;
Cache *LocalFile::_cache = NULL;

void LocalFile::cache_init() {
    int level = 0;
    Cache *c = NULL;

    if (Config::useMemoryCache) {
        c = MemoryCache::addNewMemoryCache(MEMORYCACHENAME, CacheType::privateMemory, Config::memoryCacheSize, Config::memoryCacheBlocksize, Config::memoryCacheAssociativity);
        std::cerr << "[TAZER] " << "mem cache: " << (void *)c << std::endl;
        LocalFile::_cache->addCacheLevel(c, ++level);
    }

    if (Config::useSharedMemoryCache) {
        c = SharedMemoryCache::addNewSharedMemoryCache(SHAREDMEMORYCACHENAME,CacheType::sharedMemory, Config::sharedMemoryCacheSize, Config::sharedMemoryCacheBlocksize, Config::sharedMemoryCacheAssociativity);
        std::cerr << "[TAZER] " << "shared mem cache: " << (void *)c << std::endl;
        LocalFile::_cache->addCacheLevel(c, ++level);
    }

    #ifdef USE_CURL
        if(Config::urlFileCacheOn) {
            c = UrlFileCache::addNewUrlFileCache(URLFILECACHENAME, CacheType::local);
            std::cerr << "[TAZER] " << "Url file cache: " << (void *)c << std::endl;
            LocalFile::_cache->addCacheLevel(c, ++level);
            return;
        }
        else {
            c = UrlCache::addNewUrlCache(URLCACHENAME, CacheType::local, Config::sharedMemoryCacheBlocksize);
            std::cerr << "[TAZER] " << "Url cache: " << (void *)c << std::endl;
            LocalFile::_cache->addCacheLevel(c, ++level);
        }
    #endif
    c = LocalFileCache::addNewLocalFileCache(LOCALFILECACHENAME, CacheType::local);
    std::cerr << "[TAZER] " << "Local file cache: " << (void *)c << std::endl;
    LocalFile::_cache->addCacheLevel(c, ++level);
}

LocalFile::LocalFile(std::string name, std::string metaName, int fd, bool openFile) : TazerFile(TazerFile::Type::Local, name, metaName, fd),
                                                                                      _fileSize(0),
                                                                                      _numBlks(0),
                                                                                      _regFileIndex(id()), 
                                                                                      _url(supportedUrlType(name)) { //This is if there is no file cache...
    std::call_once(local_init_flag, LocalFile::cache_init);
    std::unique_lock<std::mutex> lock(_openCloseLock);
    if(_url)
        _fileSize = sizeUrlPath(name);
    else {
        struct stat sbuf;
        if (stat(_name.c_str(), &sbuf) == 0)
            _fileSize = sbuf.st_size;
    }

    if(_fileSize == (uint64_t) -1)
        std::cout << "Failed to open file " << _name << std::endl;
    else if(openFile)
        open();
    else {
        _active.store(false);
    }
    lock.unlock();
}

LocalFile::~LocalFile() {
    *this << "Destroying file " << _metaName << std::endl;
    close();
}

void LocalFile::open() {
    _blkSize = Config::memoryCacheBlocksize;

    if (_fileSize < _blkSize)
        _blkSize = _fileSize;

    _numBlks = _fileSize / _blkSize;
    if (_fileSize % _blkSize != 0)
        _numBlks++;

    bool prev = false;
    if (_active.compare_exchange_strong(prev, true)) {
        FileCacheRegister *reg = FileCacheRegister::openFileCacheRegister();
        if (reg) {
            _regFileIndex = reg->registerFile(_name);
            _cache->addFile(_regFileIndex, _name, _blkSize, _fileSize);                
        }
    }
}

void LocalFile::close() {
    std::unique_lock<std::mutex> lock(_openCloseLock);
    bool prev = true;
    if (_active.compare_exchange_strong(prev, false)) {
        DPRINTF("CLOSE: _FC: %p _BC: %p\n", _fc, _bc);
        #ifdef USE_CURL
            if(Config::urlFileCacheOn)
                ((UrlFileCache*)(_cache->getCacheByName(URLFILECACHENAME)))->removeFile(_regFileIndex);
            else 
            {
                if(_url) {
                    ((UrlCache*)(_cache->getCacheByName(URLCACHENAME)))->removeFile(_regFileIndex);
                }
                else
                    ((LocalFileCache*)_cache->getCacheByName(LOCALFILECACHENAME))->removeFile(_regFileIndex);
            }
        #else
            ((LocalFileCache*)_cache->getCacheByName(LOCALFILECACHENAME))->removeFile(_regFileIndex);
        #endif 
    }
    lock.unlock();
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

ssize_t LocalFile::read(void *buf, size_t count, uint32_t index) {
    if (_active.load() && _numBlks) {
        if (_filePos[index] >= _fileSize) {
            // std::cerr << "[TAZER]" << _name << " " << _filePos[index] << " " << _fileSize << " " << count << std::endl;
            _eof[index] = true;
            return 0;
        }
        
        char *localPtr = (char *)buf;
        if ((uint64_t)count > _fileSize - _filePos[index])
            count = _fileSize - _filePos[index];

        uint32_t startBlock = _filePos[index] / _blkSize;
        uint32_t endBlock = ((_filePos[index] + count) / _blkSize);

        if (((_filePos[index] + count) % _blkSize))
            endBlock++;

        if (endBlock > _numBlks)
            endBlock = _numBlks;

        std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> reads;
        uint64_t priority = 0;
        for (uint32_t blk = startBlock; blk < endBlock; blk++) {
            auto request = _cache->requestBlock(blk, _blkSize, _regFileIndex, reads, priority);
            if (request->ready) {
                copyBlock(localPtr, (char *)request->data, blk, startBlock, endBlock, index, count);
                _cache->bufferWrite(request);
            }
            else {
                auto request = reads[blk].get().get();
                if (request->ready) {
                    copyBlock(localPtr, (char *)request->data, blk, startBlock, endBlock, index, count);
                    _cache->bufferWrite(request);
                }
                else {
                    std::cout << "Error reading local file: " << _name << " " << _fileSize << std::endl;
                    return 0;
                }
            }
        }
        _filePos[index] += count;
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

off_t LocalFile::seek(off_t offset, int whence, uint32_t index) {
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
    _eof[index] = false;
    return _filePos[index];
}


