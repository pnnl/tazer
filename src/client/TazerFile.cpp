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
    _save_local(false),
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

//meta.in format:
//server-ip-address:server-port:compression:prefetch:localfile:blk_size:file to read(location on the server)|
//notes:
//"prefetch" enables prefetching for this file
//"localfile" is currently unused right now (should be set to 0)
//the entry for a server must end in "|"
//multiple servers can be provided (separated by "|")
bool TazerFile::readMetaInfo() {
    TIMEON(uint64_t t1 = Timer::getCurrentTime());
    auto start = Timer::getCurrentTime();
    unixread_t unixRead = (unixread_t)dlsym(RTLD_NEXT, "read");
    unixlseek_t unixlseek = (unixlseek_t)dlsym(RTLD_NEXT, "lseek");

    if (_fd < 0) {
        log(this) << "ERROR: Failed to open local metafile " << _metaName.c_str() << " : " << strerror(errno) << std::endl;
        return 0;
    }

    int64_t fileSize = (*unixlseek)(_fd, 0L, SEEK_END);
    (*unixlseek)(_fd, 0L, SEEK_SET);
    char *meta = new char[fileSize + 1];
    int ret = (*unixRead)(_fd, (void *)meta, fileSize);
    if (ret < 0) {
        log(this) << "ERROR: Failed to read local metafile: " << strerror(errno) << std::endl;
        raise(SIGSEGV);
        return 0;
    }
    meta[fileSize] = '\0';
    std::string metaStr(meta);
    uint32_t numServers = 0;
    size_t cur = 0;
    size_t l = metaStr.find("|");
    while (l != std::string::npos) {
        std::string line = metaStr.substr(cur, l - cur);
        log(this) << cur << " " << line << std::endl;

        uint32_t lcur = 0;
        uint32_t next = line.find(":", lcur);
        if (next == std::string::npos) {
            log(this) << "0:improperly formatted meta file" << std::endl;
            break;
        }
        std::string hostAddr = line.substr(lcur, next - lcur);
        log(this) << "hostaddr: " << hostAddr << std::endl;

        lcur = next + 1;
        next = line.find(":", lcur);
        if (next == std::string::npos) {
            log(this) << "1:improperly formatted meta file" << std::endl;
            break;
        }
        int port = atoi(line.substr(lcur, next - lcur).c_str());
        log(this) << "port: " << port << std::endl;

        lcur = next + 1;
        next = line.find(":", lcur);
        if (next == std::string::npos) {
            log(this) << "2:improperly formatted meta file" << std::endl;
            break;
        }
        _compress = atoi(line.substr(lcur, next - lcur).c_str());
        log(this) << "compress: " << _compress << std::endl;

        lcur = next + 1;
        next = line.find(":", lcur);
        if (next == std::string::npos) {
            log(this) << "3:improperly formatted meta file" << std::endl;
            break;
        }
        _prefetch = atoi(line.substr(lcur, next - lcur).c_str());
        log(this) << "prefetch: " << _prefetch << std::endl;

        lcur = next + 1;
        next = line.find(":", lcur);
        if (next == std::string::npos) {
            log(this) << "4:improperly formatted meta file" << std::endl;
            break;
        }
        _save_local = atoi(line.substr(lcur, next - lcur).c_str());
        log(this) << "save_local: " << _save_local << std::endl;

        lcur = next + 1;
        next = line.find(":", lcur);
        if (next == std::string::npos) {
            log(this) << "5:improperly formatted meta file" << std::endl;
            break;
        }
        _blkSize = atoi(line.substr(lcur, next - lcur).c_str());
        log(this) << "blkSize: " << _blkSize << std::endl;

        lcur = next + 1;
        next = line.size();
        std::string fileName = line.substr(lcur, next - lcur);
        if(fileName.length()){
            _name = fileName;
            log(this) << "fileName: " << fileName << std::endl;
        }

        if (_type != TazerFile::Local) {
            Connection *connection = Connection::addNewClientConnection(hostAddr, port);
            log(this) << hostAddr << " " << port << " " << connection << std::endl;
            if (connection) {
                if (ConnectionPool::useCnt->count(connection->addrport()) == 0) {
                    ConnectionPool::useCnt->emplace(connection->addrport(), 0);
                    ConnectionPool::consecCnt->emplace(connection->addrport(), 0);
                }
                _connections.push_back(connection);
                numServers++;
            }
        }

        cur = l + 1;
        l = metaStr.find("|", cur);
    }

    delete[] meta;
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

bool TazerFile::eof() {
    return _eof;
}

uint32_t TazerFile::newFilePosIndex() {
    uint32_t ret;
    std::unique_lock<std::mutex> lock(_fpMutex);
    ret = _filePos.size();
    _filePos.push_back(0);
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
            fileName, [=]() -> TazerFile * {
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
            fileName, [=]() -> TazerFile * {
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
        return Trackable<std::string, TazerFile *>::RemoveTrackable(temp);
    }
    else {
        return Trackable<std::string, TazerFile *>::RemoveTrackable(fileName);
    }
}

bool TazerFile::removeTazerFile(TazerFile *file) {
    return removeTazerFile(file->_name);
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
