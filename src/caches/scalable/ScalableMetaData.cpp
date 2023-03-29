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
#include <csignal>

#define DPRINTF(...)
//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

#define PPRINTF(...)
//#define PPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

//#define TPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)
#define TPRINTF(...)

//#define BPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)
#define BPRINTF(...)
#define ZPRINTF(...) //fprintf(stderr, __VA_ARGS__); fflush(stderr)
uint8_t * ScalableMetaData::getBlockData(uint64_t blockIndex, uint64_t fileOffset, bool &reserve, bool track) {
    auto blockEntry = &blocks[blockIndex];
    uint64_t timeStamp = trackAccess(blockIndex, fileOffset);
    //JS: This is for the first call from readBlock, which sets the read lock until blockset
    if(track) {
        auto old = blockEntry->blkLock.readerLock();
        //JS: This means it is the first but we still need to see if the data is there
        reserve = old == 0;
        //blockEntry->timeStamp.store(timeStamp);
    }

    //JS: Look to see if the data is avail, notice this is after a reader lock
    uint8_t * ret = blockEntry->data.load();
    
    if(ret)
    {
        blockEntry->timeStamp.store((uint64_t)Timer::getCurrentTime());
    }
    
    //JS: We &= because we only want to reserve if we are the first and the data is empty
    reserve &= (!ret);
    // if(track) 
    //     updateStats(reserve, timeStamp);
    return ret;
}

void ScalableMetaData::setBlock(uint64_t blockIndex, uint8_t * data) {
    auto blockEntry = &blocks[blockIndex];
    //blockEntry->blkLock.readerLock();
    //JS: Inc our total number of blocks
    numBlocks.fetch_add(1);
    blockEntry->data.store(data);

    metaLock.writerLock();
    currentBlocks.push_back(blockEntry);
    metaLock.writerUnlock();

    blockEntry->timeStamp.store((uint64_t)Timer::getCurrentTime());
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

uint8_t * ScalableMetaData::oldestBlock(uint64_t &blockIndex, bool reuse) {
    uint8_t * ret = NULL;
    metaLock.writerLock();

    uint64_t minTime = std::numeric_limits<uint64_t>::max();
    BlockEntry *minEntry = NULL;
    uint64_t minIndex = std::numeric_limits<uint64_t>::max();


    //if(reuse || currentBlocks.size() > 1) { //we can't give up the only block file has but it can reuse it itself
    if(true){
        for(int i=0; i<currentBlocks.size(); i++) {
            auto it = currentBlocks[i];
            if ((it)->blkLock.cowardlyTryWriterLock()) { //this will tell us if the block is in use
                if((it)->timeStamp.load() < minTime){
                    minTime = (it)->timeStamp.load();
                    minEntry = it;
                    minIndex=i;
                }
                (it)->blkLock.writerUnlock();
            }
        }
    }
    else{
        BPRINTF("this file only has 1 block, cannot give it up..");
    }

    BPRINTF("after oldestblock loop, minindex is: %u, total blocks: %d \n", minIndex,  currentBlocks.size());
    if(minEntry != NULL){ //we found a min entry 

        if (minEntry->blkLock.cowardlyTryWriterLock()) {
            blockIndex = (minEntry)->blockIndex;
            DPRINTF("block: %lu timestamp: %lu\n", blockIndex, (*it)->timeStamp.load());
            //JS: Take its memory!!!
            ret = (minEntry)->data;
            (minEntry)->data.store(NULL);
            (minEntry)->blkLock.writerUnlock();
            numBlocks.fetch_sub(1);
            //turn recalc true
            recalc=true;
            //JS: And remove its existence...
            currentBlocks.erase(currentBlocks.begin()+minIndex);
        }
        else{
            //here our minentry block has become active since our loop, we need to do something? 
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
    lastAccessTimeStamp=timestamp;
    ZPRINTF(" miss?:%d accessPerInterval:%d z:%d\n", miss, accessPerInterval, maxAccessInMissInterval);
    int blocks = numBlocks.load();
    if(miss) {
        BPRINTF("  calculating after a miss: access:%d, Zvalue: %d\n", accessPerInterval, maxAccessInMissInterval); 
        ZPRINTF("  calculating after a miss: access:%d, Zvalue: %d\n", accessPerInterval, maxAccessInMissInterval);
        partitionMissCount++;
        if(lastMissTimeStamp) {
            double i = (double)(timestamp - lastMissTimeStamp);
            totalMissIntervals += i; 
            if(lastDeliveryTime > 0 && blocks ){ //check to make sure we recorded a deliverytime
                //partitionMissCount++;
                partitionMissCost = partitionMissCost + lastDeliveryTime;
                //check for division by zero 
                double scaledMissCost = log2(partitionMissCost / (partitionMissCount-1));

                //quick fix for z value calculations 
                //(we end up counting 1 extra access.So subtract one here. We check for 0 because on the very first block access we don't count the extra 1,so Z can be calculated as 0)
                if(maxAccessInMissInterval > 0 ){
                    maxAccessInMissInterval -= 1;
                }
                if(accessPerInterval<maxAccessInMissInterval){
                    //we can't have accesses less than single block accesses, somethings wrong
                    std::cerr << "[TAZER] Error in Z value calculations [access: "<< accessPerInterval << " z value: "<< maxAccessInMissInterval << std::endl;
                    raise(SIGSEGV);
                }
                double newZ = maxAccessInMissInterval + (maxAccessInMissInterval*1.0)/(maxAccessInMissInterval+1);
                BPRINTF("  calculating after a miss: access:%d, Zvalue: %d, newZ: %f\n", accessPerInterval, maxAccessInMissInterval, newZ); 
                if(Config::H_parameter == 0){
                    double ApZp = 64*1000000000*( (double)accessPerInterval - maxAccessInMissInterval);
                    benefitHistogram.addData(i, ((ApZp/scaledMissCost)/blocks)/i);
                }
                else if(Config::H_parameter == 1){
                    double ApZp = 64*64*( (double)accessPerInterval - maxAccessInMissInterval);
                    benefitHistogram.addData(log2(i), ((ApZp/scaledMissCost)/blocks)/log2(i));
                }
                else{
                    //undefined H variable
                    std::cerr << "[TAZER] Undefined value for H parameter "<< Config::H_parameter << std::endl;
                    raise(SIGSEGV);
                }
                
                //here we are guaranteed to have at least two misses, and data in the histograms, so we can call recalc marginal benefit for the partition
                recalc = true;
            }
            else{
                PPRINTF("BM: We have a second miss but no deliverytime yet! \n");
            }
            accessPerInterval = 0;
            maxAccessInMissInterval=0;
            //config::H_parameter holds the type of miss interval histogram logging
            if(Config::H_parameter == 0){
                TPRINTF("H_parameter=0: miss histogram use normal time\n");
                missInterval.addData(i, 1);
            }
            else if(Config::H_parameter == 1){
                TPRINTF("H_parameter=1: miss histogram use log time\n");
                missInterval.addData(log2(i), 1);
            }
            else{
                //undefined H variable
                std::cerr << "[TAZER] Undefined value for H parameter "<< Config::H_parameter << std::endl;
                raise(SIGSEGV);
            }
            
        }
        DPRINTF("SETTING: %lu = %lu\n", lastMissTimeStamp, timestamp);
        lastMissTimeStamp = timestamp;
        // after a miss, reset Z value counters 
        accessPerInterval = 0;
        maxAccessInMissInterval=0;
        blockAccessCounter.store(1);
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
    else if(lastDeliveryTime && recalc) {
        double t = ((double) time) / ((double) misses);
        //OCEANE: if we test log(i), send log(t) here instead of t
        double Mh;
        double Bh;
        if(Config::H_parameter == 0){
            TPRINTF("H_parameter=0: miss histogram use normal time\n");
            Mh = missInterval.getValue(t);
            Bh = benefitHistogram.getValue(t);
        }
        else if(Config::H_parameter == 1){
            TPRINTF("H_parameter=1: miss histogram use log time\n");
            Mh = missInterval.getValue(log2(t));
            Bh = benefitHistogram.getValue(log2(t));
        }
        else{
            //undefined H variable
            std::cerr << "[TAZER] Undefined value for H parameter "<< Config::H_parameter << std::endl;
            raise(SIGSEGV);
        }

        unitBenefit = (Bh/Mh);///log2(t);

        TPRINTF("baskets for histogram: %d\n", Config::Hb_parameter);
        //Config::MC_parameter hold the miss cost type 
        if(Config::MC_parameter == 0){
            //use cost as is  
            TPRINTF("MC_parameter=0: miss cost is used as is\n");
            upperLevelMetric = Mh*(partitionMissCost/(partitionMissCount-1));
        }
        else if(Config::MC_parameter == 1){
            //use cost=1
            TPRINTF("MC_parameter=1: miss cost is used as 1\n");
            upperLevelMetric = Mh;
        }
        else if(Config::MC_parameter == 2){
            //use cost = log(cost)
            TPRINTF("MC_parameter=2: miss cost is used as log(cost)\n");
            upperLevelMetric = Mh*log2(partitionMissCost/(partitionMissCount-1));
        }
        else{
            //undefined MC value
            std::cerr << "[TAZER] Undefined value for MC parameter "<< Config::MC_parameter << std::endl;
            raise(SIGSEGV);
        }

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
        //ret = unitMarginalBenefit;
        ret = unitMarginalBenefit; // / ((double) numBlocks.load());
        //DPRINTF("* marginalBenefit: %lf unitMarginalBenefit: %lf\n", marginalBenefit, unitMarginalBenefit);
        
        //////TEMPORARY CHANGE TO UPPERLEVELMETRIC  (upperlevelmetric=umb)
        //upperLevelMetric=unitMarginalBenefit;
        //////END TEMPORARY

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

    //*** PREDICT OLDEST ***//
    auto curBlocks = numBlocks.load();
    oldestPredicted = lastAccessTimeStamp - (totalMissIntervals*1.0/(partitionMissCount))*(curBlocks-1);
    PPRINTF("Oldest Timestamp Predicted As:%lf  numblocks:%d  lastaccess:%ld totalmissintervals:%lu  partitionMissCount:%lu, averagemissinterval:%.10lf\n",oldestPredicted, curBlocks, lastAccessTimeStamp, totalMissIntervals, partitionMissCount,(totalMissIntervals*1.0/(partitionMissCount)) );
    // END PREDICT OLDEST //

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
                    //if (unitBenefit == 0){
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

void ScalableMetaData::trackZValue(uint32_t index){
    
    BPRINTF("Z val tracking. BEFORE: lastblock: %d, curaccess:%d, maxaccesses:%d\n",lastAccessedBlock, blockAccessCounter.load(), maxAccessInMissInterval);
    if(lastAccessedBlock == index){
        blockAccessCounter.fetch_add(1);
    }
    else{
        BPRINTF("new block accessed, curaccess: %d, max: %d \n", blockAccessCounter.load(), maxAccessInMissInterval);
        if(maxAccessInMissInterval < blockAccessCounter.load()){
            maxAccessInMissInterval = blockAccessCounter.load();
        }
        else{
            BPRINTF(" .  ended up in else\n");
        }
        blockAccessCounter.store(1);
        lastAccessedBlock = index;
    }
    BPRINTF("Z val tracking. AFTER: lastblock: %d, curaccess:%d, maxaccesses:%d\n",lastAccessedBlock, blockAccessCounter.load(), maxAccessInMissInterval);
    
}
