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

#ifndef REQUEST_H
#define REQUEST_H
#include "Timer.h"
#include "CacheTypes.h"
#include "Loggable.h"

#include <sstream>
#include <string.h>
#include <unordered_map>
#include <thread>

class Cache;

struct RequestTrace{
    RequestTrace(std::ostream *o, bool trigger): _o(o), _trigger(trigger){ }

    template <class T>
    RequestTrace &operator<<(const T &val) {
        if (_trigger){
            *_o << val;
        }
        return *this;
    }

    RequestTrace &operator<<(std::ostream &(*f)(std::ostream &)) {
        if (_trigger){
            *_o << f;
        }
        return *this;
    }

    RequestTrace &operator<<(std::ostream &(*f)(std::ios &)) {
        if (_trigger){
            *_o << f;
        }
        return *this;
    }

    RequestTrace &operator<<(std::ostream &(*f)(std::ios_base &)) {
        if (_trigger){
            *_o << f;
        }
        return *this;
    }
    private:
    std::ostream *_o;
    bool _trigger;

};

struct Request {
    uint8_t *data;
    Cache *originating;
    uint32_t blkIndex;
    uint32_t fileIndex;
    uint64_t offset;
    uint64_t size;
    uint64_t time;
    uint64_t retryTime;
    uint64_t deliveryTime;
    std::unordered_map<Cache *, uint8_t> reservedMap;
    bool ready;
    bool printTrace;
    bool globalTrigger;
    CacheType waitingCache;
    std::unordered_map<Cache *, uint64_t> indexMap;
    std::unordered_map<Cache *, uint64_t> blkIndexMap;
    std::unordered_map<Cache *, uint64_t> fileIndexMap;
    std::unordered_map<Cache *, uint64_t> statusMap;
    uint64_t id;
    std::thread::id threadId;
    std::stringstream ss;


    Request() : data(NULL),originating(NULL),blkIndex(0),fileIndex(0), offset(0), size(0), deliveryTime(0), globalTrigger(false), threadId(std::this_thread::get_id()){ }
    Request(uint32_t blk, uint32_t fileIndex, uint64_t size, uint64_t offset, Cache *orig, uint8_t *data) : data(data), originating(orig), blkIndex(blk), fileIndex(fileIndex), 
                                                                                           offset(offset), size(size), time(Timer::getCurrentTime()), retryTime(0), deliveryTime(0), ready(false), 
                                                                                           printTrace(false),globalTrigger(false), waitingCache(CacheType::empty),id(Request::ID_CNT.fetch_add(1)), threadId(std::this_thread::get_id()) {

    }
    ~Request();
    std::string str();
    void flushTrace();


    RequestTrace trace(std::string tag = "");
    RequestTrace trace(bool trigger = true, std::string tag = "");

    private:
        
        static std::atomic<uint64_t> ID_CNT;
        static std::atomic<uint64_t> RET_ID_CNT;
};



#endif //REQUEST_H