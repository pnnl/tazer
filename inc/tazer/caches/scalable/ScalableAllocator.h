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

#ifndef SCALABLE_ALLOCATOR_H
#define SCALABLE_ALLOCATOR_H
#include "ScalableMetaData.h"
#include <vector>
#include <atomic>

#define PPRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

class TazerAllocator : public Trackable<std::string, TazerAllocator *>
{
    protected:
        uint64_t _blockSize;
        uint64_t _maxSize;

        std::atomic<uint64_t> _availBlocks;
    
        TazerAllocator(uint64_t blockSize, uint64_t maxSize):
            _blockSize(blockSize),
            _maxSize(maxSize),
            _availBlocks(maxSize/blockSize) { }

        ~TazerAllocator() { }

        template<class T>
        static TazerAllocator * addAllocator(std::string name, uint64_t blockSize, uint64_t maxSize) {
            bool created = false;
            auto ret = Trackable<std::string, TazerAllocator *>::AddTrackable(
                name, [&]() -> TazerAllocator * {
                    TazerAllocator *temp = new T(blockSize, maxSize);
                    return temp;
                }, created);
            return ret;
        }

    public:
        virtual uint8_t * allocateBlock(uint32_t allocateForFileIndex, bool must = false) = 0;
        virtual void closeFile(ScalableMetaData * meta) { }
};

//JS: This is an example of the simplest allocator I can think of
class SimpleAllocator : public TazerAllocator
{
    public:
        SimpleAllocator(uint64_t blockSize, uint64_t maxSize):
            TazerAllocator(blockSize, maxSize) { }

        uint8_t * allocateBlock(uint32_t allocateForFileIndex, bool must = false) {
            return new uint8_t[_blockSize];
        }

        static TazerAllocator * addSimpleAllocator(uint64_t blockSize, uint64_t maxSize) {
            return addAllocator<SimpleAllocator>(std::string("SimpleAllocator"), blockSize, maxSize);
        }
};

//JS: This is an example of the simplest allocator I can think of
class FirstTouchAllocator : public TazerAllocator
{
    private:
        std::atomic<uint64_t> _numBlocks;
        uint64_t _maxBlocks;
    
    public:
        FirstTouchAllocator(uint64_t blockSize, uint64_t maxSize):
            TazerAllocator(blockSize, maxSize),
            _numBlocks(0),
            _maxBlocks(maxSize / blockSize) { 
                PPRINTF("NUMBER OF BLOCKS: %lu\n", _maxBlocks);
            }

        uint8_t * allocateBlock(uint32_t allocateForFileIndex, bool must = false) {
            uint64_t temp = _numBlocks.fetch_add(1);
            if(temp < _maxBlocks)
                return new uint8_t[_blockSize];
            _numBlocks.fetch_sub(1);
            return NULL;
        }

        static TazerAllocator * addFirstTouchAllocator(uint64_t blockSize, uint64_t maxSize) {
            return addAllocator<FirstTouchAllocator>(std::string("FirstTouchAllocator"), blockSize, maxSize);
        }
};

#endif /* SCALABLE_ALLOCATOR_H */
