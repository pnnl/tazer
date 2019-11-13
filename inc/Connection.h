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

#ifndef CONNECTION_H_
#define CONNECTION_H_

#include "Loggable.h"
#include "RSocketAdapter.h"
#include "Trackable.h"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

class Connection : public Loggable, public Trackable<std::string, Connection *> {
  public:
    Connection(std::string hostAddr, int port);             //Client
    Connection(int sock, std::string clientAddr, int port); //Server
    ~Connection();

    void lock();
    int unlock();

    void incCnt();
    void setCnt(uint64_t cnt);
    void clearCnt();
    uint64_t consecutiveCnt();

    int initializeSocket();
    bool addSocket();                           //Client only
    bool initiate(unsigned int numConnections); //Client only

    bool addSocket(int socket); //Server only
    int pollMsg();              //Server only

    int closeSocket();
    int closeSocket(int &socket);
    int forceCloseSocket(int &socket);
    bool restartSocket();

    int64_t sendMsg(void *msg, int64_t msgSize);
    int64_t recvMsg(char *buf, int64_t bufSize);
    int64_t recvMsg(char **dataPtr);

    int id();
    std::string addr();
    unsigned int port();
    std::string addrport();
    bool open();
    unsigned int numSockets();
    int localSocket();

    static Connection *addNewClientConnection(std::string hostAddr, int port, unsigned int numCreated = 0);
    static Connection *addNewHostConnection(std::string clientAddr, int port, int socket, bool &created);
    static bool removeConnection(Connection *connection, unsigned int dec = 1);
    static bool removeConnection(std::string addr, unsigned int dec = 1);
    static bool removeServerConnection(Connection *connection, unsigned int dec = 1);
    static bool removeServerConnection(std::string addr, unsigned int dec = 1);
    static void closeAllConnections();

    // int _tlSocket;
    // int _tlSocketsClosed;

    // uint64_t _tlSocketTime;
    // uint64_t _tlSocketBytes;

  private:
    bool isServer();
    bool isClient();

    std::mutex _sMutex;               //Lock used for _pfds and _sockets
    std::vector<struct pollfd> _pfds; //Used for server
    std::queue<int> _sockets;         //Used for clients
    std::condition_variable _cv;      //Used for clients to pop from queue
    std::atomic_uint _numSockets;     //Number of sockets for both

    unsigned int _nextSocket; //Index used by server to start polling from
    int _inMsgs;

    std::string _addr; //Remote server to connect to
    int _port;         //Remote port to connect to
    std::string _addrport;
    bool _isServer;
    uint64_t _consecutiveCnt;
};

#endif /* CONNECTION_H_ */
