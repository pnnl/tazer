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

#ifndef READERWRITERLOCK_H
#define READERWRITERLOCK_H

#include "Request.h"
#include <atomic>
#include <thread>

class ReaderWriterLock {
  public:
    unsigned int readerLock();
    unsigned int readerUnlock();

    void writerLock();
    void fairWriterLock();
    void writerUnlock();
    void fairWriterUnlock();

    bool tryWriterLock();
    bool cowardlyTryWriterLock();
    bool tryReaderLock();
    bool cowardlyUpdgradeWriterLock();

    ReaderWriterLock();
    ~ReaderWriterLock();

  private:
    std::atomic_uint _readers;
    std::atomic_uint _writers;
    std::atomic<uint64_t> _cnt;
    std::atomic<uint64_t> _cur;
};

class MultiReaderWriterLock {
  public:
    void readerLock(uint64_t entry, Request* req = NULL);
    void readerUnlock(uint64_t entry, Request* req = NULL);

    void writerLock(uint64_t entry, Request* req = NULL);
    void fairWriterLock(uint64_t entry);
    void writerUnlock(uint64_t entry, Request* req = NULL);
    void fairWriterUnlock(uint64_t entry);
    int lockAvail(uint64_t entry, Request* req = NULL);

    MultiReaderWriterLock(uint32_t numEntries);
    MultiReaderWriterLock(uint32_t numEntries, uint8_t *dataAddr, bool init = false);
    ~MultiReaderWriterLock();
    static uint64_t getDataSize(uint64_t numEntries) {
        return (numEntries * sizeof(std::atomic<uint16_t>)) * 4;
    }

  private:
    uint32_t _numEntries;
    std::atomic<uint16_t> *_readers;
    std::atomic<uint16_t> *_writers;
    std::atomic<uint16_t> *_cnt;
    std::atomic<uint16_t> *_cur;
    uint8_t *_dataAddr;
};

#endif /* READERWRITERLOCK_H */
