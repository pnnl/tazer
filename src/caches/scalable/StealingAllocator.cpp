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
#include "StealingAllocator.h"
#include <unistd.h>
#include <memory>
#include <random>
#include <algorithm>
#include <string.h>

#define DPRINTF(...)
// #define PRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

void StealingAllocator::setCache(ScalableCache * cache) {
    scalableCache = cache;
}

TazerAllocator * StealingAllocator::addStealingAllocator(uint64_t blockSize, uint64_t maxSize, ScalableCache * cache) {
    StealingAllocator * ret = (StealingAllocator*) addAllocator<StealingAllocator>(std::string("StealingAllocator"), blockSize, maxSize);
    ret->setCache(cache);
    return ret;
}

uint8_t * StealingAllocator::allocateBlock(uint32_t allocateForFileIndex, bool must) {
    //JS: For trackBlock
    uint64_t sourceBlockIndex;
    uint32_t sourceFileIndex;

    //JS: Can we allocate new blocks
    if(_availBlocks.fetch_sub(1)) {
        DPRINTF("[JS] StealingAllocator::allocateBlock new block\n");
        return new uint8_t[_blockSize];
    }
    _availBlocks.fetch_add(1);

    //JS: Try to take from closed files first
    uint8_t * ret = NULL;
    allocLock.writerLock();
    if(priorityVictims.size()) {
        auto meta = priorityVictims.back();
        ret = meta->oldestBlock(sourceBlockIndex);
        //if no blocks left, remove it from victims list
        if( ! meta->getNumBlocks() ) {
            priorityVictims.pop_back();
        }
        DPRINTF("[JS] StealingAllocator::allocateBlock taking from victim %p\n", ret);
    }
    allocLock.writerUnlock();

    //JS: Try to steal
    if(!ret) {
        ret = stealBlock(allocateForFileIndex, sourceBlockIndex, sourceFileIndex, must);
        DPRINTF("[JS] StealingAllocator::allocateBlock trying to steal %p %lu %u\n", ret, sourceBlockIndex, sourceFileIndex);
        if(ret)
            scalableCache->trackBlockEviction(sourceFileIndex, sourceBlockIndex);
    }
    return ret;
}

void StealingAllocator::closeFile(ScalableMetaData * meta) {
    if(meta) {
        allocLock.writerLock();
        priorityVictims.push_back(meta);
        allocLock.writerUnlock();
    }
}


void StealingAllocator::openFile(ScalableMetaData * meta) {
    //remove from priority victims list if it's there
    allocLock.writerLock();
    DPRINTF("[JS] StealingAllocator::openFile adding new file %p\n", meta);
    for ( std::vector<ScalableMetaData*>::iterator it = priorityVictims.begin(); it != priorityVictims.end(); it++ ){
        if(*it == meta){
            priorityVictims.erase(it);
            DPRINTF("[BM] StealingAllocator::openFile removed a victim file\n");
            break;
        }
    }
    allocLock.writerUnlock();
}
