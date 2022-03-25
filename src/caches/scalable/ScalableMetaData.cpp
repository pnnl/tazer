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

#include "ScalableMetaData.h"
#include "ScalableCache.h"
#include <cmath>
#include <cfloat>

#define DPRINTF(...)
//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

//#define PRINTF(...)
#define PPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

uint8_t * ScalableMetaData::getBlockData(uint64_t blockIndex, uint64_t fileOffset, bool &reserve, bool track) {
    auto blockEntry = &blocks[blockIndex];
    uint64_t timeStamp = trackAccess(blockIndex, fileOffset);
    //JS: This is for the first call from readBlock, which sets the read lock until blockset
    if(track) {
        auto old = blockEntry->blkLock.readerLock();
        //JS: This means it is the first but we still need to see if the data is there
        reserve = old == 0;
        blockEntry->timeStamp.store(timeStamp);
    }

    //JS: Look to see if the data is avail, notice this is after a reader lock
    uint8_t * ret = blockEntry->data.load();
    //JS: We &= because we only want to reserve if we are the first and the data is empty
    reserve &= (!ret);
    if(track) 
        updateStats(reserve, timeStamp);
    return ret;
}

void ScalableMetaData::setBlock(uint64_t blockIndex, uint8_t * data) {
    auto blockEntry = &blocks[blockIndex];

    //JS: Inc our total number of blocks
    numBlocks.fetch_add(1);
    blockEntry->data.store(data);

    metaLock.writerLock();
    currentBlocks.push_back(blockEntry);
    metaLock.writerUnlock();

    //JS: Dec our usage. It was inc'ed in getBlock
    blockEntry->blkLock.readerUnlock();
}

void ScalableMetaData::decBlockUsage(uint64_t blockIndex) {
    auto temp = blocks[blockIndex].blkLock.readerUnlock();
    DPRINTF("ScalableMetaData::decBlockUsage unlock: %lu\n", temp);
}

bool ScalableMetaData::backOutOfReservation(uint64_t blockIndex) {
    auto ret = blocks[blockIndex].blkLock.cowardlyUpdgradeWriterLock();
    if(ret)
        blocks[blockIndex].blkLock.writerUnlock();
    return ret;
}

uint64_t ScalableMetaData::trackAccess(uint64_t blockIndex, uint64_t readIndex) {
    metaLock.writerLock();

    uint64_t timeStamp = (uint64_t)Timer::getCurrentTime();
    std::array<uint64_t, 3> access = {readIndex, blockIndex, timeStamp};
    window.push_front(access);
    
    if(window.size() == 6)
        window.pop_back();
    metaLock.writerUnlock();
    
    return timeStamp;
}

bool ScalableMetaData::checkPattern(Cache * cache, uint32_t fileIndex) {
    bool ret = true;
    metaLock.readerLock();

    auto newPattern = pattern;
    auto oldPattern = pattern;
    if (window.size() > 2) {
        newPattern = BLOCKSTREAMING;
        for (unsigned int i = 1; i < window.size(); i++) {
            uint64_t stride = window[i-1][1] - window[i][1];
            DPRINTF("window[%d] : %lu %lu %lu\n", i-1, window[i-1][0], window[i-1][1], window[i-1][2]);
            DPRINTF("%d:%lu - %d:%lu %lu\n", i, window[i-1][1], i-1, window[i][1], stride);
            if (stride > 1) {
                newPattern = UNKNOWN;
            }
        }
    }

    DPRINTF("FILEINDEX: %d, oldPattern: %s newPattern: %s\n", fileIndex, patternName[oldPattern], patternName[newPattern]);
    //JS: For now the only case that limits is linear with more than one block
    if(newPattern == BLOCKSTREAMING && numBlocks.load())
        ret = false;

    metaLock.readerUnlock();

    if(oldPattern != newPattern) {
        DPRINTF("[JS] Updating cache pattern %s\n", patternName[newPattern]);
        ((ScalableCache*)cache)->trackPattern(fileIndex, patternName[newPattern]);
        metaLock.writerLock();
        pattern = newPattern;
        metaLock.writerUnlock();
    }
    return ret;
}

uint64_t ScalableMetaData::getLastTimeStamp() {
    uint64_t ret = 0;
    metaLock.readerLock();
    if(!window.empty())
        ret = window.front()[2];
    metaLock.readerUnlock();
    return ret;
}

uint64_t ScalableMetaData::getNumBlocks() {
    uint64_t ret = 0;
    metaLock.readerLock();
    ret = numBlocks.load();
    metaLock.readerUnlock();
    return ret;
}

uint64_t ScalableMetaData::getPattern() {
    uint64_t ret = 0;
    metaLock.readerLock();
    ret = pattern;
    metaLock.readerUnlock();
    return ret;
}

uint8_t * ScalableMetaData::randomBlock(uint64_t &blockIndex) {
    uint8_t * ret = NULL;
    metaLock.writerLock();
    unsigned int mapSize = currentBlocks.size();
    for(unsigned int i=0; i<mapSize; i++) {
        //JS: Get random block
        blockIndex = rand() % mapSize;
        auto it = currentBlocks.begin() + blockIndex;

        //JS: Check the block is not being requested
        if ( (*it)->blkLock.cowardlyTryWriterLock() ) {
            DPRINTF("block: %lu timestamp: %lu\n", blockIndex, (*it)->timeStamp.load());
            //JS: Take its memory!!!
            ret = (*it)->data;
            (*it)->data.store(NULL);
            (*it)->blkLock.writerUnlock();
            numBlocks.fetch_sub(1);
            //JS: And remove its existence...
            currentBlocks.erase(it);
            break;
        }
    }
    metaLock.writerUnlock();
    return ret;
}

uint8_t * ScalableMetaData::oldestBlock(uint64_t &blockIndex) {
    uint8_t * ret = NULL;
    metaLock.writerLock();

    if(currentBlocks.size() > 1) {
        DPRINTF("ScalableMetaData::oldestBlock CurrentBlock.size(): %d\n", currentBlocks.size());
        sort(currentBlocks.begin(), currentBlocks.end(), 
            [](BlockEntry* lhs, BlockEntry* rhs) {
                return lhs->timeStamp.load() < rhs->timeStamp.load();
            });
    }

    for(auto it = currentBlocks.begin(); it != currentBlocks.end(); it++) {
        //JS: Check the block is not being requested
        if ((*it)->blkLock.cowardlyTryWriterLock()) {
            //JS: For tracking...
            blockIndex = (*it)->blockIndex;
            DPRINTF("block: %lu timestamp: %lu\n", blockIndex, (*it)->timeStamp.load());
            //JS: Take its memory!!!
            ret = (*it)->data;
            (*it)->data.store(NULL);
            (*it)->blkLock.writerUnlock();
            numBlocks.fetch_sub(1);
            //JS: And remove its existence...
            currentBlocks.erase(it);
            break;
        }
    }

    metaLock.writerUnlock();
    return ret;
}

//JS: This function only updates the "per partition" stats
void ScalableMetaData::updateStats(bool miss, uint64_t timestamp) {
    metaLock.writerLock();
    access++;
    accessPerInterval++;

    if(miss) {
        if(lastMissTimeStamp) {
            double i = (double)(timestamp - lastMissTimeStamp);
            //missInterval.addData(i, 1);

            if(lastDeliveryTime > 0){ //check to make sure we recorded a deliverytime
                //PRINTF("adding to cost histogram, i: %f,accessPerInterval:%d, lastDeliveryTime:%f, val: %.10f\n", i, accessPerInterval, lastDeliveryTime, ((double)accessPerInterval)/lastDeliveryTime);
                //costHistogram.addData(i, ((double) accessPerInterval) / lastDeliveryTime);

//                costHistogram.addData(i, (lastDeliveryTime / (double) accessPerInterval) );


                //maxcost is 2^30  (~1 second) 
                double cost = (lastDeliveryTime < 1073741824 ? lastDeliveryTime : 1073741824);
                demandHistogram.addData(i, cost/1073741824.0);
                costHistogram.addData(i, (double) accessPerInterval);
                recalc = true;
            }
            else{
                PPRINTF("BM: We have a second miss but no deliverytime yet! \n");
            }
            accessPerInterval = 0;
        }
        DPRINTF("SETTING: %lu = %lu\n", lastMissTimeStamp, timestamp);
        lastMissTimeStamp = timestamp;
    }
    metaLock.writerUnlock();
}

//JS: This can be considered another hueristic for determining eviction
double ScalableMetaData::calcRank(uint64_t time, uint64_t misses) {
    double ret = unitMarginalBenefit;
    metaLock.writerLock();
    DPRINTF("* Timestamp: %lu recalc: %u\n", time, recalc);
    if(lastMissTimeStamp && recalc) {
        double i = ((double) time) / ((double) misses);
        //double Mp = missInterval.getValue(i);
        //double Ch = costHistogram.getValue(i);
        //DPRINTF("* access: %lf misses: %lf\n", (double)access, (double)misses);
        //marginalBenefit = Ch / Mp; 
        
        double Dh = demandHistogram.getValue(i);
        double Ch = costHistogram.getValue(i);
        marginalBenefit = Ch/Dh;

        
        ret = unitMarginalBenefit = marginalBenefit / ((double) numBlocks.load());
        DPRINTF("* marginalBenefit: %lf unitMarginalBenefit: %lf\n", marginalBenefit, unitMarginalBenefit);
        if(isnan(unitMarginalBenefit)){
           // PPRINTF("* nan! i: %f, misses: %d, ch: %lf Mp: %lf marginalBenefit: %lf unitMarginalBenefit: %lf numblocks %d \n",i, misses, Ch, Mp , marginalBenefit, unitMarginalBenefit,numBlocks.load());
            PPRINTF("** nan! i: %f, misses: %d, ch: %lf dh: %lf marginalBenefit: %lf unitMarginalBenefit: %lf numblocks %d \n",i, misses, Ch, Dh , marginalBenefit, unitMarginalBenefit,numBlocks.load());
            demandHistogram.printBins();
            costHistogram.printBins();
        }
        if(isinf(unitMarginalBenefit)){
            PPRINTF("* inf! i: %f, misses: %d, ch: %lf dh: %lf marginalBenefit: %lf unitMarginalBenefit: %lf numblocks %d \n",i, misses, Ch, Dh , marginalBenefit, unitMarginalBenefit,numBlocks.load());
        }
        recalc = false;
    }
    metaLock.writerUnlock();
    return ret;
}

void ScalableMetaData::updateRank(bool dec) {
    metaLock.writerLock();
    if(lastMissTimeStamp) {
        DPRINTF("ScalableMetaData::updateRank Before marginalBenefit: %lf unitMarginalBenefit: %lf\n", marginalBenefit, unitMarginalBenefit);
        if(!std::isnan(marginalBenefit) && !std::isnan(unitMarginalBenefit)) {
            if(dec) {
                marginalBenefit-=unitMarginalBenefit;
                //JS: This checks if marginalBenefit == 0
                // The check marginalBenefit == 0 seems to fail
                if(marginalBenefit - unitMarginalBenefit < 0) {
                    unitMarginalBenefit = 0;
                    marginalBenefit = 0;
                }
            }
            else
                marginalBenefit+=unitMarginalBenefit;
            DPRINTF("ScalableMetaData::updateRank After marginalBenefit: %lf unitMarginalBenefit: %lf\n", marginalBenefit, unitMarginalBenefit);
        }
    }
    metaLock.writerUnlock();
}

void ScalableMetaData::updateDeliveryTime(uint64_t deliveryTime) {
    metaLock.writerLock();
    lastDeliveryTime = deliveryTime;
    metaLock.writerUnlock();
}

double ScalableMetaData::getUnitMarginalBenefit() {
    double ret = 0;
    metaLock.readerLock();
    ret = unitMarginalBenefit;
    metaLock.readerUnlock();
    return ret;
}