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

#ifndef COUNTERDATA_H_
#define COUNTERDATA_H_

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <hpcrun-fmt.h>

#include "Connection.h"

#define NET 0xA
#define DISK 0xB
#define CONNECTION 0xC

class CounterData {
  public:
    CounterData(std::string name, std::string path, uint32_t type, bool log);
    ~CounterData();
    void increment(std::string clientId, long hostid);
    void decrement(std::string clientId, long hostid);
    void increment(long hostid);
    void decrement(long hostid);

    void increment(std::unordered_set<Connection *> connections);
    void decrement(std::unordered_set<Connection *> connections);

    void reset();
    void close();
    //void writeToLog(int pid,std::string clientId);
    void writeLog();
    void writeToLog(int pid, std::string hostname, long hostid, long remote_pid);
    void writeToLog(std::unordered_set<Connection *> connections);

    template <class T>
    CounterData &operator<<(const T &val) {
        if (_log && _of.is_open()) {
            _of << val;
        }
        std::cerr << "[TAZER]" << val;
        return *this;
    }

    CounterData &operator<<(std::ostream &(*f)(std::ostream &)) {
        if (_log && _of.is_open()) {
            f(_of);
        }
        f(std::cerr << "[TAZER]");
        return *this;
    }

    CounterData &operator<<(std::ostream &(*f)(std::ios &)) {
        if (_log && _of.is_open()) {
            f(_of);
        }
        f(std::cerr << "[TAZER]");
        return *this;
    }

    CounterData &operator<<(std::ostream &(*f)(std::ios_base &)) {
        if (_log && _of.is_open()) {
            f(_of);
        }
        f(std::cerr << "[TAZER]");
        return *this;
    }

  private:
    void logThread();
    void writeTraceHeader(hpcio_outbuf_t *outbuf);
    void writeTraceEndMarker(hpcio_outbuf_t *outbuf);

    void initializeMaxLoads();

    std::string _name;
    std::string _path;
    int _cnt;
    std::unordered_set<std::string> _currentProcesses;
    std::ofstream _absLog;
    std::ofstream _normLog;
    int _nfd; //normalized
    bool _log;
    std::ofstream _of;

    uint32_t _type;

    struct LogTask {
        uint64_t time;
        int fileCntServer;
        int taskCntServer;
        int taskCntNode;
        int pid;
        long remote_pid;
        long hostid;
        std::string hostname;
        std::string clientId;
    };

    struct LogTaskCompare {
        bool operator()(const LogTask &lhs, const LogTask &rhs) {
            return (lhs.time < rhs.time);
        }
    };

    std::unordered_map<long, std::unordered_set<std::string>> _nodeCounts;
    std::unordered_map<long, long> _nodeMaxs;

    std::unordered_map<long, std::multiset<LogTask, LogTaskCompare>> _nodeLogs;

    uint64_t _prevTime;

    // log thread
    std::thread _thread;
    std::mutex _mutex;
    std::mutex _lMutex;
    std::condition_variable _cv;
    std::queue<LogTask> _q;

    std::unordered_map<std::string, int> _maxLoads;
};

#endif /* COUNTERDATA_H_ */