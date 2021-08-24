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

#include "ThreadPool.h"
#include "Timer.h"
#include "string.h"
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

unsigned int numThreads = 1;
ThreadPool<std::function<void()>> pool(numThreads);

size_t blockSize = 1024*512;
size_t numBlocks = 0;
size_t fileSize = 0;

std::mutex writeMutex;
int dFile = -1;

std::atomic_int errors(0);
std::atomic_int idCount(0);
thread_local int id = -1;
int * sFiles;
int * mFiles;
char * buff;

char *makeBuffer(unsigned int size) {
    char *buffer = new char[size + 1];
    if (buffer) {
        memset(buffer, '0', sizeof(char) * size);
        buffer[size] = '\0';
        return buffer;
    }
    return NULL;
}

size_t getFileSize(std::string fileName) {
    struct stat64 sb;
    if (stat64(fileName.c_str(), &sb) != -1) { //stat instead of lstat so we get size of the file, not the link
        std::cout << fileName << " size: " << sb.st_size << std::endl;
        return sb.st_size;
    }
    std::cout << fileName << " EMPTY FILE" << std::endl;
    return 0;
}

void readFromFile(int fd, char *buffer, int size) {
    char *local = buffer;
    while (size) {
        int bytes = read(fd, local, size);
        if (bytes >= 0) {
            local += bytes;
            size -= bytes;
        }
        else {
            std::cout << "Failed a read " << fd << " " << size << std::endl;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc == 4) {
        std::string sourceFileName(argv[1]);
        std::string sourceFileNameMeta = sourceFileName + ".meta.in";

        unsigned int numThreads = (unsigned int) atoi(argv[2]);
        sFiles = new int[numThreads];
        mFiles = new int[numThreads];

        auto fileSize = getFileSize(sourceFileName);
        auto fileSizeMeta = getFileSize(sourceFileNameMeta);
        if (fileSize && fileSize == fileSizeMeta) {
            fileSize = fileSize;

            numBlocks = fileSize / blockSize;
            if (fileSize % blockSize)
                numBlocks++;

            int seed = atoi(argv[3]);
            unsigned int * blockOrder = new unsigned int[numBlocks];
            if(seed) {
                srand(seed);
                
                for (unsigned int i = 0; i < numBlocks; i++) {
                    blockOrder[i] = rand() % numBlocks;
                }
            }
            else {
                for (unsigned int i = 0; i < numBlocks; i++) {
                    blockOrder[i] = i;
                }
            }

            pool.initiate(); //Start local threads to do write
            uint64_t time = Timer::getCurrentTime();

            for (unsigned int i = 0; i < numBlocks; i++) {
                unsigned int blk = blockOrder[i];
                pool.addTask([sourceFileName, sourceFileNameMeta, blk, fileSize] { //Push blocks to a task pool
                    if (id < 0) { //Get an id and use it to open file and create a buffer once per thread
                        id = idCount.fetch_add(1);
                        sFiles[id] = open(sourceFileName.c_str(), O_RDONLY);
                        mFiles[id] = open(sourceFileNameMeta.c_str(), O_RDONLY);
                    }

                    if (sFiles[id] != -1 && mFiles[id]) {
                        char * buff = makeBuffer(blockSize);
                        char * buffMeta = makeBuffer(blockSize);
                        if (buff && buffMeta) {
                            size_t bytesToCopy = (blk + 1 == numBlocks) ? fileSize - blk * blockSize : blockSize;
                            //Read real file
                            lseek(sFiles[id], blk * blockSize, SEEK_SET);
                            readFromFile(sFiles[id], buff, sizeof(char) * bytesToCopy);
                            //Read tazer file
                            lseek(mFiles[id], blk * blockSize, SEEK_SET);
                            readFromFile(mFiles[id], buffMeta, sizeof(char) * bytesToCopy);
                            if(strcmp(buff, buffMeta))
                                errors.fetch_add(1);
                            
                            delete buff;
                            delete buffMeta;
                        }
                        else
                            std::cout << "Failed to create buff size: " << blockSize << " blk: " << blk << std::endl;
                    }
                    else
                        std::cout <<id << "Failed to open " << sourceFileName << " blk: " << blk <<" "<< strerror(errno)<< std::endl;
                });
            }

            pool.wait();      //Wait on writing threads
            pool.terminate(); //Join writing threads

            time = Timer::getCurrentTime() - time;
            std::cout << "--------TIME: " << time << " ------------" << std::endl;
            std::cout << "--------ERROR: " << errors << " ------------" << std::endl;
            //Close files and free buffers
            close(dFile);
            for (int i = 0; i < idCount; i++) {
                close(sFiles[i]);
                close(mFiles[i]);
            }
            delete blockOrder;
        }
        else {
            std::cout <<"(source) File size is 0 (possibly failed to open or tazer server not running): " << sourceFileName <<std::endl;
        }
    }
    else{
        std::cout << "CheckCp: fileName threads seed" << argc << std::endl;
    }
    return 0;
}
