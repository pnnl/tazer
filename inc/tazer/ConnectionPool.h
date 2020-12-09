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

#ifndef CONNECTIONPOOL_H_
#define CONNECTIONPOOL_H_
#include "Connection.h"
#include "Trackable.h"
#include <list>
#include <mutex>

class ConnectionPool : public Trackable<std::string, ConnectionPool *> {
public:
  ConnectionPool(std::string name, bool compress, std::vector<Connection *> &connections);
  virtual ~ConnectionPool();
  void pushConnection(Connection *connection, bool useTL = false);
  Connection *popConnection();
  Connection *popConnection(uint64_t size);
  int numConnections();

  uint64_t openFileOnAllServers();
  bool closeFileOnAllServers();
  static ConnectionPool *addNewConnectionPool(std::string filename, bool compress, std::vector<Connection *> &connections, bool &created);
  static bool removeConnectionPool(std::string filename, unsigned int dec = 1);
  static void removeAllConnectionPools();
  static std::unordered_map<std::string, uint64_t> *useCnt;
  static std::unordered_map<std::string, uint64_t> *consecCnt;
  static std::unordered_map<std::string, std::pair<double, double>> *stats;

private:
  void addOpenFileTask(Connection *server);
  bool openFileOnServer(Connection *server);

  std::string _name;
  uint64_t _fileSize;
  bool _compress;

  std::vector<Connection *> _connections;
  std::atomic_int _servers_requested;
  std::atomic_bool _valid_server;

  double _startTime;

  struct ConnectionCompare {
    double rate;
    double wRate;
    Connection *connection;

    ConnectionCompare() : rate(1000000000000),
                          wRate(0),
                          connection(NULL) {}

    ConnectionCompare(uint64_t bytes, uint64_t time, Connection *con) : rate(1000000000000),
                                                                        wRate(0),
                                                                        connection(con) {
      if (time) {
        rate = (bytes * 1000000000) / time;
        // wRate = rate; //* pow(0.95,con->consecutiveCnt());
        // rate=wRate;
        // std::cout<<"[TAZER] "<<"r: "<<rate/1000000.0<<" wr: "<<wRate/1000000.0<<" "<<connection->port()<<std::endl;
      }
    }

    ConnectionCompare(const ConnectionCompare &entry) {
      rate = entry.rate;
      connection = entry.connection;
      wRate = entry.wRate;
    }

    ~ConnectionCompare() {}

    ConnectionCompare &operator=(const ConnectionCompare &other) {
      if (this != &other) {
        rate = other.rate;
        connection = other.connection;
        wRate = other.wRate;
      }
      return *this;
    }

    bool operator()(const ConnectionCompare &lhs, const ConnectionCompare &rhs) const {
      //bool ret = lhs.rate*pow(1.05,_curCnt-lhs.connection->consecutiveCnt()) < rhs.rate;
      bool ret = lhs.rate < rhs.rate;

      // if(rhs.connection->consecutiveCnt()>1){
      //     ret = lhs.rate < rhs.wRate;
      // }
      // std::cout<<"[TAZER] "<<lhs.rate/1000000.0<<"MB/s "<<rhs.rate/1000000.0<<"MB/s "<<rhs.wRate/1000000.0<<"MB/s "<<lhs.connection->port()<<" "<<rhs.connection->port()<<" "<<ret<<std::endl;
      return ret;
    }
  };
  std::list<ConnectionCompare> _mq;
  std::mutex _qMutex;
  static std::atomic<uint64_t> _curCnt;
  std::unordered_map<Connection *, std::pair<double, double>> eta;
  std::unordered_map<Connection *, uint64_t> _prevStart;
  std::unordered_map<Connection *, double> _tmp;
};

#endif /* CONNECTIONPOOL_H_ */