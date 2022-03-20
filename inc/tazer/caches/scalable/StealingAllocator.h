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

#ifndef STEALINGALLOCATOR_H
#define STEALINGALLOCATOR_H
#include "ReaderWriterLock.h"
#include "ScalableMetaData.h"
#include "ScalableAllocator.h"
#include "ScalableCache.h"

class StealingAllocator : public TazerAllocator
{
    private:
        ReaderWriterLock allocLock;
        std::vector<ScalableMetaData*> priorityVictims;

    protected:
        ScalableCache * scalableCache;
        void setCache(ScalableCache * cache);

        //JS: This is the steal heuristic.  Overload for more iteresting things!
        virtual uint8_t * stealBlock(uint32_t allocateForFileIndex, uint64_t &sourceBlockIndex, uint32_t &sourceFileIndex, bool must = false) {
            auto meta = scalableCache->oldestFile(sourceFileIndex);
            if(meta)
                return meta->oldestBlock(sourceBlockIndex);
            return NULL;
        }

    public:
        StealingAllocator(uint64_t blockSize, uint64_t maxSize):
            TazerAllocator(blockSize, maxSize) { }

        uint8_t * allocateBlock(uint32_t allocateForFileIndex, bool must);
        virtual void closeFile(ScalableMetaData * meta);
        static TazerAllocator * addStealingAllocator(uint64_t blockSize, uint64_t maxSize, ScalableCache * cache);
        virtual void openFile(ScalableMetaData * meta);
};

class RandomStealingAllocator : public StealingAllocator
{
    private:
        bool _randomBlock;

    protected:
        //JS: This is a more iteresting one!
        virtual uint8_t * stealBlock(uint32_t allocateForFileIndex, uint64_t &sourceBlockIndex, uint32_t &sourceFileIndex, bool must = false) {
            auto meta = scalableCache->randomFile(sourceFileIndex);
            if(meta) {
                if(_randomBlock)
                    return meta->randomBlock(sourceBlockIndex);
                else
                    return meta->oldestBlock(sourceBlockIndex);
            }
            return NULL;
        }

    public:
        RandomStealingAllocator(uint64_t blockSize, uint64_t maxSize):
            StealingAllocator(blockSize, maxSize) { }

        void setRandomBlock(bool randomBlock) {
            _randomBlock = randomBlock;
        }

        static TazerAllocator * addRandomStealingAllocator(uint64_t blockSize, uint64_t maxSize, ScalableCache * cache, bool randomBlock) {
            RandomStealingAllocator * ret = (RandomStealingAllocator*) addAllocator<RandomStealingAllocator>(std::string("RandomStealingAllocator"), blockSize, maxSize);
            ret->setCache(cache);
            ret->setRandomBlock(randomBlock);
            return ret;
        }
};

class LargestStealingAllocator : public StealingAllocator
{
    protected:
        virtual uint8_t * stealBlock(uint32_t allocateForFileIndex, uint64_t &sourceBlockIndex, uint32_t &sourceFileIndex, bool must = false) {
            auto meta = scalableCache->largestFile(sourceFileIndex);
            if(meta) {
                return meta->oldestBlock(sourceBlockIndex);
            }
            return NULL;
        }

    public:
        LargestStealingAllocator(uint64_t blockSize, uint64_t maxSize):
            StealingAllocator(blockSize, maxSize) { }

        static TazerAllocator * addLargestStealingAllocator(uint64_t blockSize, uint64_t maxSize, ScalableCache * cache) {
            LargestStealingAllocator * ret = (LargestStealingAllocator*) addAllocator<LargestStealingAllocator>(std::string("LargestStealingAllocator"), blockSize, maxSize);
            ret->setCache(cache);
            return ret;
        }
};

class AdaptiveAllocator : public StealingAllocator
{
    protected:
        //JS: This is a more iteresting one!
        virtual uint8_t * stealBlock(uint32_t allocateForFileIndex, uint64_t &sourceBlockIndex, uint32_t &sourceFileIndex, bool must) {
            auto meta = scalableCache->findVictim(allocateForFileIndex, sourceFileIndex, must);
            if(meta)
                return meta->oldestBlock(sourceBlockIndex);
            return NULL;
        }

    public:
        AdaptiveAllocator(uint64_t blockSize, uint64_t maxSize):
            StealingAllocator(blockSize, maxSize) { }

        static TazerAllocator * addAdaptiveAllocator(uint64_t blockSize, uint64_t maxSize, ScalableCache * cache) {
            AdaptiveAllocator * ret = (AdaptiveAllocator*) addAllocator<AdaptiveAllocator>(std::string("AdaptiveAllocator"), blockSize, maxSize);
            ret->setCache(cache);
            return ret;
        }
};

class AdaptiveForceWithUMBAllocator : public StealingAllocator
{
    protected:
        //JS: This is a more iteresting one!
        virtual uint8_t * stealBlock(uint32_t allocateForFileIndex, uint64_t &sourceBlockIndex, uint32_t &sourceFileIndex, bool must) {
            auto meta = scalableCache->findVictim(allocateForFileIndex, sourceFileIndex, must);
            if(meta)
                return meta->oldestBlock(sourceBlockIndex);
            else
                return scalableCache->findBlockFromCachedUMB(allocateForFileIndex, sourceFileIndex, sourceBlockIndex);
            return NULL;
        }

    public:
        AdaptiveForceWithUMBAllocator(uint64_t blockSize, uint64_t maxSize):
            StealingAllocator(blockSize, maxSize) { }

        static TazerAllocator * addAdaptiveForceWithUMBAllocator(uint64_t blockSize, uint64_t maxSize, ScalableCache * cache) {
            AdaptiveForceWithUMBAllocator * ret = (AdaptiveForceWithUMBAllocator*) addAllocator<AdaptiveForceWithUMBAllocator>(std::string("AdaptiveForceWithUMBAllocator"), blockSize, maxSize);
            ret->setCache(cache);
            return ret;
        }
};

class AdaptiveForceWithOldestAllocator : public StealingAllocator
{
    protected:
        //JS: This is a more iteresting one!
        virtual uint8_t * stealBlock(uint32_t allocateForFileIndex, uint64_t &sourceBlockIndex, uint32_t &sourceFileIndex, bool must) {
            auto meta = scalableCache->findVictim(allocateForFileIndex, sourceFileIndex, must);
            if(meta)
                return meta->oldestBlock(sourceBlockIndex);
            else
                return scalableCache->findBlockFromOldestFile(allocateForFileIndex, sourceFileIndex, sourceBlockIndex);
            return NULL;
        }

    public:
        AdaptiveForceWithOldestAllocator(uint64_t blockSize, uint64_t maxSize):
            StealingAllocator(blockSize, maxSize) { }

        static TazerAllocator * addAdaptiveForceWithOldestAllocator(uint64_t blockSize, uint64_t maxSize, ScalableCache * cache) {
            AdaptiveForceWithOldestAllocator * ret = (AdaptiveForceWithOldestAllocator*) addAllocator<AdaptiveForceWithOldestAllocator>(std::string("AdaptiveForceWithOldestAllocator"), blockSize, maxSize);
            ret->setCache(cache);
            return ret;
        }
};

#endif // STEALINGALLOCATOR_H
