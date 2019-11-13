// -*-Mode: C++;-*-

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

#include "BlockCache.h"
#include "Config.h"
#include "InputFile.h"
#include "Timer.h"
#include <algorithm>
#include <mutex>
#include <signal.h>
#include <vector>

#define LINEARSEARCHMAX 16

//#define TIMEON(...) __VA_ARGS__
#define TIMEON(...)

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

#ifdef FILE_CACHE_WRITE_THROUGH
#define writeThrough(file, block)
#define writeBack(file, entry, retSize)
#else
#define writeThrough(file, block)
#define writeBack(file, entry, retSize)                                                           \
    if (file) {                                                                                   \
        file->writeBlock(entry->index, entry->size, entry->data, entry->fileEntry->regFileIndex); \
        retSize += entry->decUsage(false);                                                        \
    }
#endif

BlockCache::BlockCache(uint64_t maxSize) : Loggable(Config::CacheFileLog, "CacheLog"),
                                           _pushTime(0),
                                           _updateTime(0),
                                           _evictionTime(0),
                                           _pushes(0),
                                           _updates(0),
                                           _evictions(0),
                                           _size(0),
                                           _maxSize(maxSize),
                                           _timeStamp(1),
                                           _reserved_blocks(0) {}

BlockCache::~BlockCache() {}

void BlockCache::pushBlock(BlockEntry *entry, unsigned int reservedSize, FileCache *file, FileCache *file2, bool push) {
    TIMEON(uint64_t t1 = Timer::getCurrentTime());
    entry->fileEntry->totalBlocks.fetch_add(1);
    if (push) {
        writeThrough(file2, entry); //This is the burst buffer cache
        writeThrough(file, entry);  //I think this is safe...
    }
    *this << "blk: " << entry->index << " added to cache" << std::endl;
    _blockLock.lock();
    entry->timeStamp = _timeStamp++;

    if (reservedSize > entry->size + entry->compressedSize) {
        _size.fetch_sub((reservedSize - (entry->size + entry->compressedSize)));
    }

    if (entry->size + entry->compressedSize > reservedSize) {
        DPRINTF("Error: Data Size is larger than reserved %d %d\n", entry->size + entry->compressedSize, reservedSize);
        raise(SIGSEGV);
    }
    if (_size.load() > _maxSize) {
        DPRINTF("Error: Size larger than Max: %lu vs %lu\n", _size.load(), _maxSize);
        raise(SIGSEGV);
    }

    _blocks.push_back(entry);
    *this << "blocks cnt " << _blocks.size() << " " << reservedSize << " " << (entry->size + entry->compressedSize) << " " << (reservedSize - (entry->size + entry->compressedSize)) << std::endl;
    _reserved_blocks.fetch_sub(1);
    _blockLock.unlock();
    TIMEON(_pushTime.fetch_add(Timer::getCurrentTime() - t1));
    TIMEON(_pushes.fetch_add(1));
}

unsigned int BlockCache::binarySearch(unsigned int start, unsigned int end, unsigned int value) {
    if (_blocks[start]->timeStamp == value)
        return start;

    while (start + LINEARSEARCHMAX <= end) {
        unsigned int middle = start + (end - start) / 2;

        if (_blocks[middle]->timeStamp == value) {
            return middle;
        }

        if (_blocks[middle]->timeStamp > value)
            end = middle - 1;
        else
            start = middle + 1;
    }

    for (unsigned int i = start; i <= end; i++) {
        if (_blocks[i]->timeStamp == value)
            return i;
    }

    return (unsigned int)-1;
}

void BlockCache::updateBlock(BlockEntry *entry) {
    *this << "Cache hit updating block" << std::endl;
    TIMEON(uint64_t t1 = Timer::getCurrentTime());
    _blockLock.lock();
    if (_blocks.size() > 0) {
        unsigned int index = binarySearch(0, _blocks.size() - 1, entry->timeStamp);
        if (index != (unsigned int)-1) {
            _blocks.erase(_blocks.begin() + index);
            entry->timeStamp = _timeStamp++;
            _blocks.push_back(entry);
        }
    }
    _blockLock.unlock();

    TIMEON(_updateTime.fetch_add(Timer::getCurrentTime() - t1));
    TIMEON(_updates.fetch_add(1));
}

bool BlockCache::decBlockUsage(FileEntry *fileEntry) {
    if (fileEntry->totalBlocks.fetch_sub(1) == 1) {
        _mapLock.lock();
        _files.erase(fileEntry->fileName);
        delete[] fileEntry->blocks;
        delete fileEntry;
        _mapLock.unlock();
        return true;
    }
    return false;
}

unsigned int BlockCache::reserveBlocks(unsigned int blockSize, unsigned int numBlocks, FileCache *file, FileCache *file2) {
    unsigned int ret = 0;
    uint64_t size = blockSize * numBlocks;

    if (blockSize <= _maxSize) {
        _blockLock.lock();

        if (_size.load() + size > _maxSize) {
            TIMEON(uint64_t t1 = Timer::getCurrentTime());
            for (auto it = _blocks.begin(); it != _blocks.end();) {
                BlockEntry *entryToDelete = (*it);
                if (entryToDelete->lock.cowardlyTryWriterLock()) {
                    unsigned int retSize = 0;
                    writeBack(file2, entryToDelete, retSize);
                    writeBack(file, entryToDelete, retSize);
                    retSize += entryToDelete->decUsage(false);
                    entryToDelete->lock.writerUnlock();
                    if (retSize)
                        _size.fetch_sub(retSize);

                    FileEntry *fileEntry = entryToDelete->fileEntry;
                    it = _blocks.erase(it);

                    decBlockUsage(fileEntry);
                    TIMEON(_evictions.fetch_add(1));

                    if (_size.load() + size <= _maxSize) {
                        break;
                    }
                }
                else
                    ++it;
            }
            TIMEON(_evictionTime.fetch_add(Timer::getCurrentTime() - t1));
        }
        uint64_t freeSpace = _maxSize - _size.load();

        ret = freeSpace / blockSize;
        ret = (numBlocks < ret) ? numBlocks : ret;
        _size.fetch_add(ret * blockSize); //Can only add under lock
        _reserved_blocks.fetch_add(ret);
        _blockLock.unlock();
        if (file) {
            //*this << "Free space: " << freeSpace << " " << _size.load() << " " << _maxSize << " "<<_blocks.size()<<" "<<file->_fileName <<" "<< ret <<" "<<blockSize<<" "<<numBlocks<<" "<<size<<" "<<(_size.load() + size > _maxSize)<<" rb: "<<_reserved_blocks<< std::endl;
        }
        else {
            *this << "Free space: " << freeSpace << " " << _size.load() << " " << _maxSize << " " << _blocks.size() << " " << ret << std::endl;
        }
    }
    else
        *this << "Block is too large for cache " << blockSize << " " << _maxSize << std::endl;

    return ret;
}

void BlockCache::returnSpace(unsigned int size) {
    _size.fetch_sub(size);
}

void BlockCache::returnSpace(unsigned int size, unsigned int numBlocks) {
    _blockLock.lock();
    _size.fetch_sub(size);
    _reserved_blocks.fetch_sub(numBlocks);
    _blockLock.unlock();
}

unsigned int BlockCache::freeSpace() {
    return _size.load();
}

BlockEntry *BlockCache::createBlocks(std::string fileName, unsigned int numBlocks, unsigned int fileIndex) {
    BlockEntry *ret = NULL;
    _mapLock.lock();
    if (_files.count(fileName)) {
        FileEntry *old = _files[fileName];
        old->totalBlocks.fetch_add(1);
        ret = old->blocks;
        *this << "Reusing old blocks!" << std::endl;
    }
    else {
        FileEntry *entry = new FileEntry(fileName, numBlocks, fileIndex);
        _files[fileName] = entry;
        ret = entry->blocks;
    }
    _mapLock.unlock();
    return ret;
}

void BlockCache::printEvictions() {
    TIMEON(fprintf(stderr, "Pushes: %u Updates: %u Evictions: %u PushTime: %lu UpdateTime: %lu EvictionTime: %lu\n",
                   _pushes.load(), _updates.load(), _evictions.load(), _pushTime.load(), _updateTime.load(), _evictionTime.load()));
}
