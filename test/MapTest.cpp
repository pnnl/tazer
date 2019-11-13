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

#include "Config.h"
#include "ThreadPool.h"
#include "string.h"
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

ThreadPool<std::function<void()>> pool(10);
const unsigned int blockSize = 1024;
const unsigned int progressReport = 128;
std::map<std::string, std::atomic_int *> dictionary;
std::atomic_int progress;

char *makeBuffer(unsigned int size) {
    char *buffer = new char[size + 1];
    if (buffer) {
        memset(buffer, '0', sizeof(char) * size);
        buffer[size] = '\0';
        return buffer;
    }
    return NULL;
}

char *readFromFile(int fd, int size) {
    char *buff = makeBuffer(size);
    if (buff) {
        char *local = buff;
        while (size) {
            int bytes = read(fd, local, size);
            if (bytes >= 0) {
                local += bytes;
                size -= bytes;
            }
            else {
                std::cout << "Failed a read " << fd << " " << size << std::endl;
                delete[] buff;
                return NULL;
            }
        }
    }
    else {
        std::cout << "Failed to create buffer" << std::endl;
    }
    return buff;
}

void split(char *buff, bool init) {
    std::stringstream ss(buff);
    std::string line;
    std::string item;
    if (init) {
        while (std::getline(ss, line)) {
            dictionary[line] = new std::atomic_int(0);
        }
    }
    else {

        while (std::getline(ss, line)) {
            std::stringstream ls(line);
            while (std::getline(ls, item, ' ')) {
                if (dictionary.count(item))
                    dictionary[item]->fetch_add(1);
            }
        }
    }
}

void fillDictionary(std::string fileName) {
    struct stat64 sb;
    if (lstat64(fileName.c_str(), &sb) != -1) {
        unsigned int fileSize = sb.st_size;
        int fd = open(fileName.c_str(), O_RDONLY);
        if (fd > 0) {
            char *buff = readFromFile(fd, fileSize);
            if (buff) {
                split(buff, true);
                delete[] buff;
            }
            close(fd);
        }
        else
            std::cout << "Failed to open dictionary: " << fileName << std::endl;
    }
    else
        std::cout << "Failed to read dictionary" << std::endl;
}

void readTest(std::string fileName, unsigned int start, unsigned int size) {
    //    std::cout << fileName << " " << start << " " << size << std::endl;
    int fd = open(fileName.c_str(), O_RDONLY);
    if (fd > 0) {
        lseek(fd, start, SEEK_SET);
        char *buff = readFromFile(fd, size);
        if (buff) {
            split(buff, false);
            delete[] buff;
        }
        close(fd);
    }
    else
        std::cout << "Failed to open file" << std::endl;
}

int main(int argc, char *argv[]) {
    std::string dictionaryFileName(argv[1]);
    fillDictionary(dictionaryFileName);

    if (dictionary.size()) {
        pool.initiate();

        for (int i = 2; i < argc; i++) {
            std::string fileName(argv[i]);
            struct stat64 sb;
            if (lstat64(fileName.c_str(), &sb) != -1) {
                unsigned int fileSize = sb.st_size;
                unsigned int numBlks = fileSize / blockSize;
                if (fileSize % blockSize)
                    numBlks++;

                for (unsigned int j = 0; j < numBlks; j++) {
                    pool.addTask([fileName, j, numBlks, fileSize] {
                        readTest(fileName, j * blockSize, (j + 1 == numBlks) ? fileSize - j * blockSize : blockSize);
                    });
                }
            }
            else
                std::cout << "Failed to read file " << fileName << std::endl;
        }

        pool.wait();
        pool.terminate();

        for (auto it = dictionary.begin(); it != dictionary.end(); ++it) {
            std::cout << it->first
                      << ':'
                      << it->second->load()
                      << std::endl;
        }
    }
    return 0;
}
