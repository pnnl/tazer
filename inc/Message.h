// -*-Mode: C++;-*- // technically C99

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

#ifndef MESSAGE_H
#define MESSAGE_H

#include "Connection.h"

#define MAGIC 0x32ab4fd

enum msgType {
    OPEN_FILE_MSG=0,
    REQ_FILE_SIZE_MSG,
    FILE_SIZE_MSG,
    REQ_BLK_MSG,
    SEND_BLK_MSG,
    CLOSE_FILE_MSG,
    CLOSE_CON_MSG,
    CLOSE_SERVER_MSG,
    PING_MSG,
    WRITE_MSG,
    ACK_MSG
};

#pragma pack(push, 1)

struct msgHeader {
    uint64_t magic;
    msgType type;
    uint32_t fileNameSize;
    uint32_t size;
};

struct openFileMsg {
    msgHeader header;
    uint32_t blkSize;
    uint8_t compress;
    uint8_t output;
    char name[];
};

struct fileSizeMsg {
    msgHeader header;
    uint64_t fileSize;
    uint8_t open;
    char name[];
};

struct requestFileSizeMsg {
    msgHeader header;
    char name[];
};

struct requestBlkMsg {
    msgHeader header;
    uint32_t start;
    uint32_t end;
    char name[];
};

struct sendBlkMsg {
    msgHeader header;
    int compression;
    uint32_t blk;
    uint32_t dataSize;
    char data[];
};

struct closeFileMsg {
    msgHeader header;
    char name[];
};

struct closeConMsg {
    msgHeader header;
};

struct closeServerMsg {
    msgHeader header;
};

struct writeMsg {
    msgHeader header;
    uint32_t dataSize;
    uint32_t compSize;
    uint64_t fp;
    unsigned int sn;
    char data[];
};

struct ackMsg {
    msgHeader header;
    msgType ackType;
};

#pragma pack(pop)

void printMsgHeader(char *pkt);
msgType getMsgType(char *msg);
bool checkMsg(char *pkt, unsigned int size);
void fillMsgHeader(char *buff, msgType type, unsigned int fileNameSize, unsigned int size);

//These are wrappers to support fault tolerance support stuff...
bool clientSendRetry(Connection *connection, char *buff, unsigned int size);
bool serverSendClose(Connection *connection, char *buff, unsigned int size);
bool serverSendCloseNew(Connection *connection, sendBlkMsg *packet, std::string name, char *buff, unsigned int size);
int64_t clientRecRetry(Connection *connection, char **buff);
int64_t clientRecRetryCount(Connection *connection, char *buff, int64_t size);

bool sendOpenFileMsg(Connection *connection, std::string name, unsigned int blkSize, bool compress, bool output);
std::string parseOpenFileMsg(char *pkt, unsigned int &blkSize, bool &compress, bool &output);

bool sendRequestBlkMsg(Connection *connection, std::string name, unsigned int start, unsigned int end);
std::string parseRequestBlkMsg(char *pkt, unsigned int &start, unsigned int &end);

bool sendCloseFileMsg(Connection *connection, std::string name);
std::string parseCloseFileMsg(char *pkt);

bool sendCloseConMsg(Connection *connection);

bool sendFileSizeMsg(Connection *connection, std::string name, uint64_t fileSize, uint8_t open);
std::string parseFileSizeMsg(char *pkt, uint64_t &fileSize, uint8_t &open);
bool recFileSizeMsg(Connection *connection, uint64_t &fileSize);

bool sendSendBlkMsg(Connection *connection, std::string name, unsigned int blk, char *data, unsigned int dataSize);
std::string recSendBlkMsg(Connection *connection, char **data, unsigned int &blk, unsigned int &dataSize, unsigned int dataBufSize = 0);

bool sendAckMsg(Connection *connection, msgType ackType);
bool recAckMsg(Connection *connection, msgType expMstType);

bool sendRequestFileSizeMsg(Connection *connection, std::string name);
std::string parseRequestFileSizeMsg(char *pkt);

bool sendCloseServerMsg(Connection *connection);

bool sendPingMsg(Connection *connection);

bool sendWriteMsg(Connection *connection, std::string name, char *data, unsigned int dataSize, unsigned int compSize, uint64_t fp, unsigned int sn);
std::string parseWriteMsg(char *pkt, char **data, unsigned int &dataSize, unsigned int &compSize, uint64_t &fp);

int pollWrapper(Connection *connection);
int64_t pollRecWrapper(Connection *connection, char **buff);
#endif /* MESSAGE_H */
