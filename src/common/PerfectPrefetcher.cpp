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

#include "PerfectPrefetcher.h"
#include "Loggable.h"
#include "Config.h"

#include <iostream>
#include <sstream>


PerfectPrefetcher::PerfectPrefetcher(std::string name, std::string fileName) : Prefetcher(name),
                                                                               _lastIndex(-1) {
    *this << "[TAZER] " << "Constructing " << _name << std::endl;
    std::cout << "[TAZER] " << "Constructing " << _name << " for file: " << fileName << std::endl;

    loadAccessTrace(fileName);
}


PerfectPrefetcher::~PerfectPrefetcher() {
    *this << "[TAZER] " << " Ending " << _name << std::endl;
}


std::vector<uint64_t> PerfectPrefetcher::getBlocks(uint32_t fileIndex, uint64_t startBlk, uint64_t endBlk, uint64_t numBlocks, uint64_t blkSize, uint64_t fileSize) {
    *this << "[TAZER] " << _name << "Getting list of blocks to prefetch" << std::endl;

    std::vector<uint64_t>  blocks;

    if (_lastIndex == _trace.size()-1) {
        *this << "[TAZER] " << _name << " End of trace reached" << std::endl;
        return blocks;
    }

    //Get next pair
    _lastIndex++;

//    if (_trace[_lastIndex].first == _trace[_lastIndex-1].first && _trace[_lastIndex].second == _trace[_lastIndex-1].second) {
//        *this << "[TAZER] " << _name << " Reusing previous blocks. No prefetching done." << std::endl;
//        return blocks;
//    }

    for (uint32_t blk = _trace[_lastIndex].first; blk < _trace[_lastIndex].second; blk++) {
        blocks.push_back(blk);
    }

    return blocks;
}


void PerfectPrefetcher::loadAccessTrace(std::string fileName) {
    //TODO: For now, let's assume that the access filename (name.access)

    //*this << "[TAZER] " << _name << " Loading access trace" << fileName << std::endl;

    fileName.resize(fileName.size() - 7);
    //std::string accessTraceName = "/files0/lori296/" + fileName + "access";
    std::string accessTraceName = Config::prefetchFileDir + fileName + "access";

    *this << "[TAZER] " << _name << " Loading access trace " << accessTraceName << std::endl;

    std::ifstream file;
    file.open(accessTraceName);

    if (!file.is_open()) {
        std::cerr << "Error opening the file" << std::endl;
        exit(-1);
    }
    std::string line;
    //Parse blocks
    while (!file.eof()) {
        std::getline(file, line);
        std::stringstream ssin(line);
        std::pair<uint64_t,uint64_t> pair;
        ssin >> pair.first;
        ssin >> pair.second;
        _trace.push_back(pair);
    }
    file.close();

}