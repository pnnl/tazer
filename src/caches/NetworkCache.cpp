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

#include "NetworkCache.h"
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
#include <signal.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

NetworkCache::NetworkCache(std::string cacheName, CacheType type, PriorityThreadPool<std::packaged_task<std::shared_future<Request *>()>> &txPool, PriorityThreadPool<std::packaged_task<Request *()>> &decompPool) : Cache(cacheName,type),
                                                                                                                                                                                                      _transferPool(txPool),
                                                                                                                                                                                                      _decompPool(decompPool) {
    std::thread::id thread_id = std::this_thread::get_id();
    stats.checkThread(thread_id, true);
    stats.start(false, CacheStats::Metric::constructor, thread_id);

    _lock = new ReaderWriterLock();
    
    stats.end(false, CacheStats::Metric::constructor, thread_id);
    // //log(this) /*std::cout*/<<"[TAZER] " << "Constructing " << _name << " in network cache" << std::endl;
}

NetworkCache::~NetworkCache() {
    ////log(this) /*std::cout*/<<"[TAZER] " << "deleting " << _name << " in network cache" << std::endl;
    std::thread::id thread_id = std::this_thread::get_id();
    stats.checkThread(thread_id, true);
    stats.start(false, CacheStats::Metric::destructor, thread_id);
    delete _lock;
    stats.end(false, CacheStats::Metric::destructor, thread_id);
    stats.print(_name);
    // std::cout << std::endl;
}

void NetworkCache::cleanUpBlockData(uint8_t *data){
    // debug()<<_name<<" delete data"<<std::endl;
    delete[] data;
}

bool NetworkCache::writeBlock(Request *req) {
    bool ret = true;
    req->trace(_name)<<"WRITE BLOCK"<<std::endl;
    // debug()
    // //log(this) /*std::cout*/<<"[TAZER] " << _name << " netcache writing: " << index << " " << (void *)originating << std::endl;
    // //log(this) /*std::cout*/<<"[TAZER] "<<_name<<" writeblock "<<std::hex<<(void*)buffer<<std::dec<<std::endl;
    if (req->originating == this) {
        // req->trace(_name)<<_type<<" deleting data "<<req->id<<std::endl;
        req->printTrace=false; 
        delete req;
    }
    else if (_nextLevel) { // currentlty this should be the last level....but it could be possible to do something like a level for site level servers and then a level for remote site servers...
        ret &= _nextLevel->writeBlock(req);
    }
    return ret;
}

Request *NetworkCache::decompress(Request *req, char *compBuf, uint32_t compBufSize, uint32_t blkBufSize, uint32_t blk) {
    char *blkBuf = new char[blkBufSize];
    int64_t ret = LZ4_decompress_safe(compBuf, blkBuf, compBufSize, blkBufSize);
    if (ret < 0) {
        //*this << "A negative result from LZ4_decompress_fast indicates a failure trying to decompress the data.  See exit code (echo $?) for value returned. " << ret << std::endl;
        delete blkBuf;
        blkBuf = NULL;
    }
    delete compBuf;
    req->data = (uint8_t *)blkBuf;
    req->originating = this;
    req->reservedMap[this] = 1;
    req->ready = true;
    req->time = Timer::getCurrentTime() - req->time;
    req->waitingCache = CacheType::network;
    updateRequestTime(req->time);
    return req;
}

// std::future<Request*> NetworkCache::requestBlk(Connection *server, uint32_t blkStart, uint32_t blkEnd, uint32_t fileIndex, uint32_t priority) {
std::future<Request *> NetworkCache::requestBlk(Connection *server, Request *req, uint32_t priority, bool &success) {
    stats.checkThread(req->threadId, true);
    auto fileIndex = req->fileIndex;
    auto blkStart = req->blkIndex;
    auto blkEnd = blkStart;
    // req->trace+=", requesting";
    // std::cerr << "[TAZER] " << fileIndex << " " << _fileMap[fileIndex].name << " Requesting blks: " << blkStart << " " << blkEnd << " " << Config::networkBlockSize << std::dec << std::endl;
    // bool success = false;
    server->lock();
    bool compress = _compressMap[fileIndex];
    req->trace(_name)<<"requesting block from: "<<server->addrport()<<std::endl;
    //Currently we are only ever getting a single block at a time (blkStart == blkEnd)
    //That makes this reasonable... A better idea is to keep track of what block succeeded
    //And only re-request those that failed...
    std::uint64_t _blkSize = Config::networkBlockSize; //_fileMap[fileIndex].blockSize;
    _lock->readerLock();
    std::uint64_t _fileSize = _fileMap[fileIndex].fileSize;

    std::string name = _fileMap[fileIndex].name;
    _lock->readerUnlock();
    std::future<Request *> fut;
    // auto start = Timer::getCurrentTime();
    // uint64_t sizeSum = 0;
    for (uint32_t j = 0; j < Config::socketRetry; j++) {
        if (sendRequestBlkMsg(server, name, blkStart, blkEnd)) {
            for (uint32_t i = blkStart; i <= blkEnd; i++) {
                req->trace(_name) << " requesting block " << i <<" try: "<<j <<" of "<< Config::socketRetry<< std::endl;
                uint64_t start = blkStart * _blkSize;
                uint64_t end;
                if (blkEnd * _blkSize > _fileSize) {
                    end = _fileSize;
                }
                else {
                    end = start + _blkSize;
                }
                uint64_t size = end - start;

                /*Rec a block is currently a blocking call so the client will hang
                 * until it gets something*/
                //                char * data = (_compress) ? NULL : _fileContent.data() + start;
                char *data = NULL;
                uint32_t blk = 0, dataSize = 0;
                std::string fileName = recSendBlkMsg(server, &data, blk, dataSize, _blkSize);
                debug(this) << fileName << " " << dataSize << " " << blk << std::endl;
                if (data == NULL) {
                    debug(this) << "data null: " << fileName << " " << dataSize << " " << blk << std::endl;
                    success = false;
                    break;
                    // raise(SIGSEGV);
                    // exit(0);
                }
                if (!fileName.empty() && dataSize && blk == i) { //Got a block
                    // sizeSum += dataSize;
                    req->trace(_name) << "Received " << fileName << " " << blk << " " << dataSize << " allocSize " << size << std::endl;
                    if (compress) {

                        auto task = std::packaged_task<Request *()>([this, req, data, dataSize, size, blk, priority]() {
                            stats.start(priority != 0, CacheStats::Metric::hits, req->threadId);
                            return decompress(req, data, dataSize, size, blk);
                            stats.end(priority != 0, CacheStats::Metric::hits, req->threadId);

                        });
                        fut = task.get_future();
                        _decompPool.addTask(priority, std::move(task));
                    }
                    else {
                        // log(this) << _name << " received block " << blk << std::endl; // << " " << std::hex << (void *)data << " " << dataSize << " " << (void *)(data + dataSize) << " " << std::dec << XXH64(data, dataSize, 0) << std::dec << std::endl;
                        std::promise<Request *> prom;
                        fut = prom.get_future();
                        req->data = (uint8_t *)data;
                        req->originating = this;
                        req->reservedMap[this] = 1;
                        req->ready = true;
                        req->time = Timer::getCurrentTime() - req->time;
                        req->waitingCache = CacheType::network;
                        updateRequestTime(req->time);
                        prom.set_value(req);
                    }
                    success = true;
                }
                else { //Failed to get a block
                    err(this) << "failed to get block: " << blk << std::endl;
                    success = false;
                    break;
                }
            }
            if (success) { //We got all the blocks
                break;
            }
        }
    }
    // auto elapsed = Timer::getCurrentTime() - start;
    // updateIoRate(elapsed, sizeSum);
    server->unlock();
    return fut;
}

void NetworkCache::readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority) {
    std::thread::id thread_id = req->threadId;
    stats.checkThread(thread_id, true);
    stats.start((priority != 0), CacheStats::Metric::read, thread_id); //read
    stats.start((priority != 0), CacheStats::Metric::ovh, thread_id); //ovh
    stats.start((priority != 0), CacheStats::Metric::hits, thread_id); //hits
    // std::cout << _name << " entering read " << req->blkIndex << " " << req->fileIndex << " " << priority << std::endl;
    // req->trace+=_name+":";
    req->printTrace=false;
    req->globalTrigger=false;
    req->time = Timer::getCurrentTime();
    req->originating = this;
    req->trace(_name)<<" READ BLOCK"<<std::endl;
    bool prefetch = priority != 0;
    auto task = std::packaged_task<std::shared_future<Request *>()>([this, req, priority, prefetch] { //packaged task allow the transfer to execute on an asynchronous tx thread.
        Connection *sev = NULL;
        _lock->readerLock();
        ConnectionPool* pool = _conPoolMap[req->fileIndex];
        _lock->readerUnlock();
        if (!sev) {
            while (!sev) {
                sev = pool->popConnection();
            }
        }
        bool success = false;
        auto fut = requestBlk(sev, req, priority, success);
        int retryCnt = 0;
        while (!success && retryCnt < 100) {

            Connection *oldSev = sev;
            sev = NULL;
            while (!sev) {
                sev = pool->popConnection();
            }
            err(this) << "retrying to request block old: " << oldSev->addrport() << " " << sev->addrport() << std::endl;
            req->trace(_name)<< "retrying to request block old: " << oldSev->addrport() << " " << sev->addrport() << std::endl;
            pool->pushConnection(oldSev, true); //TODO: determine if oldSev is still a viable connection
            success = false;
            fut = requestBlk(sev, req, priority, success);
            retryCnt++;
        }
        pool->pushConnection(sev, true);
        stats.addAmt(prefetch, CacheStats::Metric::hits, req->size, req->threadId);
        // req->trace+=", received->";
        return fut.share();
    });
    auto fut = task.get_future();
    _transferPool.addTask(priority, std::move(task));
    reads[req->blkIndex] = fut.share();

    stats.end(prefetch, CacheStats::Metric::hits, thread_id);
    stats.end(prefetch, CacheStats::Metric::ovh, thread_id);
    stats.end(prefetch, CacheStats::Metric::read, thread_id);
}

void NetworkCache::setFileCompress(uint32_t index, bool compress) {
    //if (_compressMap.count(index) == 0) {
    _lock->writerLock();
    _compressMap[index] = compress;
    _lock->writerUnlock();
    //}
}

void NetworkCache::setFileConnectionPool(uint32_t index, ConnectionPool *conPool) {
    _lock->writerLock();
    _conPoolMap[index] = conPool;
    _lock->writerUnlock();
}

Cache *NetworkCache::addNewNetworkCache(std::string cacheName, CacheType type, PriorityThreadPool<std::packaged_task<std::shared_future<Request *>()>> &txPool, PriorityThreadPool<std::packaged_task<Request *()>> &decompPool) {
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [&]() -> Cache * {
            Cache *temp = new NetworkCache(cacheName, type, txPool, decompPool);
            return temp;
        });
}

void NetworkCache::addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize) {

    // //log(this) /*std::cout*/<<"[TAZER] " << "adding file: " << filename << " " << (void *)this << " " << (void *)_nextLevel << std::endl;
    // //log(this) /*std::cout*/ << "[TAZER] " << _name << " " << filename << " " << fileSize << " " << blockSize << std::endl;
    _lock->writerLock();
    if (_fileMap.count(index) == 0) {
        std::string hashstr(_name + filename); //should cause each level of the cache to have different indicies for a given file
        uint64_t hash = (uint64_t)XXH32(hashstr.c_str(), hashstr.size(), 0);

        _fileMap.emplace(index, FileEntry{filename, blockSize, fileSize, hash});

        //uint64_t temp = _fileMap[index];
    }
    _lock->writerUnlock();
    if (_nextLevel) {
        _nextLevel->addFile(index, filename, blockSize, fileSize);
    }
}
