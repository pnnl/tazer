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

//#includ << "[TAZER]"
//#include <sstream>
//#include <thread>
//#include <fcntl.h>
//
//#include <unistd.h>
//#include <sys/stat.h>
//#include <assert.h>
//
//#include <hpcrun-fmt.h>
//#include <hpcio-buffer.h>
//
//#include "CounterData.h"
//#include "Connection.h"
//
//
////TODO:
////1. do per node counters, along with a max cnt seen.
////2. ensure time stamps are unique
////3. do normalization based on host architecture (each node type will have a specific normalization scalar)
//
//
//CounterData::CounterData(std::string name, std::string path, uint32_t type, bool debugLog):
//    _name(name),
//    _path(path),
//    _type(type),
//    _cnt(0),
//    _log(debugLog) {
//    if (debugLog) {
//        std::stringstream ss;
//        const char* env_p = std::getenv("IPPD_LOG_PATH");
//        if (env_p == NULL) {
//            ss << "./";
//        } else {
//            ss << env_p;
//        }
//        ss << _name.substr(0, _name.find(".meta")) << ".ippdlog";
//        std::string tstr;
//        std::stringstream tss;
//        while (getline(ss, tstr, '/')) { //construct results path if not exists
//            if (tstr.find(".ippdlog") == std::string::npos) {
//                std::cerr<<"[TAZER] " << tstr << std::endl;
//                tss << tstr << "/";
//                mkdir(tss.str().c_str(), 0777);
//            }
//        }
//        _of.open(ss.str(), std::ofstream::out);
//    }
//    initializeMaxLoads();
//    _prevTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();
//    std::stringstream ss;
//    ss << _path;
//    std::string tstr;
//    std::stringstream tss;
//    while (getline(ss, tstr, '/')) { //construct results path if not exists
//        if (!ss.eof()) {
//            tss << tstr << "/";
//            //*this <<tss.str()<<std::endl;
//            mkdir(tss.str().c_str(), 0777);
//        }
//
//    }
//    _absLog.open(_path + "/" + _name + ".txt");
//    _normLog.open(_path + "/" + "normalized_" + _name + ".txt");
//    _thread = std::thread(&CounterData::logThread, this);
//}
//
//CounterData::~CounterData() {
//    std::unique_lock<std::mutex> lock(_mutex);
//    //LogTask task = {std::chrono::high_resolution_clock::now(), -1, -1, -1, "-1"};
//    LogTask task = {std::chrono::high_resolution_clock::now().time_since_epoch().count(), -1, -1, -1, -1, -1, -1, "-1", "-1"};
//
//    _q.push(task);
//    _cv.notify_all();
//    lock.unlock();
//    close();
//    _thread.join();
//}
//
//void CounterData::initializeMaxLoads() {
//    /***** IVY bridge nodes *****/
//    for (int i = 1; i < 10; i++) {
//        _maxLoads["node0" + std::to_string(i)+".cluster.pnnl.gov"] = 10;
//    }
//    for (int i = 10; i < 33; i++) {
//        _maxLoads["node" + std::to_string(i)+".cluster.pnnl.gov"] = 10;
//    }
//    for (int i = 49; i < 51; i++) {
//        _maxLoads["node" + std::to_string(i)+".cluster.pnnl.gov"] = 10;
//    }
//
//    /***** AMD nodes *****/
//    for (int i = 33; i < 49; i++) {
//        _maxLoads["node" + std::to_string(i)+".cluster.pnnl.gov"] = 2;
//    }
//
//    /***** Haswell nodes *****/
//
//    _maxLoads["node51.cluster.pnnl.gov"] = 8;
//    _maxLoads["node52.cluster.pnnl.gov"] = 16;
//}
//
//void CounterData::logThread() {
//    bool done = false;
//    while (!done) {
//        std::unique_lock<std::mutex> lock(_lMutex);
//        while (_q.empty()) {
//            _cv.wait(lock);
//        }
//        LogTask task = _q.front();
//        _q.pop();
//        lock.unlock();
//        if (task.pid >= 0) {
//            _absLog << task.pid << " " << task.clientId << " " << task.time << " " << task.fileCntServer << std::endl;
//            _normLog << task.pid << " " << task.clientId << " " << task.time << " " << task.taskCntServer << std::endl;
//        }
//        else {
//            done = true;
//        }
//    }
//
//
//}
//
////===========================================================================
//
//void CounterData::writeTraceHeader(hpcio_outbuf_t* outbuf) {
//
//    const int header_sz = 4 + 4;
//
//    const int indexEntry_sz = 4 + 4 + 8;
//    const int index_sz = indexEntry_sz * _nodeLogs.size();
//
//    const int buf_sz = header_sz + index_sz;
//    unsigned char buf[buf_sz];
//
//    *this << "# nodes" << _nodeLogs.size() << std::endl;
//    //========================================================
//    // Header
//    //   app type   (4 bytes): (0: unknown, 1: mpi, 2: openmp, 3: hybrid
//    //   num traces (4 bytes):
//    //========================================================
//
//    int k = 0;
//    int shift = 0;
//
//    uint32_t app_ty = 1;
//    for (shift = 24; shift >= 0; shift -= 8) {
//        buf[k] = (app_ty >> shift) & 0xff;
//        k++;
//    }
//
//    *this << "here0" << std::endl;
//    uint32_t n_trace = _nodeLogs.size();
//    for (shift = 24; shift >= 0; shift -= 8) {
//        buf[k] = (n_trace >> shift) & 0xff;
//        k++;
//    }
//
//    *this << "here1" << std::endl;
//
//    //========================================================
//    // Index
//    //   for each trace:
//    //     proc-id (4 bytes)
//    //     thread-id (4 bytes)
//    //     offset in file (8 bytes)
//    //
//    // Thus:
//    // - 1st index value: header_sz + index_sz
//    // - 2nd index value: header_sz + index_sz + length(first trace)...
//    //========================================================
//
//
//    uint64_t trace_offset = header_sz + index_sz;
//    uint32_t node_id = 0;
//    for (auto node : _nodeLogs) {
//        //*this << "node: " << node.first << " " << node.second.size() << std::endl;
//        uint32_t proc_id = node_id++;
//        for (shift = 24; shift >= 0; shift -= 8) {
//            buf[k] = (proc_id >> shift) & 0xff;
//            k++;
//        }
//        //*this << "here2" << std::endl;
//        uint32_t thread_id = 0;
//        for (shift = 24; shift >= 0; shift -= 8) {
//            buf[k] = (thread_id >> shift) & 0xff;
//            k++;
//        }
//        //*this << "here3" << std::endl;
//        uint64_t trace_idx = trace_offset;
//        for (shift = 56; shift >= 0; shift -= 8) {
//            buf[k] = (trace_idx >> shift) & 0xff;
//            k++;
//        }
//        //*this << "here4" << std::endl;
//        uint64_t trace_sz = (HPCTRACE_FMT_HeaderLen + node.second.size() * sizeof(hpctrace_fmt_datum_t));
//
//        trace_offset += trace_sz;
//        *this << "nodeid: " << node_id << " trace offset: " << trace_offset << " trace_sz: " << node.second.size() << " #nodes: " << _nodeLogs.size() << std::endl;
//    }
//
//    //========================================================
//    // Write Header + Index
//    //========================================================
//
//    assert(k == buf_sz);
//    if (hpcio_outbuf_write(outbuf, buf, k) != k) {
//        // error;
//    }
//
//}
//
//void CounterData::writeTraceEndMarker(hpcio_outbuf_t* outbuf) {
//    uint64_t marker = 0xFFFFFFFFDEADF00D;
//    const int buf_sz = sizeof(uint64_t);
//    unsigned char buf[buf_sz];
//    int k = 0;
//    int shift = 0;
//
//    for (shift = 56; shift >= 0; shift -= 8) {
//        buf[k] = (marker >> shift) & 0xff;
//        k++;
//    }
//
//
//    //=======================================================
//    // write end marker
//    //=======================================================
//    assert(k == buf_sz);
//    if (hpcio_outbuf_write(outbuf, buf, k) != k) {
//        // error;
//    }
//
//}
//
//
////===========================================================================
//
//void CounterData::writeLog() {
//
//    std::string name = _path + "/" + _name + "." + HPCRUN_TraceFnmSfx;
//    int fd = open(name.c_str(), O_WRONLY | O_CREAT | O_TRUNC /*| O_EXCL*/, 0644);
//    if (fd < 0) { /*error*/ }
//
//
//    hpcio_outbuf_t trace_outbuf;
//
//    void* trace_buffer = malloc(HPCIO_RWBufferSz);
//
//    int ret = hpcio_outbuf_attach(&trace_outbuf, fd, trace_buffer,
//                                  HPCIO_RWBufferSz, HPCIO_OUTBUF_UNLOCKED);
//
//    *this << "creating log file for " << name << std::endl;
//
//    //std::ofstream log;
//
//    //log.open(_path+"/"+_name+"_noderows.txt");
//    //normLog.open(_path+"/"+"normalized_" + _name + "_noderows.txt");
//
//    writeTraceHeader(&trace_outbuf);
//
//
//    uint64_t minTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();
//    uint64_t maxTime = 0;
//
//    std::ofstream log;
//    log.open(_path + "/" + _name + "_time_bounds.txt");
//    //log << maxTime << " " << maxTime << "                                               " << std::endl;
//
//    for (auto node : _nodeLogs) {
//        hpctrace_hdr_flags_t flags = hpctrace_hdr_flags_NULL;
//        flags.fields.isDataCentric = true;
//        ret = hpctrace_fmt_hdr_outbuf(flags, &trace_outbuf);
//
//        //log << node.first;
//        for (auto entry : node.second) {
//            hpctrace_fmt_datum_t trace_datum;
//            trace_datum.time = (uint64_t)entry.time; // 64-bit timestamp, gettimeofday
//            trace_datum.cpId = (uint32_t)entry.remote_pid;
//            trace_datum.metricId = (uint32_t)_type;
//            double maxLoad = 1.0;
//            if (_maxLoads.count(entry.hostname) > 0) {
//                maxLoad = _maxLoads[entry.hostname];
//            }
//            //*this << "ml: " << maxLoad << " " << entry.hostname << std::endl;
//            double load = (double)entry.taskCntServer / maxLoad;
//            trace_datum.time2 = *reinterpret_cast<uint64_t *>(&load);
//            load = (double)entry.taskCntNode / maxLoad;
//            trace_datum.time3 = *reinterpret_cast<uint64_t *>(&load);
//
//            //*this <<std::hex<<trace_datum.time <<" "<<trace_datum.cpId <<" "<<trace_datum.metricId <<" "<< trace_datum.time2<<" "<< trace_datum.time3<<std::endl;
//            //*this <<std::dec<<trace_datum.time <<" "<<trace_datum.cpId <<" "<<trace_datum.metricId <<" "<< trace_datum.time2<<" "<< trace_datum.time3<<std::endl;
//
//            //*this << load<<" "<<(uint64_t)load<<" "<<entry.taskCntServer<<" "<< entry.taskCntNode<<" "<<entry.fileCntServer<<std::endl;
//
//            hpctrace_hdr_flags_t flags = hpctrace_hdr_flags_NULL;
//            flags.fields.isDataCentric = true;
//            ret = hpctrace_fmt_datum_outbuf(&trace_datum, flags, &trace_outbuf);
//            //log << trace_datum.time << " " << trace_datum.time2 << "(" << *reinterpret_cast<double*>(&trace_datum.time2) << ") " << trace_datum.time3 << "(" << *reinterpret_cast<double*>(&trace_datum.time3) << ")" << std::endl;
//
//            if (trace_datum.time < minTime) {
//                minTime = trace_datum.time;
//            }
//            if (trace_datum.time > maxTime) {
//                maxTime = trace_datum.time;
//            }
//
//            //log <<entry.time<<" "<<entry.fileCntServer<<" "<<entry.taskCntServer<<" "<<entry.taskCntNode<<" "<<entry.pid<<" "<<entry.remote_pid<<" "<<entry.hostid<<" "<<entry.clientId<<std::endl;
//            //log <<" <"<< entry.time.time_since_epoch().count()<<" "<<entry.remote_pid << " " <<entry.absCnt<<" "<<entry.normCnt<<">";
//        }
//        log << std::endl;
//    }
//    writeTraceEndMarker(&trace_outbuf);
//    ret = hpcio_outbuf_close(&trace_outbuf);
//    //log.seekp(0);
//    log << minTime << " " << maxTime << std::endl;
//    log.close();
//    //log.close();
//}
//
//
//// void CounterData::addHost(long hostid) { //maybe add normalization value?
////     if (_nodeCounts.count(hostid)==0){
////         _nameCounts[hostid]
////     }
//// }
//
//
//void CounterData::increment(std::string clientId, long hostid) {
//
//    std::unique_lock<std::mutex> lock(_mutex);
//    //*this <<"incrementing: "<<_name<<" "<<clientId<<" "<<_currentProcesses.size()<<std::endl;
//    _currentProcesses.insert(clientId);
//    _cnt++;
//    _nodeCounts[hostid].insert(clientId);
//    //*this <<"incremented: "<<hostid<<" "<<clientId<<" "<<_cnt<<" "<<_currentProcesses.size()<<" "<<_nodeCounts[hostid].size()<<std::endl;
//    lock.unlock();
//}
//
//
//
//void CounterData::decrement(std::string clientId, long hostid) {
//
//    std::unique_lock<std::mutex> lock(_mutex);
//    //*this <<"decrementing: "<<_name<<" "<<clientId<<" "<<_currentProcesses.size()<<std::endl;
//    _currentProcesses.erase(clientId);
//    //*this <<"decremented: "<<_name<<" "<<clientId<<" "<<_currentProcesses.size()<<std::endl;
//    _cnt--;
//    _nodeCounts[hostid].erase(clientId);
//    lock.unlock();
//}
//
//void CounterData::increment(std::unordered_set<Connection*> connections) {
//
//    std::unique_lock<std::mutex> lock(_mutex);
//    _cnt++;
//    //*this <<"incrementing: "<<_name<<" "<<clientId<<" "<<_currentProcesses.size()<<std::endl;
//    for (auto connection : connections) {
//        _currentProcesses.insert(connection->clientId());
//
//        _nodeCounts[connection->hostId()].insert(connection->clientId());
//    }
//    //*this <<"incremented: "<<hostid<<" "<<clientId<<" "<<_cnt<<" "<<_currentProcesses.size()<<" "<<_nodeCounts[hostid].size()<<std::endl;
//    lock.unlock();
//}
//
//void CounterData::decrement(std::unordered_set<Connection*> connections) {
//
//    std::unique_lock<std::mutex> lock(_mutex);
//    _cnt--;
//    for (auto connection : connections) {
//        //*this <<"decrementing: "<<_name<<" "<<clientId<<" "<<_currentProcesses.size()<<std::endl;
//        _currentProcesses.erase(connection->clientId());
//        //*this <<"decremented: "<<_name<<" "<<clientId<<" "<<_currentProcesses.size()<<std::endl;
//        _nodeCounts[connection->hostId()].erase(connection->clientId());
//    }
//    lock.unlock();
//}
//
//
//void CounterData::reset() {
//    _absLog << "start - " << std::chrono::high_resolution_clock::now().time_since_epoch().count() << std::endl;
//    _normLog << "start - " << std::chrono::high_resolution_clock::now().time_since_epoch().count() << std::endl;
//    _cnt = 0;
//    _currentProcesses.clear();
//}
//
//void CounterData::close() {
//    std::unique_lock<std::mutex> lock(_mutex);
//    _absLog.close();
//    _normLog.close();
//    lock.unlock();
//}
//
//// void CounterData::writeToLog(int pid, std::string clientId) {
////  int normCnt;
////  int cnt;
////  std::unique_lock<std::mutex> lock(_mutex);
////  normCnt = _currentProcesses.size();
////  cnt=_cnt;
////  lock.unlock();
//
////  std::unique_lock<std::mutex> lock1(_lMutex);
////  LogTask task = {std::chrono::high_resolution_clock::now(), _cnt, _currentProcesses.size(), pid, clientId};
////  _q.push(task);
////  _cv.notify_all();
////  lock1.unlock();
//// }
//
//
//
//void CounterData::writeToLog(int pid, std::string hostname, long hostid, long remote_pid) {
//    int fileCntServer;
//    int taskCntServer;
//    int taskCntNode;
//    std::unique_lock<std::mutex> lock(_mutex);
//    taskCntServer = _currentProcesses.size();
//    fileCntServer = _cnt;
//    taskCntNode = _nodeCounts[hostid].size();
//    lock.unlock();
//
//
//
//    std::unique_lock<std::mutex> lock1(_lMutex);
//    uint64_t time = std::chrono::high_resolution_clock::now().time_since_epoch().count();
//    if (time == _prevTime) {
//        time++;
//    }
//    _prevTime = time;
//    LogTask task = {time, fileCntServer, taskCntServer, taskCntNode, pid, remote_pid, hostid, hostname, hostname + std::to_string(remote_pid)};
//    _nodeLogs[hostid].insert(task);
//    _q.push(task);
//    _cv.notify_all();
//    lock1.unlock();
//}
//
//
//void CounterData::writeToLog(std::unordered_set<Connection*> connections) {
//    int fileCntServer;
//    int taskCntServer;
//    int taskCntNode;
//    std::unique_lock<std::mutex> lock(_mutex);
//    taskCntServer = _currentProcesses.size();
//    fileCntServer = _cnt;
//    lock.unlock();
//
//
//
//    std::unique_lock<std::mutex> lock1(_lMutex);
//
//    uint64_t time = std::chrono::high_resolution_clock::now().time_since_epoch().count();
//    for (auto connection : connections){
//        if (time == _prevTime) {
//            time++;
//        }
//        _prevTime = time;
//        LogTask task = {time, fileCntServer, taskCntServer, _nodeCounts[connection->hostId()].size(), connection->id(), connection->ppid(), connection->hostId(), connection->hostname(), connection->hostname() + std::to_string(connection->ppid())};
//        _nodeLogs[connection->hostId()].insert(task);
//        _q.push(task);
//    }
//    _cv.notify_all();
//
//    lock1.unlock();
//}
//
//
//
//
