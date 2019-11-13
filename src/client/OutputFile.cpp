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

#include <chrono>
#include <deque>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "OutputFile.h"

#include "Config.h"
#include "Message.h"
#include "lz4.h"
#include "lz4hc.h"

PriorityThreadPool<std::function<void()>> OutputFile::_transferPool(1);
ThreadPool<std::function<void()>> OutputFile::_decompressionPool(Config::numClientDecompThreads);

OutputFile::OutputFile(std::string fileName, std::string metaName, int fd) : TazerFile(TazerFile::Type::Output, fileName, metaName, fd),
                                                       _compLevel(0),
                                                       _fileSize(0),
                                                       _messageOffset(sizeof(writeMsg) + _name.size() + 1),
                                                       _seqNum(0),
                                                       _sendNum(0) {
    std::unique_lock<std::mutex> lock(_bufferLock);
    _buffer = new char[Config::outputFileBufferSize];
    _bufferIndex = _messageOffset;
    _bufferFp = 0;
    _bufferCnt = 0;
    _totalCnt = 0;
    lock.unlock();
    open();
}

OutputFile::~OutputFile() {
    *this << "Destorying file" << _metaName << std::endl;
    close();
}

void OutputFile::open() {
    if (_connections.size()) {
        std::unique_lock<std::mutex> lock(_openCloseLock);
        bool prev = false;
        if (_active.compare_exchange_strong(prev, true)) {
            //Open file on all possible servers.  Connections should already exist!
            if (openFileOnServer()) {
                _transferPool.initiate();
                _decompressionPool.initiate();
                std::cerr << "[TAZER] "
                          << "output: " << _name << " opened" << std::endl;
            }
            else { //We failed to open the file kill the threads we started...
                _active.store(false);
            }
        }
        lock.unlock();
    }
    else {
        std::cerr << "[TAZER] "
                  << "ERROR: " << _name << " has no connections!" << std::endl;
    }
}

void OutputFile::close() {
    *this << "Closing file " << _name << std::endl;
    //std::cout<<"[TAZER] " << "closing bi: " << _bufferIndex << " bc: " << _bufferCnt << " bfp: " << _bufferFp << " " << _totalCnt << std::endl;
    if (_bufferCnt > 0) {
        uint32_t seqNum = _seqNum.fetch_add(1);
        //char * buffer = new char[Config::outputFileBufferSize];
        if (_compress)
            addCompressTask(_buffer, _bufferCnt, _bufferFp, seqNum);
        else
            addTransferTask(_buffer, _bufferCnt, _bufferCnt, _bufferFp, seqNum);
        //std::cout<<"[TAZER] " << "final add tx task" << std::endl;
    }
    std::unique_lock<std::mutex> lock(_openCloseLock);
    bool prev = true;
    if (_active.compare_exchange_strong(prev, false)) {

        while (_seqNum.load() != _sendNum.load()) {
            std::this_thread::yield();
        }
        //std::cout<<"[TAZER] " << "closing bi: " << _bufferIndex << " bc: " << _bufferCnt << " bfp: " << _bufferFp << " " << _totalCnt << std::endl;

        //Kill threads
        _transferPool.terminate();
        _decompressionPool.terminate();

        //Close file
        closeFileOnServer();

        //Reset values
        _seqNum.store(0);
        _sendNum.store(0);
    }
    lock.unlock();
}

bool OutputFile::openFileOnServer() {
    _connections[0]->lock();
    for (uint32_t i = 0; i < Config::socketRetry; i++) {
        if (sendOpenFileMsg(_connections[0], _name, 0, _compress, true)) {
            if (recFileSizeMsg(_connections[0], _fileSize))
                break;
        }
    }
    _connections[0]->unlock();
    return true;
}

bool OutputFile::closeFileOnServer() {
    _connections[0]->lock();
    bool ret = sendCloseFileMsg(_connections[0], _name);
    recAckMsg(_connections[0], CLOSE_FILE_MSG);
    _connections[0]->unlock();
    return ret;
}

ssize_t OutputFile::read(void *buf, size_t count, uint32_t index) {
    *this << "in outputfile read.... need to implement... exiting" << std::endl;
    exit(-1);
    return 0;
}

// void OutputFile::addCompressTask(char *buf, uint32_t size, uint64_t fp, uint32_t seqNum) {
//void OutputFile::addTransferTask(char *buf, uint32_t size, uint32_t compSize, uint64_t fp, uint32_t seqNum) {
ssize_t OutputFile::write(const void *buf, size_t count, uint32_t index) {
    if (_active.load()) {
        trackWrites(count, index, 0, 0);

        uint64_t fp = _filePos[index];
        // std::cout << "[TAZER] "
        //           << "write: i: " << fp << " cnt: " << count << " num tasks: " << _transferPool.numTasks() << " index: " << index << std::endl;
        _filePos[index] += count;
        if (_filePos[index] > _fileSize)
            _fileSize = _filePos[index] + 1;

        uint64_t bytesToSend = count;
        uint64_t bytesSent = 0;
        uint64_t spaceAvail = Config::outputFileBufferSize - _bufferIndex;
        while (bytesToSend > spaceAvail || _bufferCnt + _bufferFp != fp + bytesSent) {
            // std::cout << "[TAZER] " << (bytesToSend > spaceAvail) << " " << (_bufferCnt + _bufferFp != fp + bytesSent) << std::endl;
            // std::cout << "[TAZER] " << _bufferIndex << " " << bytesSent << " " << spaceAvail << " " << bytesToSend << " " << _bufferCnt << " " << _bufferFp << " " << _bufferCnt + _bufferFp << " " << fp + bytesSent << std::endl;
            if (bytesToSend < spaceAvail) {
                spaceAvail = bytesToSend;
            }
            memcpy(_buffer + _bufferIndex, buf + bytesSent, spaceAvail);
            //_totalCnt += spaceAvail;
            bytesSent += spaceAvail;
            bytesToSend -= spaceAvail;
            _bufferCnt += spaceAvail;
            //if (_bufferCnt > 0) {
            uint32_t seqNum = _seqNum.fetch_add(1);
            char *buffer = new char[Config::outputFileBufferSize];
            if (_compress)
                addCompressTask(_buffer, _bufferCnt, _bufferFp, seqNum);
            else
                addTransferTask(_buffer, _bufferCnt, _bufferCnt, _bufferFp, seqNum);
            _buffer = buffer;
            //}
            //std::cout<<"[TAZER] " << "add tx task" << std::endl;
            _bufferIndex = _messageOffset;
            //_bufferFp = fp;
            _bufferFp += _bufferCnt;
            _bufferCnt = 0;
            spaceAvail = Config::outputFileBufferSize - _bufferIndex;
        }

        memcpy(_buffer + _bufferIndex, buf + bytesSent, bytesToSend);
        //_totalCnt += bytesToSend;
        //memcpy(_buffer + _bufferIndex, buf, count);
        _bufferIndex += bytesToSend;
        _bufferCnt += bytesToSend;
        if (_transferPool.numTasks() == 0) {
            uint32_t seqNum = _seqNum.fetch_add(1);
            char *buffer = new char[Config::outputFileBufferSize];
            if (_compress)
                addCompressTask(_buffer, _bufferCnt, _bufferFp, seqNum);
            else
                addTransferTask(_buffer, _bufferCnt, _bufferCnt, _bufferFp, seqNum);
            //std::cout<<"[TAZER] " << "add tx task" << std::endl;
            _buffer = buffer;
            _bufferIndex = _messageOffset;
            _bufferCnt = 0;
            _bufferFp = fp + count;
        }
        // std::cout << "[TAZER] "
        //           << "bi: " << _bufferIndex << " bc: " << _bufferCnt << " bfp: " << _bufferFp << " " << _totalCnt << std::endl;

        return count;
    }
    return 0;
}

bool OutputFile::trackWrites(size_t count, uint32_t index, uint32_t startBlock, uint32_t endBlock) {
    if (Config::TrackReads) {
        unixopen_t unixopen = (unixopen_t)dlsym(RTLD_NEXT, "open");
        unixclose_t unixclose = (unixclose_t)dlsym(RTLD_NEXT, "close");
        unixwrite_t unixwrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");

        int fd = (*unixopen)("access_new.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
        if (fd != -1) {
            std::stringstream ss;
            ss << _name << " " << _filePos[index] << " " << count << " " << startBlock << " " << endBlock << std::endl;
            unixwrite(fd, ss.str().c_str(), ss.str().length());
            unixclose(fd);
            return true;
        }
    }
    return false;
}

uint64_t OutputFile::compress(char **buffer, uint64_t offset, uint64_t size) {
    uint64_t compSize = 0;
    uint64_t maxCompSize = LZ4_compressBound(size);
    char *compBuf = new char[maxCompSize + _messageOffset];
    if (_compLevel < 0) {
        compSize = LZ4_compress_fast((*buffer) + _messageOffset, compBuf + _messageOffset, size, maxCompSize, -_compLevel);
    }
    else if (_compLevel == 0) {
        compSize = LZ4_compress_default((*buffer) + _messageOffset, compBuf + _messageOffset, size, maxCompSize);
    }
    else {
        compSize = LZ4_compress_HC((*buffer) + _messageOffset, compBuf + _messageOffset, size, maxCompSize, _compLevel);
    }

    delete[] * buffer;
    *buffer = compBuf;
    return compSize;
}

void OutputFile::addTransferTask(char *buf, uint64_t size, uint64_t compSize, uint64_t fp, uint32_t seqNum) {
    _transferPool.addTask(seqNum, [this, buf, size, compSize, fp, seqNum] {
        if (seqNum == _sendNum.load()) {
            //Send the message
            _connections[0]->lock();
            if (sendWriteMsg(_connections[0], _name, buf, size, compSize, fp, seqNum)) {
                if (recAckMsg(_connections[0], WRITE_MSG)) {

                    _totalCnt += (size - _messageOffset);
                    *this << "Successful write " << _totalCnt << " " << seqNum << std::endl;
                }
            }
            else
                *this << "Failed send" << std::endl;
            _connections[0]->unlock();
            _sendNum.fetch_add(1);
        }
        else
            addTransferTask(buf, size, compSize, fp, seqNum);
    });
}

void OutputFile::addCompressTask(char *buf, uint64_t size, uint64_t fp, uint32_t seqNum) {
    _decompressionPool.addTask([this, buf, size, fp, seqNum] {
        char *temp = buf;
        uint32_t newSize = compress(&temp, fp, size);
        addTransferTask(temp, size, newSize, fp, seqNum);
    });
}

off_t OutputFile::seek(off_t offset, int whence, uint32_t index) {
    // std::cout << _name << " seek " << offset << " " << whence << " " << index << std::endl;

    /// flush buffer because we are not sending consecutive data
    uint32_t seqNum = _seqNum.fetch_add(1);
    char *buffer = new char[Config::outputFileBufferSize];
    if (_compress) {
        addCompressTask(_buffer, _bufferCnt, _bufferFp, seqNum);
    }
    else {
        addTransferTask(_buffer, _bufferCnt, _bufferCnt, _bufferFp, seqNum);
    }
    _buffer = buffer;
    _bufferIndex = _messageOffset;
    _bufferCnt = 0;

    switch (whence) {
    case SEEK_SET:
        _filePos[index] = offset;
        _bufferFp = _filePos[index];
        break;
    case SEEK_CUR:
        _filePos[index] += offset;
        _bufferFp = _filePos[index];
        break;
    case SEEK_END:
        _filePos[index] = _fileSize + offset;
        _bufferFp = _filePos[index];
        break;
    }
    return _filePos[index];
}

uint64_t OutputFile::fileSize() {
    if (_active.load())
        return _fileSize;

    uint64_t fileSize = 0;
    if (_connections.size()) {
        _connections[0]->lock();
        for (uint32_t i = 0; i < Config::socketRetry; i++) {
            *this << "Requesting file size on " << _connections[0] << " for " << _name << std::endl;
            if (sendRequestFileSizeMsg(_connections[0], _name)) {
                if (recFileSizeMsg(_connections[0], fileSize)) {
                    break;
                }
            }
            else
                std::cout << "[TAZER] "
                          << "ERROR: Failed to send size request" << std::endl;
        }
        _connections[0]->unlock();
    }
    return fileSize;
}