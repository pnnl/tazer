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

#include "FSReaderWriterLock.h"
#include <chrono>
#include <dirent.h>
#include <errno.h>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)

FSMutex::FSMutex(std::string lockPath) : _lockPath(lockPath) {
  mkdir(_lockPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  std::cout << "trying to create: " << _lockPath << std::endl;
}
FSMutex::~FSMutex() {
}
void FSMutex::lock() {
  std::string mutexPath = _lockPath + "/lck";
  // std::cout << "trying to get: " << mutexPath << std::endl;
  int ret = mkdir(mutexPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  while (ret != 0) {
    // std::cout << mutexPath << " " << ret << " " << strerror(errno) << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ret = mkdir(mutexPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  }
  // std::cout << "got: " << mutexPath << std::endl;
}
void FSMutex::lock(std::string path) {
  std::string mutexPath = _lockPath + "/" + path + "/lck";
  int ret = mkdir(mutexPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  while (ret != 0) {
    // std::cout << mutexPath << " " << ret << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ret = mkdir(mutexPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  }
}
void FSMutex::unlock() {
  std::string mutexPath = _lockPath + "/lck";
  remove(mutexPath.c_str());
}
void FSMutex::unlock(std::string path) {
  std::string mutexPath = _lockPath + "/" + path + "/lck";
  remove(mutexPath.c_str());
}

FSReaderWriterLock::FSReaderWriterLock(std::string lockPath) : _lockPath(lockPath) {
  mkdir(_lockPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  _fsmutex = new FSMutex(_lockPath + "/mutex");
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);
  std::stringstream ss;
  ss << hostname << "-" << std::this_thread::get_id();
  _id = ss.str();
}

FSReaderWriterLock::~FSReaderWriterLock() {
  // while (_readers) {
  //     std::this_thread::yield();
  // }
}

// bool FSReaderWriterLock::any_writers() {
//     std::string wrPath = _lockPath + "/writers";
//     bool writers = false;
//     struct stat statbuf;
//     if ((*stat)(wrPath.c_str(), &statbuf) != -1) {
//         if (S_ISDIR(statbuf.st_mode)) {
//             writers = true;
//         }
//     }
// }

bool FSReaderWriterLock::any_readers() {
  std::string rdPath = _lockPath + "/readers";
  bool readers = false;
  struct stat statbuf;
  if ((*stat)(rdPath.c_str(), &statbuf) != -1) {
    if (S_ISDIR(statbuf.st_mode)) {
      readers = true;
    }
  }
  return readers;
}

void FSReaderWriterLock::readerLock() {
  _fsmutex->lock();
  std::string rdPath = _lockPath + "/readers";
  mkdir(rdPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  rdPath += "/" + _id;
  mkdir(rdPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  _fsmutex->unlock();
}

void FSReaderWriterLock::readerUnlock() {
  _fsmutex->lock();
  std::string rdPath = _lockPath + "/readers/" + _id;
  remove(rdPath.c_str());
  rdPath = _lockPath + "/readers";
  DIR *dp = opendir(rdPath.c_str());
  struct dirent *ep;
  int cnt = 0;
  if (dp != NULL) {
    while ((ep = readdir(dp))) { //2 is for "." and ".." entries
      // std::cout << ep->d_name << std::endl;
      cnt++;
      // if (cnt > 2) {
      //     break;
      // }
    }
  }
  // std::cout << "readers: " << cnt << std::endl;
  if (cnt <= 2) {
    remove(rdPath.c_str());
  }
  _fsmutex->unlock();
}

void FSReaderWriterLock::writerLock() {
  while (true) {
    _fsmutex->lock();
    if (any_readers()) {
      _fsmutex->unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } else {
      break;
    }
  }
}

void FSReaderWriterLock::writerUnlock() {
  _fsmutex->unlock();
}

bool FSReaderWriterLock::tryWriterLock() {
  // unsigned int check = 1;
  // if (_writers.exchange(check) == 0) {
  //     while (_readers.load()) {
  //         std::this_thread::yield();
  //     }
  //     return true;
  // }
  return false;
}

bool FSReaderWriterLock::cowardlyTryWriterLock() {
  // unsigned int check = 1;
  // if (_writers.exchange(check) == 0) {
  //     if (_readers.load()) {
  //         _writers.store(0);
  //         return false;
  //     }
  //     return true;
  // }
  return false;
}

bool FSReaderWriterLock::tryReaderLock() {
  // if (!_writers.load()) {
  //     _readers.fetch_add(1);
  //     if (!_writers.load())
  //         return true;
  //     _readers.fetch_sub(1);
  //     return false;
  // }
  return false;
}
