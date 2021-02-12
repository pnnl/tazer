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

#include "FileLinkReaderWriterLock.h"
#include "AtomicHelper.h"
#include "Config.h"
#include "Loggable.h"
#include <cstring>
#include <fcntl.h>
#include <experimental/filesystem>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>



FileLinkReaderWriterLock::FileLinkReaderWriterLock(uint32_t numEntries, std::string lockPath, std::string id) : _numEntries(numEntries),
                                                                                                                _lockPath(lockPath),
                                                                                                                _id(id)
                                                                                                                {

    

    std::error_code err;
    std::experimental::filesystem::create_directories(_lockPath, err);
    auto temp_open=((unixopen_t)dlsym(RTLD_NEXT, "open"));
    auto temp_close=((unixclose_t)dlsym(RTLD_NEXT, "close"));
    for (int entry = 0; entry<_numEntries; entry++){
        std::string entryPath = _lockPath+"/"+std::to_string(entry);
        auto fd = (*temp_open)(entryPath.c_str(), O_CREAT | O_RDWR | O_EXCL);
        (*temp_close)(fd);
    }
    char buf[256];
    gethostname(buf,256);
    _id = std::string(buf)+"."+std::to_string(::getpid());
    _writers = new std::atomic<uint16_t>[_numEntries];
    init_atomic_array(_writers,_numEntries,(uint16_t)0);

}

//todo better delete aka make sure all readers/writers are done...
FileLinkReaderWriterLock::~FileLinkReaderWriterLock() {
    delete[] _writers;
}


int FileLinkReaderWriterLock::getLock(std::string entryPath, std::string linkPath, Request* req, bool debug){
    int cnt=0;
    while (link(entryPath.c_str(), linkPath.c_str())){
        std::this_thread::sleep_for(std::chrono::microseconds(getRandTime()));
        
        if (cnt % 1000000 == 0 ){
            cnt = 0;
            req->trace(debug) << "[TAZER] R LOCK LINK ERROR!!! "<<linkPath<<" "<<__LINE__<< " " << strerror(errno) << std::endl;
        }
        cnt++;
    }
    return 0;
}

int FileLinkReaderWriterLock::getRandTime(){
    _randLock.writerLock();
    int t = rand()%1000 + 100;
    _randLock.writerUnlock();
    return t;
}

std::string FileLinkReaderWriterLock::createLinkPath(uint64_t entry){
    std::string linkPath = _lockPath+"/"+std::to_string(entry)+".locked";
    return linkPath;
}

void FileLinkReaderWriterLock::readerLock(uint64_t entry, Request* req, bool debug) {
    writerLock(entry,req,debug);
}

void FileLinkReaderWriterLock::readerUnlock(uint64_t entry, Request* req,bool debug) {
    writerUnlock(entry,req,debug);
}

void FileLinkReaderWriterLock::writerLock(uint64_t entry, Request* req, bool debug) {
    std::string entryPath = _lockPath+"/"+std::to_string(entry);
    std::string linkPath = createLinkPath(entry);

    req->trace(debug)<<_id<<" trying to wlock: "<<entry<<" "<<linkPath<<" w: "<<_writers[entry].load()<<std::endl;
     uint16_t check = 1;
    while (_writers[entry].exchange(check) == 1) {
        std::this_thread::sleep_for(std::chrono::microseconds(getRandTime()));
    }
    while (getLock(entryPath, linkPath, req, debug) != 0){
        std::string linkPath = createLinkPath(entry);
        std::this_thread::sleep_for(std::chrono::microseconds(getRandTime()));
    }
    req->trace(debug)<<_id<<" leaving wlock: "<<entry<<" "<<linkPath<<" w: "<<_writers[entry].load()<<std::endl;
}

void FileLinkReaderWriterLock::writerUnlock(uint64_t entry, Request* req,bool debug) {
    std::string linkPath = createLinkPath(entry);
    req->trace(debug)<<_id<<" trying to wunlock: "<<entry<<" "<<linkPath<<" w: "<<_writers[entry].load()<<std::endl;
    int cnt=0;
    while(unlink(linkPath.c_str())){
        std::this_thread::sleep_for(std::chrono::microseconds(getRandTime()));
         
        if (cnt%1000000 == 0 ){
            cnt = 0;
           req->trace(debug)<< "[TAZER] W LOCK UNLINK ERROR!!! "<<linkPath<<" "<<__LINE__<< " " << strerror(errno) << std::endl;
        }
        cnt++;
    }  
    _writers[entry].store(0);
    req->trace(debug)<<_id<<" leaving wunlock: "<<entry<<" "<<linkPath<<" w: "<<_writers[entry].load()<<std::endl;
}
