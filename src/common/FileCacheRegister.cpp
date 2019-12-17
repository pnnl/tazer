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

#include "FileCacheRegister.h"
#include "Config.h"
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

#define REG_EMPTY 0
#define REG_INIT 1
#define REG_AVAIL 2

std::atomic_int FileCacheRegister::_registerInitLock = {0};
FileCacheRegister *FileCacheRegister::_register = NULL;

FileCacheRegister::FileCacheRegister() : _available(0) {
    for (int i = 0; i < NUMENTRIES; i++) {
        _fileName[i][0] = '\0';
    }
    
    _initFlag.fetch_add(1);
    // std::cout<<"init flag"<<_initFlag<<std::endl;
}

FileCacheRegister::~FileCacheRegister() {
}

void FileCacheRegister::incUsage() {
    while (!_initFlag.load())
        ;
    _initFlag.fetch_add(1);
}

void FileCacheRegister::unlink() {
    //shm_unlink(Config::sharedMemName.c_str());
}

void FileCacheRegister::decUsage() {
    int res = _initFlag.fetch_sub(1);
    DPRINTF("%s: %u\n", _fileName[0], _full[0].load());
    munmap(this, sizeof(FileCacheRegister));
    if (res == 1) {
        unlink();
    }
}

unsigned int FileCacheRegister::registerFile(std::string name) {
    unsigned int fileIndex = 0;
    // std::cout<<"trying to register file"<<std::endl;
    _registerLock.writerLock();
    // std::cout<<"got lock"<<std::endl;
    int temp = _available.load();
    for (int i = 0; i < temp; i++) {
        if (!name.compare(_fileName[i])) {
            DPRINTF("Found %s\n", name.c_str());
            fileIndex = i + 1;
            break;
        }
    }

    if (fileIndex == 0) {
        int index = _available.fetch_add(1);
        if (index < NUMENTRIES) {
            memcpy(_fileName[index], name.c_str(), name.length() + 1);
            fileIndex = index + 1;
            DPRINTF("New %s\n", name.c_str());
        }
    }
    _registerLock.writerUnlock();
    return fileIndex;
}

FileCacheRegister *FileCacheRegister::initFileCacheRegister(bool server) {
    FileCacheRegister *reg = NULL;
    if (Config::enableSharedMem){
        // printf("Using Register %s\n", Config::sharedMemName.c_str());
        std::string name = server ? Config::sharedMemName+"_server" : Config::sharedMemName;
        int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0644);
        if (fd == -1) {
            // printf("Reusing shared memory\n");
            std::cout << "resuing file cache register" << std::endl;
            fd = shm_open(name.c_str(), O_RDWR, 0644);
            if (fd != -1) {
                ftruncate(fd, sizeof(FileCacheRegister));
                void *ptr = mmap(NULL, sizeof(FileCacheRegister), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                reg = (FileCacheRegister *)ptr;
                reg->incUsage();
            }
            else {
                std::cerr << "[TAZER]"
                        << "Error opening shared memory " << strerror(errno) << std::endl;
            }
        }
        else {
            // printf("Created shared memory\n");
            std::cout<<"creating file cache register (size: "<<sizeof(FileCacheRegister)<<")"<<std::endl;
            ftruncate(fd, sizeof(FileCacheRegister));
            // printf("ftruncate success\n");
            void *ptr = mmap(NULL, sizeof(FileCacheRegister), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            // printf("mmap success\n");
            reg = new (ptr) FileCacheRegister();
        }
    }
    else{
        reg = new FileCacheRegister();
    }
    return reg;
}

FileCacheRegister *FileCacheRegister::openFileCacheRegister(bool server) {
    int check = REG_EMPTY;
    // std::cout<<"opening file cache reg..."<<_registerInitLock<<" "<<check<<" "<<REG_INIT<<std::endl;
    if (_registerInitLock.compare_exchange_weak(check, REG_INIT)) {
        _register = FileCacheRegister::initFileCacheRegister();
        _registerInitLock.store(REG_AVAIL);
    }
    else {
        while (_registerInitLock.load() != REG_AVAIL)
            ;
    }
    return _register;
}

unsigned int FileCacheRegister::getFileRegisterIndex(std::string name) {
    unsigned int index = 0;
    _registerLock.readerLock();
    int temp = _available.load();
    for (int i = 0; i < temp; i++) {
        if (!name.compare(_fileName[i])) {
            index = i + 1;
        }
    }
    _registerLock.readerUnlock();

    return index;
}

void FileCacheRegister::closeFileCacheRegister() {
    //Not sure if this should lock...
    if (_register) {
       // _register->decUsage();
    }
    //    shm_unlink(Config::sharedMemName.c_str());
}
