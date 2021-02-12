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

#include <atomic>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "InputFile.h"
#include "TazerFile.h"
#include "OutputFile.h"
#include "LocalFile.h"
#include "MetaFileParser.h"
#include "Timer.h"
#include "UnixIO.h"

//#define TIMEON(...) __VA_ARGS__
#define TIMEON(...)

extern int removeStr(char *s, const char *r);
TazerFile::TazerFile(TazerFile::Type type, std::string name, std::string metaName, int fd) : 
    Loggable(Config::TazerFileLog, "TazerFile"),
    _type(type),
    _name(name),
    _metaName(metaName),
    _filePos(0),
    _eof(false),
    _compress(false),
    _prefetch(false),
    _blkSize(1),
    _initMetaTime(0),
    _active(false),
    _fd(fd)
     {
    readMetaInfo();
    newFilePosIndex();
}

TazerFile::~TazerFile() {
}

// std::string TazerFile::findMetaParam(std::string param, std::string server, bool required){
//     size_t start = server.find(param);
//     if (start == std::string::npos){
//         if (required){
//             log(this) << "improperly formatted meta file, unable to find: "<< param << std::endl;
//             exit(0);
//         }
//         else{
//             return std::string("");
//         }
//     }
//     size_t end = server.find("\n",start);
//     start = start+param.size(); //only return the data part
//     size_t len = end;
//     if (end != std::string::npos){
//         len -= start;
//     }
//     std::string val = server.substr(start, len);
//     log(this) << param << val << std::endl;
//     return val;
// }




bool TazerFile::readMetaInfo() {
    TIMEON(uint64_t t1 = Timer::getCurrentTime());
    auto start = Timer::getCurrentTime();


    if (_fd < 0) {
        log(this) << "ERROR: Failed to open local metafile " << _metaName.c_str() << " : " << strerror(errno) << std::endl;
        return 0;
    }
    MetaFileParser parse(_fd);
    
    uint32_t numServers = 0;
    Entry e;
    while (parse.getNext(&e)){
        _name = e.file;
        _compress = e.compress;
        _prefetch = e.prefetch;
        _blkSize = e.blkSize;
        //-------------------------------------------------------------------
        if (_type != TazerFile::Local) {
            Connection *connection = Connection::addNewClientConnection(e.host, e.port);
            // std::cout << hostAddr << " " << port << " " << connection << std::endl;
            if (connection) {
                if (ConnectionPool::useCnt->count(connection->addrport()) == 0) {
                    ConnectionPool::useCnt->emplace(connection->addrport(), 0);
                    ConnectionPool::consecCnt->emplace(connection->addrport(), 0);
                }
                _connections.push_back(connection);
                numServers++;
            }
        };
    }
    TIMEON(fprintf(stderr, "Meta Time: %lu\n", Timer::getCurrentTime() - t1));
    _initMetaTime = Timer::getCurrentTime()-start;
    return (numServers > 0);
}


TazerFile::Type TazerFile::type() {
    return _type;
}

std::string TazerFile::name() {
    return _name;
}

std::string TazerFile::metaName() {
    return _metaName;
}

uint64_t TazerFile::blkSize() {
    return _blkSize;
}

bool TazerFile::compress() {
    return _compress;
}

bool TazerFile::prefetch() {
    return _prefetch;
}

bool TazerFile::active() {
    return _active.load();
}

bool TazerFile::eof(uint32_t index) {
    return _eof[index];
}

uint32_t TazerFile::newFilePosIndex() {
    uint32_t ret;
    std::unique_lock<std::mutex> lock(_fpMutex);
    ret = _filePos.size();
    _filePos.push_back(0);
    _eof.push_back(false);
    lock.unlock();
    return ret;
}

uint64_t TazerFile::filePos(uint32_t index) {
    //no lock here... we are assuming the fp index is inbounds...
    return _filePos[index];
}

void TazerFile::setFilePos(uint32_t index, uint64_t pos) {
    //no lock here... we are assuming the fp index is inbounds...
    _filePos[index] = pos;
}

//fileName is the metafile
TazerFile *TazerFile::addNewTazerFile(TazerFile::Type type, std::string fileName, std::string metaName, int fd, bool open) {
    if (type == TazerFile::Input) {
        return Trackable<std::string, TazerFile *>::AddTrackable(
            metaName, [=]() -> TazerFile * {
                // std::cout << "new input " <<fileName<<" "<<metaName<<" "<<std::endl;
                TazerFile *temp = new InputFile(fileName, metaName, fd, open);
                if (open && temp && temp->active() == 0) {
                    delete temp;
                    return NULL;
                }
                return temp;
            });
    }
    else if (type == TazerFile::Output) {
        return Trackable<std::string, TazerFile *>::AddTrackable(
            metaName, [=]() -> TazerFile * {
                // std::cout << "new output " <<fileName<<" "<<metaName<<" "<<std::endl;
                TazerFile *temp = new OutputFile(fileName, metaName, fd);
                if (temp && temp->active() == 0) {
                    delete temp;
                    return NULL;
                }
                return temp;
            });
    }
    else if (type == TazerFile::Local) {
        return Trackable<std::string, TazerFile *>::AddTrackable(
            fileName, [=]() -> TazerFile * {
                TazerFile *temp = new LocalFile(fileName, metaName, fd, open);
                if (open && temp && temp->active() == 0) {
                    delete temp;
                    return NULL;
                }
                return temp;
            });
    }
    return NULL;
}

//fileName is the metaFile
bool TazerFile::removeTazerFile(std::string fileName) {
    if (strstr(fileName.c_str(), ".tmp") != NULL) {
        char temp[1000];
        strcpy(temp, fileName.c_str());
        removeStr(temp, ".tmp");
        // std::cout<<" remove tazer 1: "<<fileName<<std::endl;
        return Trackable<std::string, TazerFile *>::RemoveTrackable(temp);
    }
    else {
        // std::cout<<" remove tazer 2: "<<fileName<<std::endl;
        return Trackable<std::string, TazerFile *>::RemoveTrackable(fileName);
    }
}

bool TazerFile::removeTazerFile(TazerFile *file) {
    // std::cout<<" remove: "<<file->_name<<" "<<file->_metaName<<std::endl;
    return removeTazerFile(file->_metaName);
}

TazerFile *TazerFile::lookUpTazerFile(std::string fileName) {
    if (strstr(fileName.c_str(), ".tmp") != NULL) {
        char temp[1000];
        strcpy(temp, fileName.c_str());
        removeStr(temp, ".tmp");
        return Trackable<std::string, TazerFile *>::LookupTrackable(temp);
    }
    else {
        return Trackable<std::string, TazerFile *>::LookupTrackable(fileName);
    }
}
