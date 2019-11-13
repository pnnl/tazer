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

#ifndef BLOCKCACHE_H
#define BLOCKCACHE_H

#include "FileCache.h"
#include "Loggable.h"
#include "ReaderWriterLock.h"
#include <atomic>
#include <deque>
#include <iostream>
#include <map>
#include <mutex>
#include <tuple>

// #define BLK_EMPTY 0
// #define BLK_RES 1
// #define BLK_INIT 2
// #define BLK_COMP 3
// #define BLK_AVAIL 4
//#define BLK_REG 5
//#define BLK_FAIL 6

struct FileEntry;

struct BlockEntry {
    std::atomic_int status;
    std::atomic_int usage;
    FileEntry *fileEntry;
    ReaderWriterLock lock;
    unsigned int index;
    unsigned int timeStamp;
    unsigned int compressedSize;
    unsigned int size;
    char *compressed;
    char *data;

    BlockEntry() : status(0),
                   usage(0),
                   timeStamp(0),
                   compressedSize(0),
                   size(0),
                   compressed(NULL),
                   data(NULL) {}

    BlockEntry(const BlockEntry &entry) {
        status.store(entry.status.load());
        status.store(entry.usage.load());
        fileEntry = entry.fileEntry;
        timeStamp = entry.timeStamp;
        compressedSize = entry.compressedSize;
        size = entry.size;
        compressed = entry.compressed;
        data = entry.data;
    }

    BlockEntry(unsigned int time, unsigned int compSize, unsigned int fullSize, char *comp, char *full) : status(0),
                                                                                                          usage(0),
                                                                                                          fileEntry(NULL),
                                                                                                          timeStamp(time),
                                                                                                          compressedSize(compSize),
                                                                                                          size(fullSize),
                                                                                                          compressed(comp),
                                                                                                          data(full) {}

    ~BlockEntry() {}

    BlockEntry &operator=(const BlockEntry &other) {
        if (this != &other) {
            status.store(other.status.load());
            usage.store(other.status.load());
            fileEntry = other.fileEntry;
            timeStamp = other.timeStamp;
            compressedSize = other.compressedSize;
            size = other.size;
            compressed = other.compressed;
            data = other.data;
        }
        return *this;
    }

    bool operator<(const BlockEntry &rhs) {
        return (timeStamp < rhs.timeStamp);
    }

    unsigned int decUsage(bool useLock = true) {
        unsigned int freeSize = 0;
        if (usage.fetch_sub(1) == 1) {
            if (useLock)
                lock.writerLock();
            status.store(BLK_EMPTY);
            if (compressed) {
                delete[] compressed;
                compressed = NULL;

                freeSize += compressedSize;
                compressedSize = 0;
            }
            if (data) {
                delete[] data;
                data = NULL;

                freeSize += size;
                size = 0;
            }
            if (useLock)
                lock.writerUnlock();
        }
        return freeSize;
    }
};

struct FileEntry {
    std::string fileName;
    std::atomic_int totalBlocks;
    unsigned int regFileIndex;
    BlockEntry *blocks;

    FileEntry() : totalBlocks(0),
                  blocks(NULL) {}

    FileEntry(std::string name, unsigned int numBlocks, unsigned int fileIndex) : fileName(name),
                                                                                  totalBlocks(1),
                                                                                  regFileIndex(fileIndex) {
        blocks = new BlockEntry[numBlocks];
        for (unsigned int i = 0; i < numBlocks; i++) {
            blocks[i].index = i;
            blocks[i].fileEntry = this;
        }
    }
};

class BlockCache : public Loggable {
  public:
    BlockCache(uint64_t maxSize);
    ~BlockCache();

    void pushBlock(BlockEntry *entry, unsigned int reseredSize, FileCache *file, FileCache *file2, bool push = true);

    void updateBlock(BlockEntry *entry);
    unsigned int reserveBlocks(unsigned int blockSize, unsigned int numBlocks, FileCache *file, FileCache *file2);
    void returnSpace(unsigned int size);
    void returnSpace(unsigned int size, unsigned int numBlocks);
    unsigned int freeSpace();

    BlockEntry *createBlocks(std::string fileName, unsigned int numBlocks, unsigned int fileIndex = 0);
    bool decBlockUsage(FileEntry *fileEntry);

    void printEvictions();

    unsigned int binarySearch(unsigned int start, unsigned int end, unsigned int value);

  private:
    void pushHeap(BlockEntry *entry);

    std::atomic_ulong _pushTime;
    std::atomic_ulong _updateTime;
    std::atomic_ulong _evictionTime;

    std::atomic_uint _pushes;
    std::atomic_uint _updates;
    std::atomic_uint _evictions;

    std::atomic<std::uint64_t> _size;
    std::mutex _blockLock;
    uint64_t _maxSize;
    unsigned int _timeStamp;
    std::deque<BlockEntry *> _blocks;

    std::mutex _mapLock;
    std::map<std::string, FileEntry *> _files;

    std::atomic_uint _reserved_blocks;
};

#endif /* BLOCKCACHE_H */
