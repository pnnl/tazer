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

#ifndef BoundedCache_H
#define BoundedCache_H
#include "Cache.h"
#include "../scalable/ScalableCache.h"
#include "Loggable.h"
#include "ReaderWriterLock.h"
#include "Trackable.h"
#include "UnixIO.h"
#include <future>
#include <memory>

#define BLK_EMPTY 0
#define BLK_PRE 1
#define BLK_RES 2
#define BLK_WR 3
#define BLK_AVAIL 4
#define BLK_EVICT 5

// const uint64_t MAX_CACHE_NAME_LEN = 30;

// struct dummy {};
template <class Lock>
class BoundedCache : public Cache {
  public:

    BoundedCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, Cache * scalableCache=NULL);
    virtual ~BoundedCache();

    virtual bool writeBlock(Request *req);
    virtual void readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority);
    virtual void addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize);

    //TODO: merge/reimplement from old cache structure...
    virtual void cleanReservation();

  protected:
    struct BlockEntry {
        uint32_t id;
        uint32_t fileIndex;
        uint32_t blockIndex;
        uint64_t timeStamp;
        uint32_t status;
        uint32_t prefetched;
        // char origCache[32];
        std::atomic<CacheType> origCache; 
        // this member is atomic because blockAvailable calls are not protected by locks
        // we know that when calling blockAvailable the actual data cannot change during the read (due to refernce counting protections)
        // what can change is the meta data associated with the last access of the data (and which cache it originated from)
        // in certain cases, we want to capture the originating cache and return it for performance reasons
        // there is a potential race between when a block is available and when we capture the originating cache
        // having origCache be atomic ensures we always get a valid ID (even though it may not always be 100% acurate)
        // we are willing to accept some small error in attribution of stats
        void init(BoundedCache* c,uint32_t entryId){
          // memset(this,0,sizeof(BlockEntry));
          id=entryId;
          fileIndex =0;
          blockIndex = 0;
          timeStamp = 0;
          status = 0;
          prefetched= 0;
          std::atomic_init(&origCache,CacheType::empty);
        }
        BlockEntry& operator= (const BlockEntry &entry){
          if (this == &entry){
            return *this;
          }
          id = entry.id;
          fileIndex = entry.fileIndex;
          blockIndex = entry.blockIndex;
          timeStamp = entry.timeStamp;
          status = entry.status;
          prefetched= entry.prefetched;
          origCache.store(entry.origCache.load());
          
          return *this;
        }
        std::string str(){
          std::stringstream ss;
          ss<<"addr: "<< this <<" id: "<<id<<" fi: "<<fileIndex<<" bi: "<<blockIndex<<" status: "<<status<<" oc: "<<cacheTypeName(origCache);
          return ss.str();
        }
        // std::atomic<uint32_t> activeCnt;
    };

    // virtual BlockEntry* blockReserve(uint32_t index, uint32_t fileIndex, Request* req, bool prefetch = false);
    virtual BlockEntry* getBlock(uint32_t index, uint32_t fileIndex, Request* req);
    virtual BlockEntry* oldestBlock(uint32_t index, uint32_t fileIndex, Request* req);

    

    virtual uint32_t getBinIndex(uint32_t index, uint32_t fileIndex);
    virtual uint32_t getBinOffset(uint32_t index, uint32_t fileIndex);

    //TODO: eventually when we create a flag for stat keeping we probably dont need to store the cacheName...
    virtual void blockSet(BlockEntry* blk, uint32_t fileIndex, uint32_t blockIndex, uint8_t byte,  CacheType type, int32_t prefetch, int32_t activeUpdate,Request* req) = 0;
    virtual bool blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs = false, uint32_t cnt = 0, CacheType *origCache = NULL) = 0;

    virtual void cleanUpBlockData(uint8_t *data);
    // virtual char *blockMiss(uint32_t index, uint64_t &size, uint32_t fileIndex, std::unordered_map<uint64_t,std::future<std::future<Request*>>> &reads);
    virtual uint8_t *getBlockData(uint32_t blockIndex) = 0;
    virtual void setBlockData(uint8_t *data, uint32_t blockIndex, uint64_t size) = 0;
    virtual BlockEntry* getBlockEntry(uint32_t blockIndex, Request* req) = 0;
    virtual std::vector<BlockEntry*> readBin(uint32_t binIndex) = 0;
    virtual std::string blockEntryStr(BlockEntry *entry) = 0;
   
    virtual int incBlkCnt(BlockEntry * entry, Request* req) = 0;
    virtual int decBlkCnt(BlockEntry * entry, Request* req) = 0;
    virtual bool anyUsers(BlockEntry * entry, Request* req) = 0;

    // virtual void getCompareBlkEntry(uint32_t index, uint32_t fileIndex, BlockEntry *entry);

    virtual std::shared_ptr<BlockEntry> getCompareBlkEntry(uint32_t index, uint32_t fileIndex);

    virtual bool sameBlk(BlockEntry *blk1, BlockEntry *blk2);

    uint64_t _cacheSize;
    uint64_t _blockSize;
    uint32_t _associativity;
    uint32_t _numBlocks;
    uint32_t _numBins;
    std::atomic<uint64_t> _collisions;
    std::atomic<uint64_t> _prefetchCollisions;
    std::atomic_uint _outstanding;

    std::unordered_map<uint64_t, uint64_t> _blkMap;

    // T *_lock;
    Lock *_binLock;
    ReaderWriterLock *_localLock;

    //JS: This is just temp for checking which files had blocks evicted
    Histogram evictHisto;

    //JS: Metric piggybacking
    Cache * _scalableCache;
    virtual double getLastUMB(uint32_t fileIndex);
    virtual void setLastUMB(std::vector<std::tuple<uint32_t, double>> &UMBList);
    virtual void addBlocktoFileCacheCount(uint32_t index);
    virtual void decBlocktoFileCacheCount(uint32_t index);
    virtual uint32_t getFileCacheCount(uint32_t index);
};

#endif /* BoundedCache_H */
