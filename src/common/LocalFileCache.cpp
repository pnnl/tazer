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

#include "LocalFileCache.h"
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "Message.h"
#include "ReaderWriterLock.h"
#include "Timer.h"
#include "lz4.h"
#include "xxhash.h"

#include <chrono>
#include <fcntl.h>
#include <future>
#include <memory>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

LocalFileCache::LocalFileCache(std::string cacheName, CacheType type) : Cache(cacheName,type) {
    stats.start();
    _lock = new ReaderWriterLock();
    stats.end(false,CacheStats::Metric::constructor);
    // //log(this) /*std::cout*/<<"[TAZER] " << "Constructing " << _name << " in network cache" << std::endl;
}

LocalFileCache::~LocalFileCache() {
    ////log(this) /*std::cout*/<<"[TAZER] " << "deleting " << _name << " in network cache" << std::endl;
    delete _lock;
}

uint8_t *LocalFileCache::getBlockData(std::ifstream *file, uint32_t blkIndex, uint64_t blkSize,uint64_t fileSize) {
    uint64_t offset = blkIndex * blkSize;
    uint64_t size = blkSize;
    if ((blkIndex + 1)*blkSize > fileSize){
        size = fileSize - offset;
    }
    file->seekg(offset);
    char *buff = new char[blkSize];
    file->read(buff, size); //TODO error detection?
    return (uint8_t *)buff;
}

bool LocalFileCache::writeBlock(Request *req) {
    bool ret = true;
    // //log(this) /*std::cout*/<<"[TAZER] " << _name << " netcache writing: " << index << " " << (void *)originating << std::endl;
    // //log(this) /*std::cout*/<<"[TAZER] "<<_name<<" writeblock "<<std::hex<<(void*)buffer<<std::dec<<std::endl;
    if (req->originating == this) {
        delete[] req->data;
        delete req;
    }
    else if (_nextLevel) {
        ret &= _nextLevel->writeBlock(req);
    }
    return ret;
}

void LocalFileCache::readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority) {
    stats.start(); //read
    stats.start(); //ovh
    log(this) << _name << " entering read " << req->blkIndex << " " << req->fileIndex << " " << priority << std::endl;
    req->time = Timer::getCurrentTime();
    req->originating = this;

    bool prefetch = priority != 0;
    auto blkIndex = req->blkIndex;
    auto fileIndex = req->fileIndex;

    _lock->readerLock();
    auto fileSize = _fileMap[fileIndex].fileSize;
    auto name = _fileMap[fileIndex].name;
    // auto blkSize = _fileMap[fileIndex].blockSize;
    auto file = _fstreamMap[fileIndex];
    _lock->readerUnlock();

    std::uint64_t memblkSize = Config::localFileBlockSize;
    if (file.first) { //we have an open file stream! (represents a hit)
        if (!prefetch) {
            stats.addAmt(prefetch, CacheStats::Metric::hits, req->size);
        }
        else {
            stats.addAmt(prefetch, CacheStats::Metric::prefetches, 1);
        }
        // uint64_t htime = Timer::getCurrentTime();
        stats.end(prefetch, CacheStats::Metric::ovh);
        stats.start(); //hits
        file.second->writerLock();
        uint8_t *buff = getBlockData(file.first, blkIndex, memblkSize,fileSize);
        file.second->writerUnlock();
        stats.end(prefetch, CacheStats::Metric::hits);
        stats.start(); //ovh
        req->data = buff;
        req->originating = this;
        req->reservedMap[this] = 1;
        req->ready = true;
        req->time = Timer::getCurrentTime() - req->time;
        updateRequestTime(req->time);
    }
    else {
        stats.addAmt(prefetch, CacheStats::Metric::misses, 1);
        
        req->time = Timer::getCurrentTime() - req->time;
        updateRequestTime(req->time);
        stats.end(prefetch, CacheStats::Metric::ovh);
        stats.start(); //misses
        _nextLevel->readBlock(req, reads, priority);
        stats.end(prefetch, CacheStats::Metric::misses);
        stats.start(); //ovh
    }
    stats.end(prefetch, CacheStats::Metric::ovh);
    stats.end(prefetch, CacheStats::Metric::read);
}

Cache *LocalFileCache::addNewLocalFileCache(std::string cacheName, CacheType type) {
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [&]() -> Cache * {
            Cache *temp = new LocalFileCache(cacheName, type);
            return temp;
        });
}

void LocalFileCache::addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize) {
    // //log(this) /*std::cout*/<<"[TAZER] " << "adding file: " << filename << " " << (void *)this << " " << (void *)_nextLevel << std::endl;
    // //log(this) /*std::cout*/ << "[TAZER] " << _name << " " << filename << " " << fileSize << " " << blockSize << std::endl;
    _lock->writerLock();
    if (_fileMap.count(index) == 0) {
        std::string hashstr(_name + filename); //should cause each level of the cache to have different indicies for a given file
        uint64_t hash = (uint64_t)XXH32(hashstr.c_str(), hashstr.size(), 0);
        // bool compress = false;

        _fileMap.emplace(index, FileEntry{filename, blockSize, fileSize, hash});
        std::ifstream *file = new std::ifstream();
        file->open(filename, std::fstream::binary);
        if (!file->is_open()) {
            log(this) << "WARNING: " << filename << " did not open" << std::endl;
            _fstreamMap.emplace(index, std::make_pair((std::ifstream *)NULL, (ReaderWriterLock *)NULL));
        }
        else {
            _fstreamMap.emplace(index, std::make_pair(file, new ReaderWriterLock()));
        }

        //uint64_t temp = _fileMap[index];
    }
    _lock->writerUnlock();
    if (_nextLevel) {
        _nextLevel->addFile(index, filename, blockSize, fileSize);
    }
}

void LocalFileCache::removeFile(uint32_t index){
    _lock->writerLock();
    auto file = _fstreamMap[index];
    _fstreamMap.erase(index);
    _fileMap.erase(index);
    file.first->close();
    delete file.first;
    delete file.second;
    _lock->writerUnlock();
}
