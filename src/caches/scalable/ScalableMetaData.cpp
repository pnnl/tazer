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
    // if(track) 
    //     updateStats(reserve, timeStamp);
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
            //turn recalc true
            recalc=true;
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
    int blocks = numBlocks.load();
    if(miss) {
        partitionMissCount++;
        if(lastMissTimeStamp) {
            double i = (double)(timestamp - lastMissTimeStamp);
            if(lastDeliveryTime > 0 && blocks ){ //check to make sure we recorded a deliverytime
                //partitionMissCount++;
                partitionMissCost = partitionMissCost + lastDeliveryTime;
                //check for division by zero 
                double scaledMissCost = log2(partitionMissCost / (partitionMissCount-1));
                benefitHistogram.addData(i, (((double) accessPerInterval/scaledMissCost)/blocks)/log2(i));

                // double cost = log2(lastDeliveryTime);
                // benefitHistogram.addData(i, accessPerInterval/cost/blocks);
                
                //here we are guaranteed to have at least two misses, and data in the histograms, so we can call recalc marginal benefit for the partition
                recalc = true;
            }
            else{
                PPRINTF("BM: We have a second miss but no deliverytime yet! \n");
            }
            accessPerInterval = 0;
            missInterval.addData(i, 1);
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
    if(numBlocks.load() < 1){
        unitBenefit=0;
        upperLevelMetric = std::numeric_limits<double>::min();
        prevUnitBenefit=0;
        prevSize=0;
        ret = unitMarginalBenefit = 0;
    }
    else if(lastMissTimeStamp && recalc) {
        double t = ((double) time) / ((double) misses);
        double Mh = missInterval.getValue(t);
        double Bh = benefitHistogram.getValue(t);

        unitBenefit = (Bh/Mh);///log2(t);
        upperLevelMetric = Mh*log2(partitionMissCost/(partitionMissCount-1));
        auto curBlocks = numBlocks.load();
        if(curBlocks > prevSize){
            unitMarginalBenefit = unitBenefit - prevUnitBenefit;
            prevUnitBenefit = unitBenefit;
            prevSize = curBlocks;
        }
        else if(prevSize > curBlocks) {
            unitMarginalBenefit = prevUnitBenefit - unitBenefit;
            prevUnitBenefit = unitBenefit;
            prevSize = curBlocks;
        }
        else{
            //we haven't changed the number of blocks yet, we might get a new block or reuse. we don't know yet 
            PRINTF("prevsize and current size is the same! \n");
        }
        ret = unitMarginalBenefit;
        //ret = unitMarginalBenefit = marginalBenefit / ((double) numBlocks.load());
        //DPRINTF("* marginalBenefit: %lf unitMarginalBenefit: %lf\n", marginalBenefit, unitMarginalBenefit);
        if(isnan(unitMarginalBenefit)){
            PPRINTF("** nan! t: %f, misses: %d, Bh: %lf Mh: %lf unitBenefit: %lf unitMarginalBenefit: %lf numblocks %d \n",t, misses, Bh, Mh , unitBenefit, unitMarginalBenefit,numBlocks.load());
            missInterval.printBins();
            benefitHistogram.printBins();
        }
        if(isinf(unitMarginalBenefit)){
            PPRINTF("* inf! t: %f, misses: %d, Bh: %lf Mh: %lf unitBenefit: %lf unitMarginalBenefit: %lf numblocks %d \n",t, misses, Bh, Mh , unitBenefit, unitMarginalBenefit,numBlocks.load());
            missInterval.printBins();
            benefitHistogram.printBins();
        }
        recalc = false;
    }
    // else{
    //     PPRINTF("we have blocks but no misstime yet \n");
    // }
    metaLock.writerUnlock();
    return ret;
}


void ScalableMetaData::updateRank(bool dec) {
    metaLock.writerLock();
    if(lastMissTimeStamp) {
        DPRINTF("ScalableMetaData::updateRank Before marginalBenefit: %lf unitMarginalBenefit: %lf\n", marginalBenefit, unitMarginalBenefit);
        if(!std::isnan(unitBenefit) && !std::isnan(unitMarginalBenefit)) {
            if(dec) {
                unitBenefit-=unitMarginalBenefit;
                //JS: This checks if marginalBenefit == 0
                // The check marginalBenefit == 0 seems to fail
                if(unitBenefit - unitMarginalBenefit < 0) {
                    unitMarginalBenefit = 0;
                    unitBenefit = 0;
                }
            }
            else
                unitBenefit+=unitMarginalBenefit;
            DPRINTF("ScalableMetaData::updateRank After marginalBenefit: %lf unitMarginalBenefit: %lf\n", unitBenefit, unitMarginalBenefit);
        }
    }
    metaLock.writerUnlock();
}

void ScalableMetaData::updateDeliveryTime(double deliveryTime) {
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

double ScalableMetaData::getUnitBenefit() {
    double ret = 0;
    metaLock.readerLock();
    ret = unitBenefit;
    metaLock.readerUnlock();
    return ret;
}

double ScalableMetaData::getUpperMetric(){
    double ret;
    metaLock.readerLock();
    ret = upperLevelMetric;
    metaLock.readerUnlock();
    return ret;
}