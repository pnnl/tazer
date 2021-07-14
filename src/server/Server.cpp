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

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <system_error>
#include <time.h>
#include <unistd.h>
#include <unordered_set>

#include "Config.h"
//#include "CounterData.h"
#include "Connection.h"
#include "Message.h"
#include "RSocketAdapter.h"
#include "ServeFile.h"
#include "ThreadPool.h"
#include "lz4.h"
#include "lz4hc.h"
#include "xxhash.h"

int sockfd = -1;
std::atomic_bool alive(true);
ThreadPool<std::function<void()>> threadPool(Config::numServerThreads);

void openFile(Connection *connection, char *buff) {
    unsigned int blkSize;
    bool compress;
    bool output;
    std::string fileName = parseOpenFileMsg(buff, blkSize, compress, output);
    ServeFile *file = ServeFile::addNewServeFile(fileName, compress, blkSize, (compress) ? 0 : Config::numCompressTask, output, Config::removeOutput);
    // if (file){
    //     std::cout<<"opened: "<<file->name()<<std::endl;
    // }
    bool opened = (file != NULL);
    uint64_t size = (opened) ? file->size() : 0;
    if (sendFileSizeMsg(connection, fileName, size, opened)) {
        //        if(opened) {
        //            PRINTF("Opened %s size: %lu blkSize: %u compress: %u\n", file->name().c_str(), file->size(), file->blkSize(), file->compress());
        //        }
    }
    else if (opened) {
        //Must remove file since the client will try to reopen...
        ServeFile::removeServeFile(file);
    }
    //std::cout<<"[TAZER] "<<"open file"<<fileName<<" "<<connection->addr()<<":"<<connection->port()<<" "<<file<<std::endl;
}

void closeFile(Connection *connection, char *buff) {
    std::string fileName = parseCloseFileMsg(buff);
    // std::cout<<"[TAZER] "<<"close file"<<fileName<<" "<<connection->addr()<<":"<<connection->port()<<std::endl;
    if (!sendAckMsg(connection, CLOSE_FILE_MSG)) {
        PRINTF("Failed ack close %s\n", fileName.c_str());
    }
    ServeFile::removeServeFile(fileName);

    //  std::cout<<"[TAZER] "<<"close file"<<fileName<<" "<<connection->addr()<<":"<<connection->port()<<std::endl;
}

void requestBlockFromFile(Connection *connection, char *buff) {
    unsigned int start, end;
    std::string fileName = parseRequestBlkMsg(buff, start, end);
    //PRINTF("Request blocks %u - %u from %s\n", start, end, fileName.c_str());
    ServeFile *file = ServeFile::getServeFile(fileName);
    std::cout<<"[TAZER] "<<"get block "<<fileName<<" "<<connection->addr()<<":"<<connection->port()<<" "<<file<<std::endl;
    if (file) {
        for (unsigned int i = start; i <= end; i++) {
            if (!file->transferBlk(connection, i)) {
                PRINTF("Block transfer failed\n");
                break;
            }
        }
    }
    else {
        PRINTF("No such file exists to read from: %s\n", fileName.c_str());
    }
}

void writeDataToFile(Connection *connection, char *buff) {
    unsigned int dataSize = 0;
    unsigned int compSize = 0;
    uint64_t fp = 0;
    char *data = NULL;
    std::string fileName = parseWriteMsg(buff, &data, dataSize, compSize, fp);
    ServeFile *file = ServeFile::getServeFile(fileName);
    if (file) {
        if (compSize != dataSize) {
            char *decompData = new char[dataSize + 1];
            if (LZ4_decompress_safe(data, decompData, compSize, dataSize + 1) < 0)
                PRINTF("A negative result from LZ4_decompress_fast indicates a failure trying to decompress the data.  See exit code (echo $?) for value returned. ");
            data = decompData;
        }

        //        PRINTF("WRITING DATA: %s %u %lu %p %s\n", fileName.c_str(), dataSize, fp, data, data);
        //PRINTF("WRITING DATA: %s %u %lu %p\n", fileName.c_str(), dataSize, fp, data);
        file->writeData(data, dataSize, fp);

        sendAckMsg(connection, WRITE_MSG);

        if (compSize != dataSize)
            delete[] data;
    }
    else {
        PRINTF("No such file exists to read from: %s\n", fileName.c_str());
    }
}

void getFileSize(Connection *connection, char *buff) {
    std::string fileName = parseRequestFileSizeMsg(buff);
    struct stat64 sb;
    int ret = stat64(fileName.c_str(), &sb);
    bool opened = (ret != -1);
    uint64_t size = (opened) ? (uint64_t)sb.st_size : 0;
    if (!sendFileSizeMsg(connection, fileName, size, opened)) {
        PRINTF("Failed to send file size %s\n", fileName.c_str());
    }
}

void pingResponse(Connection *connection, char *buff) {
    if (!sendAckMsg(connection, PING_MSG)) {
        PRINTF("Failed acking ping\n");
    }
}

void shutDownServer() {
    PRINTF("Received server shutdown %u!\n", sockfd);
    alive.store(false);
    rshutdown(sockfd, SHUT_RDWR);
    rclose(sockfd);
}

void pollConnection(Connection *connection) {
    if (connection && alive.load()) {
        unsigned int closeConCount = 0;
        connection->lock();
        while (pollWrapper(connection) > 0) { //We have a message!
            char *buff = NULL;
            if (pollRecWrapper(connection, &buff) >= (int64_t)sizeof(msgHeader)) {

                printMsgHeader(buff);

                msgHeader *header = (msgHeader *)buff;
                switch (header->type) {
                case OPEN_FILE_MSG: {
                    openFile(connection, buff);
                    break;
                }
                case REQ_FILE_SIZE_MSG: {
                    getFileSize(connection, buff);
                    break;
                }
                case REQ_BLK_MSG: {
                    requestBlockFromFile(connection, buff);
                    break;
                }
                case WRITE_MSG: {
                    writeDataToFile(connection, buff);
                    break;
                }
                case CLOSE_FILE_MSG: {
                    closeFile(connection, buff);
                    break;
                }
                case CLOSE_SERVER_MSG: {
                    shutDownServer();
                    break;
                }
                case PING_MSG:
                    pingResponse(connection, buff);
                case CLOSE_CON_MSG:
                default: //Close connection on incorrect message type
                    connection->closeSocket();
                }
                if (buff)
                    delete[] buff;
            }
            else { //We were not able to read a message header... Must be an error... close socket
                PRINTF("Recv failed for client %s\n", connection->addr().c_str());
                connection->closeSocket();
            }
        }

        closeConCount += connection->unlock(); //Check and see how many connections were closed inside connection class
        bool deleted = (closeConCount) ? Connection::removeConnection(connection, closeConCount) : false;
        if (!deleted) { //Put the connection back into the task queue
            //Side note on the capture:  Connection is captured by value since it is a pointer
            threadPool.addTask([connection] { pollConnection(connection); });
        }
    }
}

int main(int argc, char *argv[]) {
    ConnectionPool::useCnt = new std::unordered_map<std::string, uint64_t>();
    ConnectionPool::consecCnt = new std::unordered_map<std::string, uint64_t>();
    ConnectionPool::stats = new std::unordered_map<std::string, std::pair<double, double>>();
    Loggable::mtx_cout = new std::mutex();
    ServeFile::cache_init();

    unsigned int portno = Config::serverPort;
    std::string addr("");
    if (argc >= 2)
        portno = atoi(argv[1]);
    if (argc == 3)
        addr += argv[2];

    if (argc == 2){
        if (getInSocket(portno, sockfd) < 0) {
            perror("ERROR on binding");
            exit(1);
        }
    }

    if (argc == 3) {
        if (getInSocketOnInterface(argv[1], sockfd, addr.c_str()) < 0) {
            perror("ERROR on binding");
            exit(1);
        }
    }

    PRINTF("Starting server on port %d socket %d\n", portno, sockfd);
    std::cerr << "[TAZER] "
              << "Starting server on port " << portno << " socket " << sockfd << std::endl;
    //    signal(SIGCHLD, SIG_IGN); //hack for now, possibly implement a child handler?
    rlisten(sockfd, 128);
    struct sockaddr_in cli_addr;
    while (alive.load()) {
        socklen_t clilen = sizeof(cli_addr);
        int newsockfd = raccept(sockfd, (struct sockaddr *)&cli_addr, &clilen); //newsockfd is either used to create a new connection or added to existing connection.
        if (alive.load() && newsockfd > 0) {
            std::string clientAddr(inet_ntoa(cli_addr.sin_addr));
            std::cout<<"[TAZER] " << clientAddr << ":" << cli_addr.sin_port << " " << newsockfd << std::endl;
            threadPool.addThreadWithTask([=] {
                bool created = false;
                Connection *newCon = Connection::addNewHostConnection(clientAddr, cli_addr.sin_port, newsockfd, created);
                if (newCon) {
                    std::cout << "[TAZER] " << clientAddr << ":" << cli_addr.sin_port << " " << newsockfd << " " << created << std::endl;
                    if (created) {
                        pollConnection(newCon);
                    }
                }
                else {
                    rclose(newsockfd);
                }
            });
        }
    }
    threadPool.terminate(true);
    Connection::closeAllConnections();
    PRINTF("Exiting Server\n");
    return 0;
}
