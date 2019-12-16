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
#include <fstream>
#include <string>
#include <cstring>
#include "ThreadPool.h"
#include "UrlDownload.h"

unsigned int numThreads = 20;
ThreadPool<std::function<void()>> pool(numThreads);
std::mutex _lock;

unsigned int findUrlSize(std::string &line) {
    unsigned int size = 0;
    
    char * del = " \t\r\n";
    char * str = (char*) malloc(line.size() + 1);
    memcpy(str, line.c_str(), line.size());
    str[line.size()] = '\0';
    
    char * savePtr;
    char * token = strtok_r(str, del, &savePtr);
    while(token) {
        int temp = sizeUrlPath(token);
        if(temp > -1) {
            line = std::string(token);
            size = temp;
            break;
        }
        token = strtok_r(NULL, " ", &savePtr);
    }
    free(str);
    return size;
}

int main(int argc, char *argv[]) {
    std::string inPath(argv[1]);
    std::string outPath(argv[2]);
    
    pool.initiate();
    for(unsigned int i=0; i<numThreads; i++) {
        pool.addTask([i, inPath, outPath] {
            std::ifstream infile(inPath);
            std::vector<std::string> toWrite;
            if(infile.is_open()) {
                int counter = 0;
                std::string line;
                while(!infile.eof()) {
                    std::getline(infile, line);
                    if(counter % numThreads == i) {
                        if(line.length()) {
                            unsigned int fs = findUrlSize(line);
                            if(fs)
                                toWrite.push_back(line);
                        }
                    }
                    counter++;
                }
                infile.close();
                
                std::unique_lock<std::mutex> lock(_lock);
                std::ofstream outfile;
                outfile.open(outPath, std::ios::out | std::ios::app);
                if(outfile.is_open()) {
                    while(!toWrite.empty()) {
                        outfile << toWrite.back() << std::endl;
                        toWrite.pop_back();
                    }
                    outfile.close();
                }
                else
                    std::cout << "Failed to open " << outPath << std::endl;
                lock.unlock();
            }
            else
                std::cout << "Failed to open " << inPath << std::endl;
        });
    }
    pool.wait();
    pool.terminate();
    return 0;
}

