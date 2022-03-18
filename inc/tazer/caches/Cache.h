// -*-Mode: C++;-*- // technically C99

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

#ifndef CACHE_H
#define CACHE_H
#include "CacheStats.h"
#include "CacheTypes.h"
#include "Loggable.h"
#include "PriorityThreadPool.h"
#include "ReaderWriterLock.h"
#include "Request.h"
#include "ThreadPool.h"
#include "Timer.h"
#include "Trackable.h"
#include "UnixIO.h"
#include "Histogram.h"
#include <future>
#include <memory>
#include <unordered_set>

#define BASECACHENAME "base"

class Cache : public Loggable, public Trackable<std::string, Cache *> {
  public:
    Cache(std::string name, CacheType type);
    virtual ~Cache();

    // virtual bool writeBlock(uint32_t index, uint64_t size, char *buffer, uint32_t fileIndex, Cache *originating = NULL);
    virtual bool writeBlock(Request *req);

    // Note about the "reads" argument:
    // the key to the map is the block number
    // the value is two chained futures that return the requested block
    // we need two futures because we potentially need to decompress the data, the second future allows the decompression to happen asynchronous without blocking a transfer thread.
    // if we dont decompress, we can simply use a promise for the second future to immediately make the data available.
    //
    // the return value either returns the request block if it is present in any of the client side caches, or it signfies we need to wait for the network requests to finish (aka call get on the futures in "reads").

    Request *requestBlock(uint32_t index, uint64_t &size, uint32_t fileIndex, uint64_t offset, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority);
    virtual void readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority);

    virtual void addCacheLevel(Cache *, uint64_t level = 0);
    virtual Cache *getCacheAtLevel(uint64_t level);
    virtual Cache *getCacheByName(std::string name);
    virtual Cache *getCacheByType(CacheType type);
    virtual Cache *getNextLevel();
    virtual void setLevel(uint64_t level);
    uint32_t numBlocks();

    virtual bool bufferWrite(Request *req);

    double getRequestTime();

    virtual std::string name() { return _name; }
    virtual CacheType type () {return _type; }

    virtual void addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize);
    //void prefetchBlocks(uint32_t index, uint64_t startBlk, uint64_t endBlk, uint64_t numBlks, uint64_t fileSize, uint64_t blkSize, uint64_t regFileIndex);
    void prefetchBlocks(uint32_t index, std::vector<uint64_t> blocks, uint64_t fileSize, uint64_t blkSize, uint64_t regFileIndex);

    CacheStats stats;

  protected:
    virtual void blockSet(uint32_t index, uint32_t fileIndex, uint32_t blockIndex);
    // virtual bool blockReserve(uint32_t index, uint32_t fileIndex, int &reservedIndex, bool prefetch = false);
    virtual void setBase(Cache *base);

    virtual void cleanUpBlockData(uint8_t *data);

    //TODO: merge/reimplement from old cache structure
    // virtual void cleanReservation();

    // virtual void updateIoRate(uint64_t elapsed, uint64_t size);
    void updateRequestTime(uint64_t time);

    void trackBlock(std::string cacheName, std::string action, uint32_t fileIndex, uint32_t  blockIndex, uint64_t priority);
    //-----------------------------------------------------------------------------------

    // std::atomic<uint64_t> _hits;
    // std::atomic<uint64_t> _misses;
    // std::atomic<uint64_t> _pre;
    // std::atomic<uint64_t> _pre_hits;
    // std::atomic<uint64_t> _pre_misses;
    // std::atomic<uint64_t> _pre_pre;

    // std::atomic<uint64_t> _stalls;
    // std::atomic<uint64_t> _stalls_1;
    // std::atomic<uint64_t> _stalls_2;
    // std::atomic<uint64_t> _pre_stalls;
    // std::atomic<uint64_t> _pre_stalls_1;
    // std::atomic<uint64_t> _pre_stalls_2;

    // std::atomic<uint64_t> _readTime;
    // std::atomic<uint64_t> _stallTime_1;
    // std::atomic<uint64_t> _stallTime_2;
    // std::atomic<uint64_t> _hitTime;
    // std::atomic<uint64_t> _missTime;
    // std::atomic<uint64_t> _ovhTime;
    // std::atomic<uint64_t> _ovhTime2;
    // std::atomic<uint64_t> _ovhTime3;
    // std::atomic<uint64_t> _ovhTime4;
    // std::atomic<uint64_t> _misses1;
    // std::atomic<uint64_t> //_dataTime;
    // std::atomic<uint64_t> //_dataAmt;
    // std::atomic<uint64_t> _hitAmt;
    // std::atomic<uint64_t> _stallAmt;
    // std::atomic<uint64_t> _numReads;
    // std::atomic<uint64_t> _lockTime;
    std::atomic<uint64_t> _ioTime;
    std::atomic<uint64_t> _ioAmt;

    std::atomic<uint64_t> *_curIoTime;
    std::atomic<uint64_t> *_curIoAmt;
    std::atomic<uint8_t> *_ioIndex;
    std::atomic<uint8_t> *_ioCnt;
    static const uint8_t _ioWinSize = 100;
    uint64_t *_ioTimes; //[_ioWinSize];
    uint64_t _ioAmts[_ioWinSize];

    CacheType _type;
    std::string _name;
    uint64_t _level;
    Cache *_nextLevel;
    Cache *_base;
    Cache *_lastLevel;

    struct FileEntry {
        std::string name;
        uint64_t blockSize;
        uint64_t fileSize;
        uint64_t hash;
    };
    std::unordered_map<uint32_t, FileEntry> _fileMap;
    bool _shared;
    std::atomic<std::uint64_t> _outstandingWrites;
    bool _terminating;


    friend class Request;
  private:
    
    ThreadPool<std::function<void()>> *_writePool;

    std::mutex _pMutex;
    PriorityThreadPool<std::function<void()>> *_prefetchPool;
    std::unordered_set<std::string> _prefetches;
    //void prefetch(uint32_t index, uint64_t startBlk, uint64_t endBlk, uint64_t numBlocks, uint64_t fileSize, uint64_t blkSize, uint64_t regFileIndex);
    void prefetch(uint32_t index, std::vector<uint64_t> blocks, uint64_t fileSize, uint64_t blkSize, uint64_t regFileIndex);
};

#endif /* CACHE_H */
