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

#include "UrlCache.h"
#include "UrlDownload.h"
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "Message.h"
#include "ReaderWriterLock.h"
#include "Timer.h"
#include "lz4.h"
#include "xxhash.h"

#include <chrono>
#include <fcntl.h>
#include <future>
#include <memory>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// #define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

UrlCache::UrlCache(std::string cacheName, CacheType type, uint64_t blockSize) : UnboundedCache(cacheName, type, blockSize) { 
    _lock = new ReaderWriterLock();
}

UrlCache::~UrlCache() { 
    delete _lock;
}

void UrlCache::addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize) {
    log(this)<<_name<<" adding file: "<<filename<<std::endl;
    _lock->writerLock();
    if(supportedUrlType(filename)){
        if(_fileMap.count(index) == 0) {
            //We probable don't need a hash, but oh well for now...
            std::string hashstr(_name + filename);
            uint64_t hash = (uint64_t)XXH32(hashstr.c_str(), filename.size(), 0);
            _fileMap.emplace(index, FileEntry{filename, blockSize, fileSize, hash});
            
            UrlDownload * url = new UrlDownload(filename, (int)fileSize);
            if(!url->exists())
                std::cout << "WARNING: " << filename << " is not downloadable" << std::endl;
            _urlMap.emplace(index, url);
        }
       
    }
    _lock->writerUnlock();
    if(_nextLevel){
        _nextLevel->addFile(index, filename, blockSize, fileSize);
    }
}

void UrlCache::removeFile(uint32_t index){
    _lock->writerLock();
    UrlDownload * url = _urlMap[index];
    _urlMap.erase(index);
    _fileMap.erase(index);
    if(url)
        delete url;
    _lock->writerUnlock();
}

bool UrlCache::blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs) { 
    bool ret = false;
    _lock->readerLock();
    if(_urlMap.count(fileIndex))
        ret = true;
    _lock->readerUnlock();
    return ret; 
}

bool UrlCache::blockReserve(unsigned int index, unsigned int fileIndex) { 
    return true; 
}

//Gets the actual data
uint8_t *UrlCache::getBlockData(unsigned int blockIndex, unsigned int fileIndex) {
    uint8_t * buff = NULL;
    _lock->readerLock();
    UrlDownload * url = _urlMap[fileIndex];
    uint64_t blockSize = _fileMap[fileIndex].blockSize;
    uint64_t fileSize = url->size();
    unsigned int start = blockIndex * blockSize;
    unsigned int end = start + blockSize - 1;
    if(end > fileSize)
        end = fileSize - 1;
    buff = (uint8_t*) url->downloadRange(start, end);
    _lock->readerUnlock();
    return buff;
}

bool UrlCache::blockWritable(unsigned int index, unsigned int fileIndex, bool checkFs) { 
    return false; 
}

void UrlCache::setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size, unsigned int fileIndex) { 
    std::cout << "Error UrlCache setBlockData" << std::endl; 
}

bool UrlCache::blockSet(unsigned int index, unsigned int fileIndex, uint8_t byte) {
    std::cout << "Error UrlCache blockSet" << std::endl;
    return false;
}

void UrlCache::cleanUpBlockData(uint8_t *data) {
    delete[] data;
}

Cache *UrlCache::addNewUrlCache(std::string cacheName, CacheType type, uint64_t blockSize) {
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [=]() -> Cache * {
            Cache *temp = new UrlCache(cacheName, type, blockSize);
            if (temp)
                return temp;
            delete temp;
            return NULL;
        });
}