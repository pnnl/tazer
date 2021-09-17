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
#include <atomic>
#include <deque>
#include <vector>
#include <array>

const char* const patternName[] = { "Random", "Linear" };

struct ScalableMetaData {
    private:
        enum StridePattern {
            RANDOM = 0,
            LINEAR
        };

        struct BlockEntry {
            int64_t blockIndex;
            ReaderWriterLock blkLock;
            std::atomic<uint64_t> timeStamp;
            std::atomic<uint8_t*> data;
            BlockEntry(): blockIndex(0), timeStamp(0), data(0) { }
        };

        uint64_t totalBlocks;
        StridePattern pattern;
        ReaderWriterLock metaLock;
        BlockEntry * blocks;

        std::atomic<uint64_t> numBlocks;
        std::deque<std::array<uint64_t, 3>> window;
        std::vector<BlockEntry*> currentBlocks;

    public:
        ScalableMetaData(uint64_t bSize, uint64_t fSize):
            totalBlocks(fSize / bSize + ((fSize % bSize) ? 1 : 0)),
            pattern(RANDOM),
            numBlocks(0) {
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
        bool checkPattern();
        uint8_t * oldestBlock(uint64_t &blockIndex);
        uint64_t getLastTimeStamp();

    private:
        uint64_t trackAccess(uint64_t blockIndex, uint64_t readIndex);
};

#endif //SCALABLE_META_DATA_H

  