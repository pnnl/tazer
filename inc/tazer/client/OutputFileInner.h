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

#ifndef OutputFileInner_H_
#define OutputFileInner_H_
#include <atomic>
#include <mutex>
#include <string>
#include <map>

#include "TazerFile.h"
#include "PriorityThreadPool.h"
#include "ThreadPool.h"
#include "OutputFile.h"

extern std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_w_stat;
class OutputFileInner : public TazerFile {
  public:
    OutputFileInner(std::string fileName, std::string metaName, int fd);
    ~OutputFileInner();

    void open();
    void close();

    ssize_t read(void *buf, size_t count, uint32_t index);
    ssize_t write(const void *buf, size_t count, uint32_t index);
    int vfprintf(unsigned int pos, int count);

    off_t seek(off_t offset, int whence, uint32_t index);
    uint64_t fileSize();

    //static PriorityThreadPool<std::function<void()>>* _transferPool;
    //static ThreadPool<std::function<void()>>* _decompressionPool;

  private:
    bool openFileOnServer();
    bool closeFileOnServer();
    uint64_t fileSizeFromServer();

    uint64_t compress(char **buffer, uint64_t offset, uint64_t size);
    void addTransferTask(char *buf, uint64_t size, uint64_t compSize, uint64_t fp, uint32_t seqNum);
    void addCompressTask(char *buf, uint64_t size, uint64_t fp, uint32_t seqNum);

    bool trackWrites(size_t count, uint32_t index, uint32_t startBlock, uint32_t endBlock);
    //    void compress(CompressionWorkArgs task);

    int _compLevel;
    uint64_t _fileSize;
    uint32_t _messageOffset; //size of header -- depends on the file name
    std::atomic_uint _seqNum;
    std::atomic_uint _sendNum;
    std::mutex _openCloseLock;

    char *_buffer;
    uint64_t _bufferIndex;
    uint64_t _bufferFp;
    uint64_t _bufferCnt;

    uint64_t _totalCnt;
    std::mutex _bufferLock;

    
};

#endif /* OutputFileInner_H_ */
