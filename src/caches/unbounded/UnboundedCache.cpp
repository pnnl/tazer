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

#include "UnboundedCache.h"
#include "Config.h"
#include "ThreadPool.h"
#include "Timer.h"
#include "xxhash.h"
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <future>
#include <signal.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

UnboundedCache::UnboundedCache(std::string cacheName, CacheType type, uint64_t blockSize) : Cache(cacheName,type),
                                                                            _blockSize(blockSize),
                                                                            _outstanding(0) {

    // std::cout<<"[TAZER] " << "Constructing " << _name << " in UnboundedCache " <<_blockSize<< std::endl;
}

UnboundedCache::~UnboundedCache() {
    std::cout << "[TAZER] "
              << "deleting " << _name << " in UnBoundedcache" << std::endl;
}

bool UnboundedCache::writeBlock(Request* req) {
    // std::cout << "[TAZER] " << _name << " entering write: " << fileIndex << " " << index << " " << size << " " << _blockSize << " " << (void *)buffer << " " << (void *)originating << " " << (void *)_nextLevel << std::dec << std::endl;
    //std::cout << "try " << _name << " fi: " << fileIndex << " i: " << index << std::endl;
    req->time = Timer::getCurrentTime();
    auto index = req->blkIndex;
    auto fileIndex = req->blkIndex;
    bool ret = false;
    if (req->originating == this) {
        // debug()<<_type<<" deleting data "<<req->id<<std::endl;
        delete req;
        ret = true;
    }
    else {
        DPRINTF("beg wb blk: %u out: %u\n", index, _outstanding.load());

        if (req->size <= _blockSize) {
            if (blockWritable(index, fileIndex, true)) {
                setBlockData(req->data, index, req->size, fileIndex);
                blockSet(index, fileIndex, UBC_BLK_AVAIL);
            }
        }
        DPRINTF("end wb blk: %u out: %u\n", index, _outstanding.load());
        if (_nextLevel) {
            ret &= _nextLevel->writeBlock(req);
        }
    }
    return ret;
}


void UnboundedCache::readBlock(Request* req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request*>>> &reads, uint64_t priority) {
    //std::cout << "read " << _name << " fi: " << fileIndex << " i: " << index << std::endl;
    // std::cout << "[TAZER] " << _name << " entering read " << index << " " << size << " " << _blockSize << " " << priority << std::endl;
    // uint64_t otime = Timer::getCurrentTime();
    // uint64_t rtime = otime;
    uint8_t *buff = nullptr;
    auto index = req->blkIndex;
    auto fileIndex = req->fileIndex;
    bool prefetch = priority != 0;

    _lock->readerLock();
    uint64_t blksize = _fileMap[fileIndex].blockSize;
    _lock->readerUnlock();
    if (!req->size) {
        req->size = blksize;
    }
    if (req->size <= _blockSize) {
        if (blockAvailable(index, fileIndex, true)) {
            // if (!prefetch) {
            //     _hits++;
            //     _ovhTime += Timer::getCurrentTime() - otime;
            // }
            // uint64_t htime = Timer::getCurrentTime();
            buff = getBlockData(index, fileIndex);
            // if (!prefetch) {
            //     _hitTime += Timer::getCurrentTime() - htime;
            // }
            // otime = Timer::getCurrentTime();
            req->data=buff;
            req->originating=this;
            req->reservedMap[this]=1;
            req->ready = true;
            req->time = Timer::getCurrentTime() - req->time;
            updateRequestTime(req->time);
        }
        else {
            // if (!prefetch) {
            //     _hits++;
            //     _ovhTime += Timer::getCurrentTime() - otime;
            // }
            // otime = Timer::getCurrentTime();
        }

        if (!buff) { // data not currently present
            // if (!prefetch) {
            //     _misses++;
            // }
            if (blockReserve(index, fileIndex)) { //we reserved block
                // if (!prefetch) {
                //     _ovhTime2 += Timer::getCurrentTime() - otime;
                // }
                // uint64_t mtime = Timer::getCurrentTime();
                _nextLevel->readBlock(req,reads,priority); //try to satisfy read at next level
                // if (!prefetch) {
                //     _missTime += Timer::getCurrentTime() - mtime;
                // }
                // otime = Timer::getCurrentTime();
            }
            else {
                // if (!prefetch) {
                //     _ovhTime2 += Timer::getCurrentTime() - otime;
                // }
                // otime = Timer::getCurrentTime();              //someone else has reserved so we will wait for it to arrive in an async //or should we just try to grab it from any local layer... if its not present at all then we wait.
                
                uint8_t zero = 0;
                uint8_t temp = UBC_BLK_RES;
                // std::cout << _name << "trying to reserve: " << fileIndex << " " << index << (uint32_t)_blkIndex[fileIndex][index] << std::endl;
                bool reserved = _blkIndex[fileIndex][index].compare_exchange_strong(zero, temp); //if we reserved it wait on the file system, otherwise we just check the _blkIndex (in blockAvailable()....)
                // // std::cout << "reserved: " << reserved << " " << _blkIndex[fileIndex][index] << std::endl;
                // if (!reserved) {
                //     res.first = (char *)1; //signal that someone else has reserved it on our node.
                // }
                auto fut = std::async(std::launch::deferred, [this, req, reserved, prefetch] {
                    // if (!prefetch) {
                    //     _stalls_2++;
                    // }
                    // uint64_t stime = Timer::getCurrentTime();
                    //std::cout << fileIndex << " waiting for block " << index << "  to become available... checking FS: " << reserved << " " << (uint32_t)_blkIndex[fileIndex][index] << std::endl;
                    while (!blockAvailable(req->blkIndex, req->fileIndex, reserved)) {
                        sched_yield();
                    }
                    uint8_t *buff = getBlockData(req->blkIndex, req->fileIndex);
                    req->data=buff;
                    req->originating=this;
                    req->reservedMap[this]=1;
                    req->ready = true;
                    req->time = Timer::getCurrentTime()-req->time;
                    updateRequestTime(req->time);
                    std::promise<Request*> prom;
                    auto fut = prom.get_future();
                    prom.set_value(req);

                    // if (!prefetch) {
                    //     _stallTime_2 += Timer::getCurrentTime() - stime;
                    // }
                    return fut.share();
                });
                reads[index] = fut.share();
                
            }
        }
    }
    else {
        std::cerr << "[TAZER] "
                  << "shouldnt be here yet... need to handle" << std::endl;
        debug()<<"EXITING!!!!"<<std::endl;
        raise(SIGSEGV);
    }
    // if (!prefetch) {
    //     _ovhTime += Timer::getCurrentTime() - otime;
    //     _readTime += Timer::getCurrentTime() - rtime;
    // }
}

void UnboundedCache::cleanReservation() {
}

void UnboundedCache::addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize) {
    // std::cout<<"[TAZER] " << "adding file: " << filename << " " << (void *)this << " " << (void *)_nextLevel << std::endl;
    // std::cout << "[TAZER] " << _name << " " << filename << " " << fileSize << " " << blockSize << std::endl;
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
