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

#include "UrlFileCache.h"
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

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

UrlFileCache::UrlFileCache(std::string cacheName, CacheType type) : LocalFileCache(cacheName,type) {
    _lock = new ReaderWriterLock();
}

UrlFileCache::~UrlFileCache() {
}

Cache *UrlFileCache::addNewUrlFileCache(std::string cacheName, CacheType type) {
    return Trackable<std::string, Cache *>::AddTrackable(
        cacheName, [&]() -> Cache * {
            Cache *temp = new UrlFileCache(cacheName, type);
            return temp;
        });
}

void UrlFileCache::cleanUpBlockData(uint8_t *data) {
    // debug()<<_name<<"(not) delete data"<<std::endl;
}

void UrlFileCache::addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize) {
    std::string name = filename;
    if(checkUrlPath(filename)) {
        name = downloadUrlPath(filename);
        if(Config::deleteDownloads) {
            _lock->writerLock();
            _urlMap.emplace(index, name);
            _lock->writerUnlock();
        }
    }
    LocalFileCache::addFile(index, name, blockSize, fileSize);
}

void UrlFileCache::removeFile(uint32_t index) {
    if(Config::deleteDownloads) {
        _lock->writerLock();

        auto it = _urlMap.find(index);
        if (it != _urlMap.end()) {
            remove(it->second.c_str());
            _urlMap.erase (it);
        }
        _lock->writerUnlock();
    }
    LocalFileCache::removeFile(index);
}