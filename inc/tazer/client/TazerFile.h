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

#ifndef TazerFile_H_
#define TazerFile_H_

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <list>
#include <math.h>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Connection.h"
#include "Loggable.h"
#include "Trackable.h"

class TazerFile : public Loggable, public Trackable<std::string, TazerFile *> {
  public:
    enum Type { Input = 0,
                Output = 1,
                Local = 2 };

    TazerFile(TazerFile::Type type, std::string name, std::string metaName, int fd);
    virtual ~TazerFile();

    virtual void open() = 0;
    virtual void close() = 0;
    virtual uint64_t fileSize() = 0;
  //  virtual uint64_t numBlks() = 0;
  
    virtual ssize_t read(void *buf, size_t count, uint32_t filePosIndex = 0) = 0;
    virtual ssize_t write(const void *buf, size_t count, uint32_t filePosIndex = 0) = 0;

    virtual uint32_t newFilePosIndex();
    virtual uint64_t filePos(uint32_t index);
    virtual void setFilePos(uint32_t index, uint64_t pos);
    virtual off_t seek(off_t offset, int whence, uint32_t index = 0) = 0;

    static TazerFile *addNewTazerFile(TazerFile::Type type, std::string fileName, std::string metaName, int fd, bool open = true);
    static bool removeTazerFile(std::string fileName);
    static bool removeTazerFile(TazerFile *file);
    static TazerFile *lookUpTazerFile(std::string fileName);

    TazerFile::Type type();
    std::string name();
    std::string metaName();
    uint64_t blkSize();
    bool compress();
    bool prefetch();
    virtual bool active();
    bool eof(uint32_t index);
  protected:
    std::string findMetaParam(std::string param, std::string server, bool required);
    bool readMetaInfo();
    

    TazerFile::Type _type;
    std::string _name;
    std::string _metaName;

    std::mutex _fpMutex;
    std::vector<uint64_t> _filePos;
    std::vector<bool> _eof;

    //Properties from meta data file
    bool _compress;
    uint32_t _prefetch; //Adding the option to prefetch or not a particular file
    uint64_t _blkSize;

    uint64_t _initMetaTime;

    std::atomic_bool _active; //if the connections are up
    std::vector<Connection *> _connections;

  private:
    int _fd;
};

#endif /* TazerFile_H_ */
