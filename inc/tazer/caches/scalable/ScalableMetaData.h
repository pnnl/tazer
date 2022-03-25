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

#ifndef SCALABLE_META_DATA_H
#define SCALABLE_META_DATA_H
#include "ReaderWriterLock.h"
#include "Trackable.h"
#include "Histogram.h"
#include "Cache.h"
#include <atomic>
#include <deque>
#include <vector>
#include <array>

const char* const patternName[] = { "UNKNOWN", "BLOCKSTREAMING" };

struct ScalableMetaData {
    private:
        enum StridePattern {
            UNKNOWN = 0,
            BLOCKSTREAMING
        };

        struct BlockEntry {
            int64_t blockIndex;
            ReaderWriterLock blkLock;
            std::atomic<uint64_t> timeStamp;
            std::atomic<uint8_t*> data;
            BlockEntry(): blockIndex(0), timeStamp(0), data(0) { }
        };

        uint64_t fileSize;
        uint64_t blockSize;
        uint64_t totalBlocks;
        StridePattern pattern;
        ReaderWriterLock metaLock;
        BlockEntry * blocks;

        //JS: For Nathan
        bool recalc;
        uint64_t access;
        uint64_t accessPerInterval;
        uint64_t lastMissTimeStamp;
        double marginalBenefit;
        double unitMarginalBenefit;
        //BM: for algorithm calculations
        double lastDeliveryTime;

        std::atomic<uint64_t> numBlocks;
        
        //JS: Also for Nathan (intervalTime, fpGrowth, missInverval)
        Histogram fpGrowth;
        Histogram missInterval;
        Histogram costHistogram;
        Histogram demandHistogram;

        std::deque<std::array<uint64_t, 3>> window;
        std::vector<BlockEntry*> currentBlocks;

    public:
        ScalableMetaData(uint64_t bSize, uint64_t fSize):
            fileSize(fSize),
            blockSize(bSize),
            totalBlocks(fSize / bSize + ((fSize % bSize) ? 1 : 0)),
            pattern(UNKNOWN),
            recalc(false),
            access(0),
            accessPerInterval(0),
            lastMissTimeStamp(0),
            marginalBenefit(0),
            unitMarginalBenefit(0),
            lastDeliveryTime(-1.0),
            numBlocks(0),
            fpGrowth(100),
            missInterval(100),
            costHistogram(100),
            demandHistogram(100) {
                blocks = new BlockEntry[totalBlocks];
                for(unsigned int i=0; i<totalBlocks; i++) {
                    blocks[i].data.store(NULL);
                    blocks[i].blockIndex = i;
                }
            }
        
        //JS: These are modifying blocks data, meta data, and usage
        uint8_t * getBlockData(uint64_t blockIndex, uint64_t fileOffset, bool &reserve, bool track);
        void setBlock(uint64_t blockIndex, uint8_t * data);
        void decBlockUsage(uint64_t blockIndex);
        bool backOutOfReservation(uint64_t blockIndex); 

        //JS: These are huristics to support cache/allocators
        bool checkPattern(Cache * cache=NULL, uint32_t fileIndex=0);
        uint8_t * oldestBlock(uint64_t &blockIndex);
        uint8_t * randomBlock(uint64_t &blockIndex);
        uint64_t getLastTimeStamp();
        uint64_t getNumBlocks();
        uint64_t getPattern();

        //JS: From Nathan
        void updateStats(bool miss, uint64_t timestamp);
        double calcRank(uint64_t time, uint64_t misses);
        void updateRank(bool dec);

        //BM: for deliverytime
        void updateDeliveryTime(uint64_t deliveryTime);
        //BM: for plots
        double getUnitMarginalBenefit();

    private:
        uint64_t trackAccess(uint64_t blockIndex, uint64_t readIndex);
};

#endif //SCALABLE_META_DATA_H

  