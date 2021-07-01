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

#include "OutputFile.h"

//PriorityThreadPool<std::function<void()>>* OutputFileInner::_transferPool;
//ThreadPool<std::function<void()>>* OutputFileInner::_decompressionPool;
PriorityThreadPool<std::function<void()>>* OutputFile::_transferPool;
ThreadPool<std::function<void()>>* OutputFile::_decompressionPool;

OutputFile::OutputFile(std::string fileName, std::string metaName, int fd) : TazerFile(TazerFile::Type::Output, fileName, metaName, fd) {
    //std::cout << "entered OutputFile constructor" << std::endl;
    _outputFiles = new std::unordered_map<int, TazerFile*>;
    _threadFileDescriptors = new std::unordered_map<std::thread::id, int>;

    addFileDescriptor(fd);
}

OutputFile::~OutputFile() {
    //std::cout << "entered OutputFile destructor" << std::endl;
    std::unordered_map<int, TazerFile*>::iterator itor;
    for(itor = _outputFiles->begin(); itor != _outputFiles->end(); itor++)
        delete itor->second;
    delete _outputFiles;

    delete _threadFileDescriptors;
}

void OutputFile::open() {
    (*_outputFiles)[getThreadFileDescriptor()]->open();
}

void OutputFile::close() {
    (*_outputFiles)[getThreadFileDescriptor()]->close();
}

ssize_t OutputFile::read(void *buf, size_t count, uint32_t index) {
    return (*_outputFiles)[getThreadFileDescriptor()]->read(buf, count, index);
}

ssize_t OutputFile::write(const void *buf, size_t count, uint32_t index) {
    return (*_outputFiles)[getThreadFileDescriptor()]->write(buf, count, index);
}

off_t OutputFile::seek(off_t offset, int whence, uint32_t index) {
    return (*_outputFiles)[getThreadFileDescriptor()]->seek(offset, whence, index);
}

uint64_t OutputFile::fileSize() {
    return (*_outputFiles)[getThreadFileDescriptor()]->fileSize();
}

bool OutputFile::active() {
    int fd = getThreadFileDescriptor();
    if (fd != -1)
        return (*_outputFiles)[getThreadFileDescriptor()]->active();
    else
        return false;
}

uint32_t OutputFile::newFilePosIndex() {
    return (*_outputFiles)[getThreadFileDescriptor()]->newFilePosIndex();
}

uint64_t OutputFile::filePos(uint32_t index) {
    return (*_outputFiles)[getThreadFileDescriptor()]->filePos(index);
}

void OutputFile::setFilePos(uint32_t index, uint64_t pos) {
    return (*_outputFiles)[getThreadFileDescriptor()]->setFilePos(index, pos);
}

void OutputFile::setThreadFileDescriptor(int fd) {
    std::thread::id threadId = std::this_thread::get_id();

    //std::cout << "Set fd=" << fd << " for thread" << std::endl; 
    _threadFdLock.writerLock();
    (*_threadFileDescriptors)[threadId] = fd;
    _threadFdLock.writerUnlock();
}

int OutputFile::getThreadFileDescriptor() {
    std::thread::id threadId = std::this_thread::get_id();
    _threadFdLock.readerLock();
    bool threadNotFound = (_threadFileDescriptors->find(threadId) == _threadFileDescriptors->end());
    _threadFdLock.readerUnlock();

    if (threadNotFound) {
        //std::cout << "thread not found?" << std::endl; 
        return -1;
    }
    else {
        return (*_threadFileDescriptors)[threadId];
    }
}

void OutputFile::addFileDescriptor(int fd) {
    _outputFileLock.readerLock();
    bool fdExists = (_outputFiles->find(fd) != _outputFiles->end());
    _outputFileLock.readerUnlock();

    if (fdExists)
        removeFileDescriptor(fd);
    //std::cout << "ADDING fd=" << fd << std::endl; 
    _outputFileLock.writerLock();
    (*_outputFiles)[fd] = new OutputFileInner(name(), metaName(), fd);
    _outputFileLock.writerUnlock();
}

void OutputFile::removeFileDescriptor(int fd) {
    //std::cout << "REMOVING fd=" << fd << std::endl; 
    _outputFileLock.writerLock();
    _outputFiles->erase(fd);
    _outputFileLock.writerUnlock();
}