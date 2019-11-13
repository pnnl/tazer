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

#include "ErrorTester.h"
#include "Message.h"
#include <execinfo.h>
#include <iostream>
#include <map>
#include <string>

const unsigned int maxBackTrace = 50;

std::string errorList[] = {
#if defined(MSG_ERROR) || defined(ALL_ERROR)
    std::string("sendRequestBlkMsg"),
    std::string("sendOpenFileMsg"),
    std::string("sendRequestBlkMsg"),
    std::string("sendCloseFileMsg"),
    std::string("sendCloseConMsg"),
    std::string("sendFileSizeMsg"),
    std::string("recFileSizeMsg"),
    std::string("sendSendBlkMsg"),
    std::string("recSendBlkMsg"),
    std::string("sendAckMsg"),
    std::string("recAckMsg"),
    std::string("sendRequestFileSizeMsg"),
    std::string("sendCloseServerMsg"),
#endif
#if defined(POLL_ERROR) || defined(ALL_ERROR)
    std::string("pollWrapper"),
    std::string("pollRecWrapper"),
#endif
    std::string("last")};

ErrorTester error;

ErrorTester::ErrorTester() : _seed(7) {
    std::unique_lock<std::mutex> lock(_lock);
    srand(_seed);
    lock.unlock();
}

ErrorTester::~ErrorTester() {
}

bool ErrorTester::randomError() {
    bool ret = false;
    if (checkStack()) {
        std::unique_lock<std::mutex> lock(_lock);
        //        ret = (rand() % 2) == 1;
        ret = true;
        lock.unlock();
    }
    return ret;
}

bool ErrorTester::checkError() {
    return error.randomError();
}

template <class RetType>
bool ErrorTester::checkError(RetType pass, RetType fail) {
    if (error.randomError())
        return pass;
    return fail;
}

bool ErrorTester::checkStack() {
    void *bt[maxBackTrace];
    size_t size = backtrace(bt, maxBackTrace);
    char **names = backtrace_symbols(bt, size);
    size_t listSize = sizeof(errorList) / sizeof(std::string);
    for (size_t j = 0; j < listSize - 1; j++) {
        for (size_t i = 0; i < size; i++) {
            char *name = names[i];
            std::string temp(name);
            if (temp.find(errorList[j]) < temp.length()) {
                std::cout<<"[TAZER] " << "Error: " << errorList[j] << std::endl;
                free(names);
                return true;
            }
        }
    }
    free(names);
    return false;
}

void ErrorTester::printStack() {
    PRINTF("HERE ............\n");
    void *bt[maxBackTrace];
    size_t size = backtrace(bt, maxBackTrace);
    char **names = backtrace_symbols(bt, size);
    for (size_t i = 0; i < size; i++) {
        char *name = names[i];
        std::cout<<"[TAZER] " << name << std::endl;
        //            PRINTF("%s\n", name);
    }
    free(names);
}