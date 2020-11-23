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

#ifndef FcntlREADERWRITERLOCK_H
#define FcntlREADERWRITERLOCK_H
#include "ReaderWriterLock.h"
#include "UnixIO.h"
#include "Timer.h"
#include "Request.h"
#include <atomic>
#include <limits.h>
#include <thread>
#include <vector>

class FcntlReaderWriterLock {
  public:
    void readerLock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);
    void readerUnlock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);

    void writerLock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);
    int writerLock2(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);
    int writerLock3(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt, std::string blkPath);
    int lockAvail(int fd, uint64_t blk);
    int lockAvail2(int fd, uint64_t blk, std::string blkPath);

    void writerUnlock(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);
    void writerUnlock2(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt);
    void writerUnlock3(int fd, uint64_t blk, std::atomic<uint32_t> &blkCnt, std::string blkPath);

    FcntlReaderWriterLock();
    ~FcntlReaderWriterLock();

  private:
    ReaderWriterLock _lock;
    ReaderWriterLock _fdMutex;
};

class OldLock {
  public:
    void readerLock(uint64_t entry,bool log=true);
    void readerUnlock(uint64_t entry,bool log=true);

    void writerLock(uint64_t entry,bool log=true);
    void writerUnlock(uint64_t entry,bool log=true);
    int lockAvail(uint64_t entry,bool log=true);

    OldLock(uint32_t entrySize, uint32_t numEntries, std::string lockPath, std::string id);
    ~OldLock();

  private:
    ReaderWriterLock *_shmLock;
    ReaderWriterLock *_fdMutex;
    uint32_t _entrySize;
    uint32_t _numEntries;
    std::atomic<uint16_t> *_readers;
    std::atomic<uint16_t> *_writers;
    std::atomic<uint8_t> *_readerMutex;
    std::string _lockPath;
    std::string _id;
    int _fd;
};

class FcntlBoundedReaderWriterLock {
  public:
    void readerLock(uint64_t entry,Request* req,bool log=true);
    void readerUnlock(uint64_t entry,Request* req,bool log=true);

    void writerLock(uint64_t entry,Request* req, bool log=true);
    void writerUnlock(uint64_t entry,Request* req, bool log=true);
    int lockAvail(uint64_t entry,Request* req, bool log=true);

    FcntlBoundedReaderWriterLock(uint32_t entrySize, uint32_t numEntries, std::string lockPath, std::string id);
    ~FcntlBoundedReaderWriterLock();

  private:
    int lockOp(uint64_t entry, int lock_type, int op_type);
    ReaderWriterLock *_shmLock;
    ReaderWriterLock *_fdMutex;
    uint32_t _entrySize;
    uint32_t _numEntries;
    std::atomic<uint16_t> *_readers;
    std::atomic<uint16_t> *_writers;
    std::atomic<uint8_t> *_readerMutex;
    std::string _lockPath;
    std::string _id;
    int _fd;
};

#endif /* FcntlREADERWRITERLOCK_H */
