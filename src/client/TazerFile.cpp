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
#include <fnmatch.h>

#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "InputFile.h"
#include "TazerFile.h"
#include "OutputFile.h"
#include "LocalFile.h"
#include "TrackFile.h"
#include "Timer.h"
#include "UnixIO.h"

//#define TIMEON(...) __VA_ARGS__
#define TIMEON(...)
// #define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)
#define MYPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define TRACKFILECHANGES 1

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
#ifdef TRACKFILECHANGES
  char pattern[] = "*.h5";
  auto ret_val = fnmatch(pattern, name.c_str(), 0);
  char pattern_2[] = "*.fits";
  auto ret_val_2 = fnmatch(pattern_2, name.c_str(), 0);
  char pattern_3[] = "*.vcf";
  auto ret_val_3 = fnmatch(pattern_3, name.c_str(), 0);
  char pattern_4[] = "*.tar.gz";
  auto ret_val_4 = fnmatch(pattern_4, name.c_str(), 0);
  char pattern_5[] = "*.txt";
  auto ret_val_5 = fnmatch(pattern_5, name.c_str(), 0);
  char pattern_6[] = "*.lht";
  auto ret_val_6 = fnmatch(pattern_6, name.c_str(), 0);
  char pattern_7[] = "*.fna";
  auto ret_val_7 = fnmatch(pattern_7, name.c_str(), 0);
  char pattern_8[] = "*.*.bt2";
  auto ret_val_8 = fnmatch(pattern_8, name.c_str(), 0);
  char pattern_9[] = "*.fastq";
  auto ret_val_9 = fnmatch(pattern_9, name.c_str(), 0);
  //  std::string hdf_file_name(name);
    // auto found = hdf_file_name.find("residue");
    //if (hdf_file_name.find("residue") == std::string::npos) {
  if (ret_val !=0 && ret_val_2 != 0 && ret_val_3 != 0 
      && ret_val_4 != 0 && ret_val_5 != 0 && ret_val_6 !=0
      && ret_val_7 !=0 && ret_val_8 !=0 && ret_val_9 !=0) {
#endif
    readMetaInfo();
#ifdef TRACKFILECHANGES
    }
#endif
    newFilePosIndex();
}

TazerFile::~TazerFile() {
}

//meta file format:
//TAZER0.1
//type=         (type can be input, output, or local)
//[server]      (the info under [server] can be in any order, but the host, port, and file parts must be provided, multiple servers can be given by using multiple [server] tags)
//host=
//port=
//file=
bool TazerFile::readMetaInfo() {
    MYPRINTF("Trying to read meta info for file %s\n", _name.c_str());
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
        std::cout << "ERROR: Failed to read local metafile: " << strerror(errno) << std::endl;
        raise(SIGSEGV);
        return 0;
    }
    meta[fileSize] = '\0';

    typedef enum {
        DEFAULT,
        SERVER,
        //OPTIONS,
        DONE
    } State;

    uint32_t numServers = 0;
    std::string curLine;
    std::string hostAddr;
    std::string fileName;
    int port;
    std::stringstream ss(meta);

    State state = DEFAULT;
    std::getline(ss, curLine);
    while(state != DONE) {
        switch (state)
        {
        case SERVER:
            //found [server]
            hostAddr = "\0";
            port = 0;
            fileName = "\0";
	    
            while(std::getline(ss, curLine)) {
                if(curLine.compare(0, 5, "host=") == 0) {
                    hostAddr = curLine.substr(5, (curLine.length() - 5));
                    log(this) << "hostaddr: " << hostAddr << std::endl;
                }
                else if(curLine.compare(0, 5, "port=") == 0) {
                    port = atoi(curLine.substr(5, (curLine.length() - 5)).c_str());
                    log(this) << "port: " << port << std::endl;
                }
                else if(curLine.compare(0, 5, "file=") == 0) {
                    fileName = curLine.substr(5, (curLine.length() - 5));
                    _name = fileName;
                    log(this) << "fileName: " << fileName << std::endl;
                }
                else if(curLine.compare(0, 13, "compress=true") == 0) {
                    _compress = true;
                    log(this) << "compress: " << _compress << std::endl;
                }
                else if(curLine.compare(0, 14, "compress=false") == 0) {
                    _compress = false;
                    log(this) << "compress: " << _compress << std::endl;
                }
                else if(curLine.compare(0, 13, "prefetch=true") == 0) {
                    _prefetch = true;
                    log(this) << "prefetch: " << _prefetch << std::endl;
                }
                else if(curLine.compare(0, 14, "prefetch=false") == 0) {
                    _prefetch = false;
                    log(this) << "prefetch: " << _prefetch << std::endl;
                }
                else if(curLine.compare(0, 11, "block_size=") == 0) {
                    _blkSize = atoi(curLine.substr(11, (curLine.length() - 11)).c_str());
                    log(this) << "blkSize: " << _blkSize << std::endl;
                }
                else {
                    break;
                }
            }
            //make sure the host port and file name were given
            if(hostAddr == "\0" || port == 0 || fileName == "\0") {
                log(this) << "0:improperly formatted meta file" << std::endl;
		std::cout << "0:improperly formatted meta file" << std::endl;
                return 0;
            }
            //after collecting info for a server
            if (_type != TazerFile::Local) {
                Connection *connection = Connection::addNewClientConnection(hostAddr, port);
                std::cout << hostAddr << " " << port << " " << connection << std::endl;
                if (connection) {
                    if (ConnectionPool::useCnt->count(connection->addrport()) == 0) {
                        ConnectionPool::useCnt->emplace(connection->addrport(), 0);
                        ConnectionPool::consecCnt->emplace(connection->addrport(), 0);
                    }
                    _connections.push_back(connection);
                    numServers++;
                }
            }
	    
            state = DEFAULT;
            break;
        default:
            if(curLine.compare(0, 8, "[server]") == 0) {
                state = SERVER;
            }
            else if(!std::getline(ss, curLine)) {
                state = DONE;
            }
            break;
        }
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
                std::cout << "new input " <<fileName<<" "<<metaName<<" "<<std::endl;
                TazerFile *temp = new InputFile(fileName, metaName, fd, open);
                if (open && temp && temp->active() == 0) {
                    delete temp;
                    return NULL;
                }
                return temp;
            });
    }
    else if (type == TazerFile::Output) {
        bool dontcare;
        return Trackable<std::string, TazerFile *>::AddTrackable(
            metaName, [=]() -> TazerFile * {
                std::cout << "new output " <<fileName<<" "<<metaName<<" "<<std::endl;
                std::cout << "Create new" << std::endl;
                TazerFile *temp = new OutputFile(fileName, metaName, fd);
                if (temp) {
                    OutputFile* out = dynamic_cast<OutputFile*>(temp);
                    out->setThreadFileDescriptor(fd);
                    if (temp->active() == 0) {
                    delete temp;
                    return NULL;
                    }
                }
                return temp;
            },
            [=](TazerFile* tazerFile) -> void {
                OutputFile* out = dynamic_cast<OutputFile*>(tazerFile);
                out->setThreadFileDescriptor(fd);
                //std::cout << "Reuse old" << std::endl;
                out->addFileDescriptor(fd);
            }, dontcare);
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
    } else if (type == TazerFile::TrackLocal) {
      DPRINTF("Trackfile going to be added to the Trackable \n");
        return Trackable<std::string, TazerFile *>::AddTrackable(
            fileName, [=]() -> TazerFile * {
	      DPRINTF("Filename in lambda %s\n", fileName.c_str());
                TazerFile *temp = new TrackFile(fileName, fd, open);
                if (open && temp && temp->active() == 0) {
                    delete temp;
		    DPRINTF("Can't add a TrackFile with Filename %s fd %d", 
			    fileName.c_str(), fd);
		    return NULL;
                }
		DPRINTF("Adding (filename,Trackfile) to map\n");
                return temp;
            });
    }  
    return NULL;
}

//fileName is the metaFile
bool TazerFile::removeTazerFile(std::string fileName) {
  DPRINTF("Removing Tazerfile %s\n", fileName.c_str());  
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
  DPRINTF("Removing Tazerfile %s\n", file->_metaName.c_str());
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
