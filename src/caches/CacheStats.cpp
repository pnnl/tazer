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

#include "CacheStats.h"
#include "Config.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>


// Note that when comparing stats for the different levels of cache, vs the "base cache"
// the base cache values might not match the other caches, this is because base provides stats from user request point of view
// not from the tazer request point of view, specifically a user request may result in multiple tazer requests (a read crosses a block boundary)

extern char *__progname;

thread_local uint64_t _depth_cs = 0;
thread_local uint64_t _current_cs[100];

char const *metricTypeName_cs[] = {
    "request",
    "prefetch"};

char const *metricName_cs[] = {
    "hits",
    "misses",
    "evictions",
    "prefetches",
    "stalls",
    "stalled",
    "ovh",
    "lock",
    "write",
    "read",
    "constructor",
    "destructor"};

CacheStats::CacheStats() {
    for (int i = 0; i < lastMetric; i++) {
        for (int j = 0; j < last; j++) {
            _time[i][j] = 0;
            _cnt[i][j] = 0;
            _amt[i][j] = 0;
        }
    }

    stdoutcp = dup(1);
    myprogname = __progname;
    _thread_stats = new std::unordered_map<std::thread::id, CacheStats::ThreadMetric*>;
}

CacheStats::~CacheStats() {
    std::unordered_map<std::thread::id, CacheStats::ThreadMetric*>::iterator itor;
    // std::stringstream ss;
    // ss << std::fixed;
    // for(int i=0; i<lastMetric; i++) {
    //     for(int j=0; j<last; j++) {
    //         ss << "[TAZER] " << metricTypeName_cs[i] << " " << metricName_cs[j] << " " << _time[i][j] / billion << " " << _cnt[i][j] << " " << _amt[i][j] << std::endl;
    //     }
    // }
    // dprintf(stdoutcp, "[TAZER] %s\n%s\n", myprogname.c_str(), ss.str().c_str());
    for(itor = _thread_stats->begin(); itor != _thread_stats->end(); itor++) {
        delete itor->second;
    }
    delete _thread_stats;
}

void CacheStats::print(std::string cacheName) {
    if (Config::printStats) {
        std::cout << cacheName << std::endl;
        std::stringstream ss;
        std::cout << std::fixed;
        for (int i = 0; i < lastMetric; i++) {
            for (int j = 0; j < last; j++) {
                std::cout << "[TAZER] " << cacheName << " " << metricTypeName_cs[i] << " " << metricName_cs[j] << " " << _time[i][j] / billion << " " << _cnt[i][j] << " " << _amt[i][j] << std::endl;
            }
            std::cout << "[TAZER] " << cacheName << " "
                      << "BW: " << (_amt[i][0] / 1000000.0) / ((_time[i][0] + _time[i][3] + _time[i][4]) / billion) << " effective BW: " << (_amt[i][7] / 1000000.0) / (_time[i][7] / billion) << std::endl;
        }
        std::cout << std::endl;

        std::unordered_map<std::thread::id, CacheStats::ThreadMetric*>::iterator itor;
        //uint64_t thread_count = 0;
        for(itor = _thread_stats->begin(); itor != _thread_stats->end(); itor++) {
            //thread_count++;
            std::cout << "[TAZER] " << cacheName << " thread " << (*itor).first << std::endl;
            for (int i = 0; i < lastMetric; i++) {
                for (int j = 0; j < last; j++) {
                    std::cout << "[TAZER] " << cacheName << " " << metricTypeName_cs[i] << " " << metricName_cs[j] << " " << itor->second->time[i][j]->load() / billion 
                     << " " << itor->second->cnt[i][j]->load() << " " << itor->second->amt[i][j]->load() << std::endl;
                }
                std::cout << "[TAZER] " << cacheName << " "
                        << "BW: " << (itor->second->amt[i][0]->load() / 1000000.0) /
                        ((itor->second->time[i][0]->load() + itor->second->time[i][3]->load() + itor->second->time[i][4]->load()) / billion)
                         << " effective BW: " << (itor->second->amt[i][7]->load() / 1000000.0) / (itor->second->time[i][7]->load() / billion) << std::endl;
            }
            std::cout << std::endl;
        }
    }

    // dprintf(stdoutcp, "[TAZER] %s\n%s\n", myprogname.c_str(), ss.str().c_str());
}

uint64_t CacheStats::getCurrentTime() {
    auto now = std::chrono::high_resolution_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
    auto value = now_ms.time_since_epoch();
    uint64_t ret = value.count();
    return ret;
}

char *CacheStats::printTime() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto buf = ctime(&t);
    buf[strcspn(buf, "\n")] = 0;
    return buf;
}

int64_t CacheStats::getTimestamp() {
    return (int64_t)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

void CacheStats::start(bool prefetch, Metric metric, std::thread::id id) {
    _current_cs[_depth_cs] = getCurrentTime();
    _depth_cs++;

    //these thread timers seem to segfault sometimes when reader lock is not used in threadStart and threadEnd
    //possibly because more than one thread can sometimes be working on the same thread id? atomics dont seem to be stopping this issue
    _lock.readerLock();
    uint64_t depth = (*_thread_stats)[id]->depth->load();
    (*_thread_stats)[id]->current[depth]->store(getCurrentTime());
    (*_thread_stats)[id]->depth->fetch_add(1);
    _lock.readerUnlock();
}

void CacheStats::end(bool prefetch, Metric metric, std::thread::id id) {
    _depth_cs--;
    CacheStats::MetricType t = prefetch ? CacheStats::MetricType::prefetch : CacheStats::MetricType::request;
    _time[t][metric].fetch_add(getCurrentTime() - _current_cs[_depth_cs]);
    _cnt[t][metric].fetch_add(1);

    _lock.readerLock();
    (*_thread_stats)[id]->depth->fetch_sub(1);
    uint64_t depth = (*_thread_stats)[id]->depth->load();
    uint64_t current = (*_thread_stats)[id]->current[depth]->load();
    (*_thread_stats)[id]->time[t][metric]->fetch_add(getCurrentTime() - current);
    (*_thread_stats)[id]->cnt[t][metric]->fetch_add(1);
    _lock.readerUnlock();
}

void CacheStats::addTime(bool prefetch, Metric metric, uint64_t time, std::thread::id id, uint64_t cnt) {
    CacheStats::MetricType t = prefetch ? CacheStats::MetricType::prefetch : CacheStats::MetricType::request;
    _time[t][metric].fetch_add(time);
    _cnt[t][metric].fetch_add(cnt);

    _lock.readerLock();
    (*_thread_stats)[id]->time[t][metric]->fetch_add(time);
    (*_thread_stats)[id]->cnt[t][metric]->fetch_add(cnt);
    _lock.readerUnlock();
}

void CacheStats::addAmt(bool prefetch, Metric metric, uint64_t amt, std::thread::id id) {
    CacheStats::MetricType t = prefetch ? CacheStats::MetricType::prefetch : CacheStats::MetricType::request;
    _amt[t][metric].fetch_add(amt);

    _lock.readerLock();
    (*_thread_stats)[id]->amt[t][metric]->fetch_add(amt);
    _lock.readerUnlock();
}

CacheStats::ThreadMetric::ThreadMetric() {
    for (int i = 0; i < 100; i++)
        current[i] = new std::atomic<uint64_t>(std::uint64_t(0));
    depth = new std::atomic<uint64_t>(std::uint64_t(0));
    for (int i = 0; i < lastMetric; i++) {
        for (int j = 0; j < last; j++) {
            time[i][j] = new std::atomic<uint64_t>(std::uint64_t(0));
            cnt[i][j] = new std::atomic<uint64_t>(std::uint64_t(0));
            amt[i][j] = new std::atomic<uint64_t>(std::uint64_t(0));
        }
    }
}

CacheStats::ThreadMetric::~ThreadMetric() {
    for (int i = 0; i < 100; i++)
        delete current[i];
    delete depth;
        for (int i = 0; i < lastMetric; i++) {
            for (int j = 0; j < last; j++) {
                delete time[i][j];
                delete cnt[i][j];
                delete amt[i][j];
            }
    }
}

void CacheStats::addThread(std::thread::id id) {
    _lock.writerLock();
    (*_thread_stats)[id] = new CacheStats::ThreadMetric();
    _lock.writerUnlock();
}

bool CacheStats::checkThread(std::thread::id id, bool addIfNotFound) {
    _lock.readerLock();
    bool ret = (_thread_stats->find(id) != _thread_stats->end());
    _lock.readerUnlock();

    if (!ret && addIfNotFound)
        addThread(id);

    return ret;
}

