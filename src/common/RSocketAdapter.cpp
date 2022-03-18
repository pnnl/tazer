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

#include "RSocketAdapter.h"
#include "Config.h"
#include <iostream>
#include <string.h>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define IPNAMELENGTH 1024

std::string fixName(std::string name) {
    std::vector<std::string> potentialNames;
#ifdef USE_RSOCKETS
    std::string pre[] = {"ib-", "ib", "ib."};
    std::string post[] = {"-ib", "ib", ".ib", "-ib.ib", ".ibnet"};

    for (std::string toAdd : pre)
        potentialNames.push_back(toAdd + name);
    for (std::string toAdd : post)
        potentialNames.push_back(name + toAdd);
#endif
    potentialNames.push_back(name);
    for (std::string nameToTry : potentialNames) {
        struct addrinfo *result;
        if (!getaddrinfo(nameToTry.c_str(), NULL, NULL, &result)) {
            PRINTF("%s out of %lu names\n", nameToTry.c_str(), potentialNames.size());
            char ip[IPNAMELENGTH];
            struct sockaddr_in *res = (struct sockaddr_in *)result->ai_addr;
            inet_ntop(AF_INET, &res->sin_addr, ip, IPNAMELENGTH);
            freeaddrinfo(result);
            std::string newName(ip);
            return newName;
        }
    }
    return name;
}

int getOutSocket(std::string name, unsigned int port) {
    int sock = rsocket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        PRINTF("Failed to get a socket\n");
        return -1;
    }
    struct sockaddr_in serv_addr;
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    std::string ip = fixName(name);
    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
        PRINTF("Could not resolve %s @ %s\n", name.c_str(), ip.c_str());
        rclose(sock);
        return -1;
    }

#ifndef USE_RSOCKETS
    struct timeval timeout;
    timeout.tv_sec = 7200;
    timeout.tv_usec = 0;
    rsetsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    rsetsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
#endif
    int optval = 1;
    if (rsetsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval))) {
        PRINTF("Could not set option to %s @ %s %s\n", name.c_str(), ip.c_str(), strerror(errno));
        rclose(sock);
        return -1;
    }
    struct sockaddr *ptr = (struct sockaddr *)&serv_addr;
    if (rconnect(sock, ptr, sizeof(struct sockaddr_in)) < 0) {
        PRINTF("Could not connect to %s @ %s %s\n", name.c_str(), ip.c_str(), strerror(errno));
        rclose(sock);
        return -1;
    }
    //    PRINTF("%s %u %d\n", ip.c_str(), port, sock);
    return sock;
}

int getInSocket(unsigned int port, int &socketFd) {
    if ((socketFd = rsocket(PF_INET, SOCK_STREAM, 0)) == -1) {
        PRINTF("Failed to get a socket\n");
    }
    int set = 1;
    rsetsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, (char *)&set, sizeof(set));

    struct sockaddr_in sock;
    memset((char *)&sock, 0, sizeof(struct sockaddr_in));
    sock.sin_family = AF_INET;
    sock.sin_addr.s_addr = htonl(INADDR_ANY);
    sock.sin_port = htons(port);

    return rbind(socketFd, (struct sockaddr *)&sock, sizeof(struct sockaddr_in));
}

int getInSocketOnInterface(const char * port, int &socketFd, const char *addr) {
    if ((socketFd = rsocket(PF_INET, SOCK_STREAM, 0)) == -1) {
        PRINTF("Failed to get a socket\n");
    }
    int set = 1;
    rsetsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, (char *)&set, sizeof(set));

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    std::cout<<addr<<std::endl;
    s = getaddrinfo(addr, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    // struct sockaddr_in sock;
    // memset((char *)&sock, 0, sizeof(struct sockaddr_in));
    // sock.sin_family = AF_INET;
    // sock.sin_addr.s_addr = inet_addr(addr);
    // sock.sin_port = htons(port);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        s = rbind(socketFd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0)
            break;                  /* Success */

    }

    freeaddrinfo(result);

   if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }
    return s; //should be 0
}