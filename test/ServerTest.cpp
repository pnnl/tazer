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
#include "Connection.h"
#include "Message.h"
#include <string>

const std::string fileName(Config::InputsDir + "/warAndPeace.txt");

//Wrapper function to read blocks and print them
void readBlocks(Connection *connection, std::string name, unsigned int start, unsigned int end, std::stringstream &ss) {
    if (sendRequestBlkMsg(connection, name, start, end)) {
        std::cout << "Requesting file block " << name << " " << start << " - " << end << std::endl;
        for (unsigned int i = start; i <= end; i++) {
            char *data = NULL;
            unsigned int blk = 0, dataSize = 0;
            std::string fileName = recSendBlkMsg(connection, &data, blk, dataSize);
            std::cout << "Received " << fileName << " " << blk << " " << dataSize << std::endl;
            std::string temp(data, dataSize);
            ss << temp;
        }
    }
}

int main(int argc, char **argv) {
    int64_t blkSize = 128;
    bool compress = false;
    std::stringstream ss;

    //Start a new connection
    std::cout << Config::serverIpString << " " << Config::serverPort << std::endl;
    Connection *connection = Connection::addNewClientConnection(Config::serverIpString, Config::serverPort, false);
    connection->lock();

    //Open a file
    if (sendOpenFileMsg(connection, fileName, blkSize, compress, false)) {
        uint64_t fileSize = 0;
        recFileSizeMsg(connection, fileSize);
        std::cout << "Open file " << fileName << " " << fileSize << " " << blkSize << "  " << compress << std::endl;
    }

    //Read some blocks
    readBlocks(connection, fileName, 0, 10, ss);

    //Open the same file again
    if (sendOpenFileMsg(connection, fileName, blkSize, compress, false)) {
        uint64_t fileSize = 0;
        recFileSizeMsg(connection, fileSize);
        std::cout << "Open file " << fileName << " " << fileSize << " " << blkSize << "  " << compress << std::endl;
    }

    //Read some more blocks
    readBlocks(connection, fileName, 11, 15, ss);

    //Close the file
    if (sendCloseFileMsg(connection, fileName)) {
        std::cout << "Closing file " << fileName << std::endl;
        if (recAckMsg(connection, CLOSE_FILE_MSG)) {
            std::cout << "Closed file" << fileName << std::endl;
        }
    }

    //Make sure it is still open because we opened it twice
    readBlocks(connection, fileName, 16, 20, ss);

    //Close the file for the last time
    if (sendCloseFileMsg(connection, fileName)) {
        std::cout << "Closing file " << fileName << std::endl;
        if (recAckMsg(connection, CLOSE_FILE_MSG)) {
            std::cout << "Closed file" << fileName << std::endl;
        }
    }

    //Close the connection
    if (sendCloseConMsg(connection)) {
        std::cout << "Closed" << std::endl;
    }
    connection->unlock();
    std::cout << ss.str() << std::endl;
    return 0;
}
