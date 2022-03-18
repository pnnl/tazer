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

#ifndef CACHESTATS_H
#define CACHESTATS_H

#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ReaderWriterLock.h"

class CacheStats {
  public:
    enum MetricType {
        request = 0,
        prefetch,
        lastMetric
    };

    enum Metric {
        hits = 0,
        misses,
        evictions,
        backout,
        prefetches,
        stalls,  //time i stall other caches
        stalled, //time im stalled
        ovh,
        lock,
        write,
        read,
        constructor,
        destructor,
        last
    };

    CacheStats();
    ~CacheStats();

    void print(std::string cacheName);

    void start(bool prefetch, Metric metric, std::thread::id id=std::this_thread::get_id());
    void end(bool prefetch, Metric metric, std::thread::id id=std::this_thread::get_id());
    void addTime(bool prefetch, Metric metric, uint64_t time, std::thread::id id=std::this_thread::get_id(), uint64_t cnt = 0);
    void addAmt(bool prefetch, Metric metric, uint64_t mnt, std::thread::id id=std::this_thread::get_id());
    // void threadStart(std::thread::id id);
    // void threadEnd(std::thread::id id, bool prefetch, Metric metric);
    // void threadAddTime(std::thread::id id, bool prefetch, Metric metric, uint64_t time, uint64_t cnt = 0);
    // void threadAddAmt(std::thread::id id, bool prefetch, Metric metric, uint64_t mnt);
  
    

    static uint64_t getCurrentTime();
    static char *printTime();
    static int64_t getTimestamp();

  private:
    class ThreadMetric {
      public:
        ThreadMetric();
        ~ThreadMetric();
        std::atomic<uint64_t> *depth;
        std::atomic<uint64_t> *current[100];
        std::atomic<uint64_t> *time[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
        std::atomic<uint64_t> *cnt[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
        std::atomic<uint64_t> *amt[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
    };

    void addThread(std::thread::id id);
    // bool checkThread(std::thread::id id, bool addIfNotFound);
    const double billion = 1000000000;
    std::atomic<uint64_t> _time[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
    std::atomic<uint64_t> _cnt[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
    std::atomic<uint64_t> _amt[CacheStats::MetricType::lastMetric][CacheStats::Metric::last];
    std::unordered_map<std::thread::id, CacheStats::ThreadMetric*> *_thread_stats;
    ReaderWriterLock _lock;

    int stdoutcp;
    std::string myprogname;
};

#endif /* CACHESTATS_H */