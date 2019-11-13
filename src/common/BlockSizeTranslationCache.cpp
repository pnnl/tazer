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

#include "BlockSizeTranslationCache.h"
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "Message.h"
#include "ReaderWriterLock.h"
#include "Timer.h"
#include "lz4.h"
#include "xxhash.h"

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <future>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

//single process, but shared across threads
BlockSizeTranslationCache::BlockSizeTranslationCache(std::string cacheName, uint64_t blockSize1, uint64_t blockSize2) : Cache(cacheName),
                                                                                                                        _blockSize1(blockSize1),
                                                                                                                        _blockSize2(blockSize2) {
    // std::cout << "[TAZER] "
    //           << "Constructing " << _name << " in blk size trans cache" << std::endl;
    if (_blockSize2 % _blockSize1) {
        std::cerr << "[TAZER]" << _name << " :" << _blockSize2 << " is not a multiple of " << _blockSize1 << std::endl;
    }
    _b1Perb2 = _blockSize2 / _blockSize1;
    _lock = new ReaderWriterLock();
}

BlockSizeTranslationCache::~BlockSizeTranslationCache() {
    delete _lock;
}

bool BlockSizeTranslationCache::writeBlock(Request *req) {
    uint32_t newIndex = req->blkIndex / _b1Perb2;
    uint32_t offset = req->blkIndex % _b1Perb2;

    req->data = req->data - offset * _blockSize1; //undo the offset into larger block;
    req->blkIndex = newIndex;
    _nextLevel->writeBlock(req);
    return true;
}

void BlockSizeTranslationCache::readBlock(Request* req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request*>>> &reads, uint64_t priority){

    // std::cout<<"[TAZER] " << _name << " entering read " << index << " " << size << std::endl;
    uint32_t newIndex = req->blkIndex / _b1Perb2;
    uint32_t offset = req->blkIndex % _b1Perb2;

    req->blkIndex=newIndex;

    std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request*>>> treads;
    _nextLevel->readBlock(req,treads,priority);
    // std::cout<<"[TAZER] " << _name << " " << treads.size() << " new index " << newIndex << " offset " << offset << std::endl;
    if (req->data != NULL) {
        // _hits++;
        req->data += (offset)*_blockSize1; //point to correct offset in larger block;
    }
    else {
        // _stalls++;
        //std::future<std::future<Request*>> task; // = treads[0];

        // auto fut = std::async(std::launch::deferred, [this, task = std::move(treads[index]), offset, index, newIndex]() mutable {
        //     // std::cout<<"[TAZER] " << _name << " " << treads.size() << std::endl;
        //     //std::vector<std::future<std::future<Request*>>> reads;
        //     // std::cout<<"[TAZER] " << _name << " waiting " << offset << std::endl;
        //     std::promise<Request*> prom;
        //     auto fut = prom.get_future();
        //     auto res = task.get().get();
        //     // std::cout<<"[TAZER] " << _name << " done waiting" << (void *)res.first <<" "<<(void *)(res.first+_blockSize2)<< " " << (void *)(res.first + offset * _blockSize1) << std::endl;
        //     // std::cout<<"[TAZER] " <<  index<<" "<<newIndex<<" "<<offset<<" "<<offset*_blockSize1<<std::endl;
        //     res.first += (offset)*_blockSize1; //point to correct offset in larger block;
        //     prom.set_value(res);
        //     return fut;
        // });
        // reads[index] = std::move(fut);

        // std::cout<<"[TAZER] " << _name << " pushed future for index: " << index << std::endl;
    }
    // std::cout<<"[TAZER] " << _name << " leaving read" << std::endl;
    // return res;
}

Cache *BlockSizeTranslationCache::addNewBlockSizeTranslationCache(std::string cacheName, uint64_t blockSize1, uint64_t blockSize2) {
    // std::string newFileName("BlockSizeTranslationCache");
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [&]() -> Cache * {
            Cache *temp = new BlockSizeTranslationCache(cacheName, blockSize1, blockSize2);
            return temp;
        });
}
