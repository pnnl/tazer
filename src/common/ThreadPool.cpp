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

#include "ThreadPool.h"
#include "Timer.h"
#include "caches/Cache.h"
#include <iostream>
#include <unordered_map>

#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)

template <class T>
ThreadPool<T>::ThreadPool(unsigned int maxThreads) : _maxThreads(maxThreads),
                                                  _users(0),
                                                  _alive(true),
                                                  _currentThreads(0),_name("pool") {
}

template <class T>
ThreadPool<T>::ThreadPool(unsigned int maxThreads,std::string name) : _maxThreads(maxThreads),
                                                  _users(0),
                                                  _alive(true),
                                                  _currentThreads(0),_name(name) {
}

template <class T>
ThreadPool<T>::~ThreadPool() {
   std::cout << "[TAZER] "
              << "deleting thread pool before: "<<_name<<" " << _users << " " << _currentThreads << " " << std::endl;
    terminate(true);
    std::cout << "[TAZER] "
              << "deleting thread pool: "<<_name<<" " << _users << " " << _currentThreads << " " << std::endl;

    std::unordered_map<std::thread::id, uint64_t>::iterator itor;

    for(itor = _activeTime.begin(); itor != _activeTime.end(); ++itor){
        std::cout << "[TAZER] threadPool "<<_name<<" thread "<<(*itor).first << std::endl;
        std::cout << "[TAZER] "<<_name<<" activeTime "<<(double)(*itor).second/1000000000.0<<std::endl;
        std::cout << "[TAZER] "<<_name<<" idleTime "<<(double)_idleTime[(*itor).first]/1000000000.0<<std::endl;
    }
    
}

template <class T>
unsigned int ThreadPool<T>::addThreads(unsigned int numThreads) {
    unsigned int threadsToAdd = numThreads;
    std::unique_lock<std::mutex> lock(_tMutex);
    if (_alive.load()) {
        _users++;

        unsigned int currentThreads = _threads.size();
        if (threadsToAdd + currentThreads > _maxThreads)
            threadsToAdd = _maxThreads - currentThreads;

        // std::cout<<"adding new threads "<<threadsToAdd<<std::endl;
        _currentThreads.fetch_add(threadsToAdd);
        for (unsigned int i = 0; i < threadsToAdd; i++)
            _threads.push_back(std::thread([this] {std::cout<<"thread pool thread"<<std::endl; workLoop(); }));
    }
    lock.unlock();
    return threadsToAdd;
}

template <class T>
unsigned int ThreadPool<T>::initiate() {
    return addThreads(_maxThreads);
}

template <class T>
bool ThreadPool<T>::terminate(bool force) {
    bool ret = false;
    std::unique_lock<std::mutex> lock(_tMutex);
    if (_users) //So we can join and terminate if there are no users
        _users--;
    if (!_users || force) {
        uint64_t cur_size = _q.size();
        uint64_t timeout_cnt =0;
        while(_q.size() && timeout_cnt < 10){ //let q drain as long as it appears to be making progress otherwise time out
            timeout_cnt++;
            std::this_thread::sleep_for (std::chrono::seconds(1));
            if (cur_size != _q.size()){
                timeout_cnt=0;
                cur_size = _q.size();
            }
        }
        if (timeout_cnt >= 10) {
            std::cerr<<"[TAZER ERROR] priority thread pool: "<<_name<<" timed out with a non empty queue: "<<_q.size()<<std::endl;
        }
        //This is to deal with the conditional variable
        _alive.store(false);
        while (_currentThreads.load())
            _cv.notify_all();
        //At this point we know the threads have exited
        while (_threads.size()) {
            _threads.back().join();
            _threads.pop_back();
        }
        //if the force is called then we can't reuse the pool
        _alive.store(!force);
        ret = true;
    }
    lock.unlock();
    return ret;
}

template <class T>
void ThreadPool<T>::addTask(T f) {
    std::unique_lock<std::mutex> lock(_qMutex);
    _q.push_back(std::move(f));
    lock.unlock();
    _cv.notify_one();
}

template <class T>
void ThreadPool<T>::workLoop() {
    // std::cout<<"starting workloop "<<std::this_thread::get_id()<<std::endl;
    
    uint64_t idleSum=0;
    uint64_t activeSum=0;
    T task;
    while (_alive.load()) {
        uint64_t idleStart = Timer::getCurrentTime();
        std::unique_lock<std::mutex> lock(_qMutex);

        //Don't make this a wait or else we will never be able to join
        if (_q.empty()) {
            _cv.wait(lock);
        }

        //Check task since we are waking everyone up when it is time to join
        bool popped = !_q.empty();
        if (popped) {
            task = std::move(_q.front());
            _q.pop_front();
        }

        lock.unlock();
        idleSum +=Timer::getCurrentTime()- idleStart;

        if (popped) {
            uint64_t activeStart = Timer::getCurrentTime();
            task();
            activeSum +=Timer::getCurrentTime()- activeStart;
            popped = false;
        }
    }
    std::thread::id thread_id = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(_qMutex);
    _activeTime[thread_id]=activeSum;
    _idleTime[thread_id]=idleSum;
    lock.unlock();
    //This is the end counter we need to decrement
    _currentThreads.fetch_sub(1);
    if (_q.size() > _currentThreads){
        std::cout<<"[TAZER DEBUG] "<<_name<<" not empty while closing!!!! remaining threads: "<<_currentThreads<<" remaining tasks: "<<_q.size()<<std::endl;
    }
}

template <class T>
void ThreadPool<T>::wait() {
    bool full = true;

    while (full) {
        std::unique_lock<std::mutex> lock(_qMutex);
        full = !_q.empty();
        lock.unlock();

        if (full) {
            std::this_thread::yield();
        }
    }
}

template <class T>
unsigned int ThreadPool<T>::getMaxThreads() {
    return _maxThreads;
}

template <class T>
bool ThreadPool<T>::addThreadWithTask(T f) {
    unsigned int ret = addThreads(1);
    addTask(std::move(f));
    return (ret == 1);
}

template <class T>
int ThreadPool<T>::numTasks() {
    return _q.size();
}

template class ThreadPool<std::packaged_task<std::pair<char*,Cache*>()>>;
template class ThreadPool<std::packaged_task<std::future<std::pair<char*,Cache*>>()>>;
template class ThreadPool<std::function<void()>>;