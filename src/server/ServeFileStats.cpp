
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

#include "ServeFileStats.h"
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

thread_local uint64_t _depth_ss = 0;
thread_local uint64_t _current_ss[100];

char const *metricName_ss[] = {
    "send",
    "constructor",
    "destructor"};

ServeFileStats::ServeFileStats() {
    for (int i = 0; i < last; i++) {
        _time[i] = new std::atomic<uint64_t>(std::uint64_t(0));
        _cnt[i] = new std::atomic<uint64_t>(std::uint64_t(0));
        _amt[i] = new std::atomic<uint64_t>(std::uint64_t(0));
    }

    stdoutcp = dup(1);
    myprogname = __progname;
    _thread_stats = new std::unordered_map<std::thread::id, ServeFileStats::ThreadMetric*>;
}

ServeFileStats::~ServeFileStats() {
    std::unordered_map<std::thread::id, ServeFileStats::ThreadMetric*>::iterator itor;

    print();

    for (int i = 0; i < last; i++) {
        delete _time[i];
        delete _cnt[i];
        delete _amt[i];
    }


    for(itor = _thread_stats->begin(); itor != _thread_stats->end(); itor++) {
        delete itor->second;
    }
    delete _thread_stats;
}

void ServeFileStats::print() {
    if (Config::printStats) {
        std::cout << std::endl;
        std::cout << "servefile" << std::endl;
        std::stringstream ss;
        std::cout << std::fixed;
        for (int i = 0; i < last; i++) {
            std::cout << "[TAZER] " << "servefile" << " " << metricName_ss[i] << " " << _time[i]->load() / billion << " " << _cnt[i]->load() << " " << _amt[i]->load() << std::endl;
        }
        std::cout << std::endl;

        std::unordered_map<std::thread::id, ServeFileStats::ThreadMetric*>::iterator itor;
        for(itor = _thread_stats->begin(); itor != _thread_stats->end(); itor++) {
            std::cout << "[TAZER] " << "servefile" << " thread " << (*itor).first << std::endl;
            for (int i = 0; i < last; i++) {
                std::cout << "[TAZER] " << "servefile" << " " << metricName_ss[i] << " " << itor->second->time[i]->load() / billion 
                     << " " << itor->second->cnt[i]->load() << " " << itor->second->amt[i]->load() << std::endl;
            }
            std::cout << std::endl;
        }
    }

    // dprintf(stdoutcp, "[TAZER] %s\n%s\n", myprogname.c_str(), ss.str().c_str());
}

uint64_t ServeFileStats::getCurrentTime() {
    auto now = std::chrono::high_resolution_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::nanoseconds>(now);
    auto value = now_ms.time_since_epoch();
    uint64_t ret = value.count();
    return ret;
}

char *ServeFileStats::printTime() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto buf = ctime(&t);
    buf[strcspn(buf, "\n")] = 0;
    return buf;
}

int64_t ServeFileStats::getTimestamp() {
    return (int64_t)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

void ServeFileStats::start(Metric metric, std::thread::id id) {
    _current_ss[_depth_ss] = getCurrentTime();
    _depth_ss++;

    uint64_t depth = (*_thread_stats)[id]->depth->load();
    (*_thread_stats)[id]->current[depth]->store(getCurrentTime());
    (*_thread_stats)[id]->depth->fetch_add(1);
}

void ServeFileStats::end(Metric metric, std::thread::id id) {
    _depth_ss--;
    _time[metric]->fetch_add(getCurrentTime() - _current_ss[_depth_ss]);
    _cnt[metric]->fetch_add(1);

    (*_thread_stats)[id]->depth->fetch_sub(1);
    uint64_t depth = (*_thread_stats)[id]->depth->load();
    uint64_t current = (*_thread_stats)[id]->current[depth]->load();
    (*_thread_stats)[id]->time[metric]->fetch_add(getCurrentTime() - current);
    (*_thread_stats)[id]->cnt[metric]->fetch_add(1);
}

void ServeFileStats::addTime(Metric metric, uint64_t time, std::thread::id id, uint64_t cnt) {
    _time[metric]->fetch_add(time);
    _cnt[metric]->fetch_add(cnt);

    (*_thread_stats)[id]->time[metric]->fetch_add(time);
    (*_thread_stats)[id]->cnt[metric]->fetch_add(cnt);
}

void ServeFileStats::addAmt(Metric metric, uint64_t amt, std::thread::id id) {
    _amt[metric]->fetch_add(amt);
    (*_thread_stats)[id]->amt[metric]->fetch_add(amt);
}

ServeFileStats::ThreadMetric::ThreadMetric() {
    for (int i = 0; i < 100; i++)
        current[i] = new std::atomic<uint64_t>(std::uint64_t(0));
    depth = new std::atomic<uint64_t>(std::uint64_t(0));
    for (int i = 0; i < last; i++) {
        time[i] = new std::atomic<uint64_t>(std::uint64_t(0));
        cnt[i] = new std::atomic<uint64_t>(std::uint64_t(0));
        amt[i] = new std::atomic<uint64_t>(std::uint64_t(0));
    }
}

ServeFileStats::ThreadMetric::~ThreadMetric() {
    for (int i = 0; i < 100; i++)
        delete current[i];
    delete depth;
    for (int i = 0; i < last; i++) {

        delete time[i];
        delete cnt[i];
        delete amt[i];
    }
}

void ServeFileStats::addThread(std::thread::id id) {
    _lock.writerLock();
    (*_thread_stats)[id] = new ServeFileStats::ThreadMetric();
    _lock.writerUnlock();
}

bool ServeFileStats::checkThread(std::thread::id id, bool addIfNotFound) {
    _lock.readerLock();
    bool ret = (_thread_stats->find(id) != _thread_stats->end());
    _lock.readerUnlock();

    if (!ret && addIfNotFound)
        addThread(id);

    return ret;
}
