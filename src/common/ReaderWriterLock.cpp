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

#include "ReaderWriterLock.h"
#include "AtomicHelper.h"
#include "Request.h"
#include "Loggable.h"
#include <cstring>
#include <iostream>
#include <sys/types.h>
       #include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)

ReaderWriterLock::ReaderWriterLock() : _readers(0),
                                       _writers(0),
                                       _cnt(0),
                                       _cur(0) {
}

ReaderWriterLock::~ReaderWriterLock() {
    // std::cout << _readers << " " << _writers << " " << std::endl;
    while (_readers) {
        std::this_thread::yield();
    }
}

void ReaderWriterLock::readerLock() {
    while (1) {
        while (_writers.load()) {
            std::this_thread::yield();
        }
        _readers.fetch_add(1);
        if (!_writers.load()) {
            break;
        }
        _readers.fetch_sub(1);
    }
}

void ReaderWriterLock::readerUnlock() {
    _readers.fetch_sub(1);
}

void ReaderWriterLock::writerLock() {
    unsigned int check = 1;
    while (_writers.exchange(check) == 1) {
        std::this_thread::yield();
    }
    while (_readers.load()) {
        std::this_thread::yield();
    }
}

void ReaderWriterLock::fairWriterLock() {
    unsigned int check = 1;
    auto my_cnt = _cnt.fetch_add(1);
    while (my_cnt == _cur.load() || _writers.exchange(check) == 1) {
        std::this_thread::yield();
    }
    while (_readers.load()) {
        std::this_thread::yield();
    }
}

void ReaderWriterLock::writerUnlock() {
    _writers.store(0);
}

void ReaderWriterLock::fairWriterUnlock() {
    _writers.store(0);
    _cur.fetch_add(1);
}

bool ReaderWriterLock::tryWriterLock() {
    unsigned int check = 1;
    if (_writers.exchange(check) == 0) {
        while (_readers.load()) {
            std::this_thread::yield();
        }
        return true;
    }
    return false;
}

bool ReaderWriterLock::cowardlyTryWriterLock() {
    unsigned int check = 1;
    if (_writers.exchange(check) == 0) {
        if (_readers.load()) {
            _writers.store(0);
            return false;
        }
        return true;
    }
    return false;
}

bool ReaderWriterLock::tryReaderLock() {
    if (!_writers.load()) {
        _readers.fetch_add(1);
        if (!_writers.load())
            return true;
        _readers.fetch_sub(1);
        return false;
    }
    return false;
}

MultiReaderWriterLock::MultiReaderWriterLock(uint32_t numEntries) : _numEntries(numEntries), _dataAddr(NULL) {

    _readers = new std::atomic<uint16_t>[_numEntries];
    // memset(_readers, 0, _numEntries * sizeof(std::atomic<uint16_t>));
    init_atomic_array(_readers,_numEntries,(uint16_t)0);
    _writers = new std::atomic<uint16_t>[_numEntries];
    // memset(_writers, 0, _numEntries * sizeof(std::atomic<uint16_t>));
    init_atomic_array(_writers,_numEntries,(uint16_t)0);

    _cur = new std::atomic<uint16_t>[_numEntries];
    // memset(_cur, 0, _numEntries * sizeof(std::atomic<uint16_t>));
    init_atomic_array(_cur,_numEntries,(uint16_t)0);
    _cnt = new std::atomic<uint16_t>[_numEntries];
    // memset(_cnt, 0, _numEntries * sizeof(std::atomic<uint16_t>));
    init_atomic_array(_cnt,_numEntries,(uint16_t)0);
}

MultiReaderWriterLock::MultiReaderWriterLock(uint32_t numEntries, uint8_t *dataAddr, bool init) : _numEntries(numEntries), _dataAddr(dataAddr) {
    _readers = (std::atomic<uint16_t> *)_dataAddr;
    _writers = (std::atomic<uint16_t> *)_readers + numEntries;
    _cur = (std::atomic<uint16_t> *)_writers + numEntries;
    _cnt = (std::atomic<uint16_t> *)_cur + numEntries;
    if (init) {
        // memset(_readers, 0, _numEntries * sizeof(std::atomic<uint16_t>));
        // memset(_writers, 0, _numEntries * sizeof(std::atomic<uint16_t>));
        // memset(_cur, 0, _numEntries * sizeof(std::atomic<uint16_t>));
        // memset(_cnt, 0, _numEntries * sizeof(std::atomic<uint16_t>));
        init_atomic_array(_readers,_numEntries,(uint16_t)0);
        init_atomic_array(_writers,_numEntries,(uint16_t)0);
        init_atomic_array(_cur,_numEntries,(uint16_t)0);
        init_atomic_array(_cnt,_numEntries,(uint16_t)0);
    }
}

MultiReaderWriterLock::~MultiReaderWriterLock() {
    if (_dataAddr == NULL) {
        delete[] _readers;
        delete[] _writers;
        delete[] _cur;
        delete[] _cnt;
    }
}

void MultiReaderWriterLock::readerLock(uint64_t entry, Request* req) {
    if(req) { req->trace("MultiReaderWriterLock")<<"rlocking: "<<entry<<std::endl;}
    while (1) {
        while (_writers[entry].load()) {
            std::this_thread::yield();
        }
        _readers[entry].fetch_add(1);
        if (!_writers[entry].load()) {
            break;
        }
        _readers[entry].fetch_sub(1);
    }
    if(req) { req->trace("MultiReaderWriterLock")<<"rlocked: "<<entry<<std::endl;}
}

void MultiReaderWriterLock::readerUnlock(uint64_t entry, Request* req) {
    if(req) { req->trace("MultiReaderWriterLock")<<"runlocking: "<<entry<<std::endl; }
    _readers[entry].fetch_sub(1);
    if(req) { req->trace("MultiReaderWriterLock")<<"runlocked: "<<entry<<std::endl;}
}

void MultiReaderWriterLock::writerLock(uint64_t entry, Request* req) {
    if(req) { req->trace("MultiReaderWriterLock")<<"wlocking: "<<entry <<" "<<::getpid()<<std::endl; }
    unsigned int check = 1;
    while (_writers[entry].exchange(check) == 1) {
        std::this_thread::yield();
    }
    while (_readers[entry].load()) {
        std::this_thread::yield();
    }
    if(req) { req->trace("MultiReaderWriterLock")<<"wlocked: "<<entry <<" "<<::getpid()<<std::endl; }
}

void MultiReaderWriterLock::fairWriterLock(uint64_t entry) {
    unsigned int check = 1;
    auto my_cnt = _cnt[entry].fetch_add(1);
    while (my_cnt == _cur[entry].load() && _writers[entry].exchange(check) == 1) {
        std::this_thread::yield();
    }
    while (_readers[entry].load()) {
        std::this_thread::yield();
    }
}

void MultiReaderWriterLock::writerUnlock(uint64_t entry, Request* req) {
    if(req) { req->trace("MultiReaderWriterLock")<<"wunlocking: "<<entry <<" "<<::getpid()<<std::endl; }
    _writers[entry].store(0);
    if(req) { req->trace("MultiReaderWriterLock")<<"wunlocked: "<<entry <<" "<<::getpid()<<std::endl; }

}

void MultiReaderWriterLock::fairWriterUnlock(uint64_t entry) {
    _writers[entry].store(0);
    _cur[entry].fetch_add(1);
}
