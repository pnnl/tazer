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

#ifndef SERVEFILE_H_
#define SERVEFILE_H_

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// #include "BlockCache.h"
#include "Cache.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "Loggable.h"
#include "ReaderWriterLock.h"
#include "ThreadPool.h"
#include "Trackable.h"

class ServeFile : public Loggable, public Trackable<std::string, ServeFile *> {
  public:
    ServeFile(std::string name, bool compress, uint64_t blkSize, uint64_t initialCompressTasks, bool output = false, bool remove = false);
    ~ServeFile();

    bool transferBlk(Connection *connection, uint32_t blk);
    bool writeData(char *data, uint64_t size, uint64_t fp);

    std::string name();
    uint64_t size();
    uint64_t blkSize();
    bool compress();
    bool open();

    static ServeFile *addNewServeFile(std::string name, bool compress, uint64_t blkSize, uint64_t initialCompressTask, bool output, bool remove);
    static ServeFile *getServeFile(std::string fileName);
    static bool removeServeFile(std::string fileName);
    static bool removeServeFile(ServeFile *file);

    static void cache_init(void);

  private:
    static bool addConnections();
    uint64_t compress(uint64_t blk, uint8_t *blkData, uint8_t *&msg);
    void addCompressTask(uint32_t blk);
    bool sendData(Connection *connection, uint64_t blk, Request *request);

    std::string _name;
    bool _output;
    bool _remove;
    bool _compress;
    uint64_t _blkSize;
    uint64_t _maxCompSize;
    int _compLevel;
    uint32_t _initialCompressTasks;

    uint64_t _size;
    uint64_t _numBlks;
    bool _open;
    bool _url;

    ReaderWriterLock _prefetchLock;

    std::mutex _fileMutex;
    std::atomic<uint64_t> _outstandingWrites;

    static Cache _cache;
    uint32_t _regFileIndex;
    static ThreadPool<std::function<void()>> _pool;

    static std::vector<Connection *> _connections;
    static PriorityThreadPool<std::packaged_task<std::shared_future<Request *>()>> _transferPool;
    static PriorityThreadPool<std::packaged_task<Request *()>> _decompressionPool;
    // ConnectionPool *_conPool;
};

#endif /* SERVEFILE_H_ */
