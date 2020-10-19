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

#include <iostream>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "UrlDownload.h"

#define WINDOW 10
#define MAXREAD 200

struct MemoryChunk {
    char * memory;
    size_t size;
    unsigned int start;
    unsigned int end;
    unsigned int current;

    MemoryChunk() : 
        memory(NULL), 
        size(1),
        start(0),
        end(0),
        current(0) {
            memory = (char*) malloc(1);
            memory[0] = '\n';
            size = 1;
        }

    ~MemoryChunk() {
        if(memory)
            free(memory);
    }

    void * get() {
        void * ret = memory;
        memory = NULL;
        return ret;
    }
};

size_t tester(char * contents, size_t size, size_t nmemb, void * userp) {
    size_t realsize = size * nmemb;
    if(nmemb) {
        MemoryChunk * mem = (MemoryChunk *) userp;
        unsigned int start = mem->current;
        unsigned int end = start + nmemb - 1;
        unsigned int copyStart;
        unsigned int copySize;
        std::cout << start << " - " << end << " " << mem->start << " - " << mem->end << " " << size << " " << nmemb << std::endl;
        if(end < mem->start || start > mem->end) { //Outside the boundary
            // std::cout << "outside" << std::endl;
            copyStart = 0;
            copySize = 0;
        }
        else if(mem->start <= start && start <= mem->end && mem->start <= end && end <= mem->end) { //Completely inside
            // std::cout << "inside" << std::endl;
            copyStart = 0;
            copySize = nmemb;
        }
        else if(start <= mem->start && mem->end <= end) {
            // std::cout << "over" << std::endl;
            copyStart = mem->start - mem->current;
            copySize = mem->end - mem->start + 1;
        }
        else if(start < mem->start && end <= mem->end) { //Shifted left
            // std::cout << "left" << std::endl;
            copyStart = mem->start - mem->current;
            copySize = nmemb - copyStart;
        }
        else if(start <= mem->end && mem->end < end) { //Shifted right
        // std::cout << "right" << std::endl;
            copyStart = 0;
            copySize = mem->end - mem->current + 1;
        }
        else { //Something is wrong
            std::cout << "This is wrong" << std::endl;
            copyStart = 0;
            copySize = 0;
        }

        copySize*=size;
        std::cout << copyStart << " " << copySize << std::endl;
        if(copySize) {
            char * ptr = (char*) realloc(mem->memory, mem->size + copySize + 1);
            if(ptr == NULL) {
                printf("Out-of-memory\n");
                return 0;
            }
            mem->memory = ptr;
            memcpy(&(mem->memory[mem->size - 1]), (void*)&contents[copyStart], copySize);
            mem->size += copySize;
            mem->memory[mem->size] = '\0';
        }
        mem->current+=nmemb;
    }
    return realsize;
}

int fillWindow(int fd, char * bytes) {
    int size = 0;
    while(size<WINDOW) {
        size += read(fd, &bytes[size], WINDOW-size);
    }
    return size;
}

bool checkCopy(std::string path, MemoryChunk * chunk) {
    bool ret = true;
    int fd = open(path.c_str(), O_RDONLY);
    char byte;
    int current = 0;
    int currentDataByte = 0;
    while(current < MAXREAD) {
        int bytes = read(fd, &byte, 1);
        if(bytes == 1) {
            if(chunk->start <= current && current <= chunk->end) {
                if(byte != chunk->memory[currentDataByte]) {
                    std::cout << "Data does not match on " << current << " " << (unsigned int)byte << " " << (unsigned int)chunk->memory[currentDataByte] << std::endl;
                    ret = false;
                }
                currentDataByte++;
            }
            current++;
        }
    }
    close(fd);
    return ret;
}


int main(int argc, char *argv[]) {
    MemoryChunk chunk;
    chunk.start = std::stoi(argv[2]);
    chunk.end = std::stoi(argv[3]);

    std::string path(argv[1]);
    int fd = open(path.c_str(), O_RDONLY);

    char data[WINDOW];
    int current = 0;
    while(current < MAXREAD) {
        int winSize = fillWindow(fd, data);
        tester(data, sizeof(char), WINDOW, (void*)&chunk);
        current+=winSize;
    }
    close(fd);

    if(checkCopy(path, &chunk))
        std::cout << "Passed" << std::endl;
    return 0;
}
