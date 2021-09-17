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

#ifndef TRACKABLE_H
#define TRACKABLE_H

#include "ReaderWriterLock.h"
#include <algorithm>
#include <atomic>
#include <functional>
#include <typeinfo>
#include <unordered_map>

template <class Key, class Value>
class Trackable {
  public:
    int id() {
        return _id;
    }

  protected:
    Trackable() : _id(_total.fetch_add(1)),
                  _users(0) {
    }

    ~Trackable() {}

    void incUsers() {
        _users.fetch_add(1);
    }

    bool decUsers(unsigned int dec) {
        return (1 == _users.fetch_sub(dec));
    }

    static Value AddTrackable(Key k, std::function<Value(void)> createNew, std::function<void(Value)> reuseOld, bool &created) {
        Value ret = NULL;
        _activeMutex.writerLock();
        if (!_active.count(k)) {
            ret = createNew();
            if (ret) {
                _active[k] = ret;
                created = true;
            }
        }
        else {
            ret = _active[k];
            if (reuseOld)
                reuseOld(ret);
            // if (ret){
            //     printf("reusing %s\n",typeid(ret).name());
            // }
        }

        if (ret)
            ret->incUsers();

        _activeMutex.writerUnlock();
        return ret;
    }

    static Value AddTrackable(Key k, std::function<Value(void)> createNew, bool &created) {
        return AddTrackable(k, createNew, NULL, created);
    }

    static Value AddTrackable(Key k, std::function<Value(void)> createNew) {
        bool dontCare;
        return AddTrackable(k, createNew, NULL, dontCare);
    }

    static bool RemoveTrackable(Key k, unsigned int dec = 1) {
        // printf("in remove trackable %d\n",_active.count(k));
        bool ret = false;
        _activeMutex.writerLock();
        if (_active.count(k)) {
            Value toDelete = _active[k];
            if (toDelete->decUsers(dec)) {
                // fprintf(stderr,"trackable delete %s\n",typeid(toDelete).name());
                //std::cout<<"[TAZER] " << "Deleting Trackable!!! " <<typeid(toDelete).name()<< std::endl;
                _active.erase(k);
                delete toDelete;
                ret = true;
            }
        }
        _activeMutex.writerUnlock();
        return ret;
    }

    static Value LookupTrackable(Key k) {
        Value ret = NULL;
        _activeMutex.readerLock();
        if (_active.count(k))
            ret = _active[k];
        _activeMutex.readerUnlock();
        return ret;
    }

    static void RemoveAllTrackable() {
        _activeMutex.writerLock();
        std::for_each(_active.begin(), _active.end(),
                      [](std::pair<Key, Value> element) {
                          delete element.second;
                      });
        _activeMutex.writerUnlock();
    }

  private:
    int _id;
    std::atomic_uint _users;

    static std::atomic_int _total;
    static ReaderWriterLock _activeMutex;
    static std::unordered_map<Key, Value> _active;
};

template <class Key, class Value>
std::atomic_int Trackable<Key, Value>::_total(0);

template <class Key, class Value>
ReaderWriterLock Trackable<Key, Value>::_activeMutex;

template <class Key, class Value>
std::unordered_map<Key, Value> Trackable<Key, Value>::_active;

#endif /* TRACKABLE_H */
