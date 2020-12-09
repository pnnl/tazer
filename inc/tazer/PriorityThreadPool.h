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

#ifndef PriorityThreadPool_H
#define PriorityThreadPool_H
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

template <class T>
class PriorityThreadPool {
  public:
    PriorityThreadPool(uint32_t maxThreads);
    PriorityThreadPool(uint32_t maxThreads,std::string name);
    ~PriorityThreadPool();

    uint32_t initiate();
    bool terminate(bool force = false);
    void wait();

    uint32_t addThreads(uint32_t numThreads);
    void addTask(uint32_t priority, T f);
    bool addThreadWithTask(uint32_t prior, T f);

    uint32_t getMaxThreads();
    int numTasks();

  private:
    struct TaskEntry {
        uint32_t priority;
        uint32_t timeStamp;
        T func;

        TaskEntry() {}

        TaskEntry(const TaskEntry &entry) {
            priority = entry.priority;
            timeStamp = entry.timeStamp;
            func = std::move(const_cast<T &>(entry.func)); // this pattern is an intricacie of move only types I dont fully understand
                                                           // found by searching for: "Broken interaction between std::priority_queue and move-only types"
        }

        TaskEntry(uint32_t prior, uint32_t time, T fun) : priority(prior),
                                                          timeStamp(time) {
            func = std::move(fun);
        }

        ~TaskEntry() {}

        TaskEntry &operator=(const TaskEntry &other) {
            if (this != &other) {
                priority = other.priority;
                timeStamp = other.timeStamp;
                func = std::move(const_cast<T &>(other.func)); // this pattern is an intricacie of move only types I dont fully understand
                                                               // found by searching for: "Broken interaction between std::priority_queue and move-only types"
            }
            return *this;
        }

        bool operator()(const TaskEntry &lhs, const TaskEntry &rhs) const {
            if (lhs.priority == rhs.priority)
                return lhs.timeStamp > rhs.timeStamp;
            return lhs.priority > rhs.priority;
        }
    };

    uint32_t _maxThreads;
    uint32_t _users;
    uint32_t _index;

    std::atomic_bool _alive;
    std::atomic_uint _currentThreads;

    std::mutex _tMutex;
    std::vector<std::thread> _threads;

    std::mutex _qMutex;
    std::priority_queue<TaskEntry, std::vector<TaskEntry>, TaskEntry> _q;
    std::condition_variable _cv;

    void workLoop();

    std::string _name;
};

#endif /* PriorityThreadPool_H */
