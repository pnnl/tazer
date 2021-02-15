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


#include "Cache.h"
#include "Request.h"
#include "Loggable.h"
#include "Timer.h"

std::atomic<uint64_t> Request::ID_CNT(0);
std::atomic<uint64_t> Request::RET_ID_CNT(0);

RequestTrace Request::trace( std::string tag){
    return trace(true,tag);
}
RequestTrace Request::trace(bool trigger, std::string tag){
    if (trigger && globalTrigger){
        return RequestTrace(&ss,true) << "[" <<Timer::getCurrentTime()<<"] "<<tag<<" --";
    }
    else{
        return RequestTrace(&ss,false);
    }    
}

Request::~Request(){
    Request::RET_ID_CNT.fetch_add(1);
    // Loggable::debug()<<"request-- deleting req: "<<id <<"("<<Request::ID_CNT.load()<<", "<<Request::RET_ID_CNT.load()<<")"<<std::endl;
    originating->cleanUpBlockData(data);
    if (printTrace){
        Loggable::log()<<str();
    }
}

std::string Request::str(){
    std::stringstream tss;
    tss<<"Req ["<<id<<"]"<<std::endl
    <<"orignal cache: "<<originating->name()<<std::endl
    <<"blk: " <<blkIndex<<std::endl
    <<"file: "<<fileIndex<<std::endl
    <<"size: "<<size<<std::endl
    <<"time: "<<(double)time/ 1000000000.0<<std::endl
    <<"retry time: "<<(double)retryTime/ 1000000000.0<<std::endl
    <<"waitingCache: "<<waitingCache <<std::endl
    <<"Reserved Map: [";
    for (auto vals : reservedMap){
        tss<<vals.first->name()<<" : "<<(int)vals.second<<", ";
    }
    tss<<"]"<<std::endl
    <<" index Map: [";
    for (auto vals : indexMap){
        tss<<vals.first->name()<<" : "<<(int)vals.second<<", ";
    }
    tss<<"]"<<std::endl
    <<"blkindex Map: [";
    for (auto vals : blkIndexMap){
        tss<<vals.first->name()<<" : "<<(int)vals.second<<", ";
    }
    tss<<"]"<<std::endl
    <<" fileindex Map: [";
    for (auto vals : fileIndexMap){
        tss<<vals.first->name()<<" : "<<(int)vals.second<<", ";
    }
    tss<<"]"<<std::endl
    <<" status Map: [";
    for (auto vals : statusMap){
        tss<<vals.first->name()<<" : "<<(int)vals.second<<", ";
    }
    tss<<"]"<<std::endl;
    tss<<ss.str()<<std::endl;;
    return tss.str();

}