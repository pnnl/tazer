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

#ifndef SCALABLECACHE_H
#define SCALABLECACHE_H
#include "Cache.h"
#include "Loggable.h"
#include "Trackable.h"
#include "ReaderWriterLock.h"
#include "ScalableMetaData.h"
#include "ScalableAllocator.h"
#include <map>

#define SCALABLECACHENAME "scalable"

class ScalableCache : public Cache {
  public:
    ScalableCache(std::string cacheName, CacheType type, uint64_t blockSize, uint64_t maxCacheSize);
    virtual ~ScalableCache();
    
    static Cache* addScalableCache(std::string cacheName, CacheType type, uint64_t blockSize, uint64_t maxCacheSize);

    virtual bool writeBlock(Request *req);
    virtual void readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority);
    
    virtual void addFile(uint32_t fileIndex, std::string filename, uint64_t blockSize, std::uint64_t fileSize);
    virtual void closeFile(uint32_t fileIndex);

    //JS: Hueristics for replacement
    virtual ScalableMetaData * oldestFile(uint32_t &oldestFileIndex);
    ScalableMetaData * findVictim(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, bool mustSucceed=false);
    ScalableMetaData * randomFile(uint32_t &sourceFileIndex);
    ScalableMetaData * largestFile(uint32_t &largestFileIndex);
    
    //JS: These hueristics are built mainly for scalable cache fallback use.  They walk through all files/blocks
    //looking for blocks.
    uint8_t * findBlockFromCachedUMB(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, uint64_t &sourceBlockIndex);
    uint8_t * findBlockFromOldestFile(uint32_t allocateForFileIndex, uint32_t &sourceFileIndex, uint64_t &sourceBlockIndex);

    //JS: For tracking
    void trackBlockEviction(uint32_t fileIndex, uint64_t blockIndex);
    void trackPattern(uint32_t fileIndex, std::string pattern);
  
    //JS: This is so we can let others piggyback off our metrics.
    //JS: I guess it is not that bad to just return vectors...
    //https://stackoverflow.com/questions/15704565/efficient-way-to-return-a-stdvector-in-c
    std::vector<std::tuple<uint32_t, double>> getLastUMB(Cache * cache);
    void setUMBCache(Cache * cache);

  protected:
    ReaderWriterLock *_cacheLock;
    std::unordered_map<uint32_t, ScalableMetaData*> _metaMap;
    uint64_t _blockSize;
    uint64_t _numBlocks;
    TazerAllocator * _allocator;
    
    //JS: This is just temp for checking which files had blocks evicted
    Histogram evictHisto;

    //JS: For Nathan
    std::atomic<uint64_t> access;
    std::atomic<uint64_t> misses;
    uint64_t startTimeStamp;

    //JS: This is to make sure there will exist a block that is not requested
    std::atomic<uint64_t> oustandingBlocksRequested;
    std::atomic<uint64_t> maxOutstandingBlocks;
    std::atomic<uint64_t> maxBlocksInUse;


  private:
    uint8_t * getBlockData(uint32_t fileIndex, uint64_t blockIndex, uint64_t fileOffset);
    uint8_t * getBlockDataOrReserve(uint32_t fileIndex, uint64_t blockIndex, uint64_t fileOffset, bool &reserve, bool &full);
    void setBlock(uint32_t fileIndex, uint64_t blockIndex, uint8_t * data, uint64_t dataSize, bool writeOptional);
    void checkMaxBlockInUse(std::string msg, bool die);
    void checkMaxInFlightRequests(uint64_t index);

    //JS: Metric piggybacking
    ReaderWriterLock * _lastVictimFileIndexLock;
    std::vector<std::tuple<uint32_t, double>> _UMBList;
    std::unordered_map<Cache*, bool> _UMBDirty;

    std::once_flag first_miss;
};

#endif // SCALABLECACHE_H
