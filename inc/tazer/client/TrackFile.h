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

#ifndef TRACKFILE_H
#define TRACKFILE_H
#include "FileCacheRegister.h"
#include "TazerFile.h"
#include <atomic>
#include <mutex>
#include <string>
#include <map>

extern std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_r_stat;
extern std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_r_stat_size;
extern std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_w_stat;
extern std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_w_stat_size;
//extern std::map<std::string, std::map<int, std::tuple<std::atomic<int64_t>, std::atomic<int64_t> > > > track_file_blk_w_stat;

// For tracing
extern std::map<std::string, std::vector<int> > trace_read_blk_seq;
extern std::map<std::string, std::vector<int> > trace_write_blk_seq;

class TrackFile : public TazerFile {
public:
  TrackFile(std::string name, int fd, bool openFile = true);
  ~TrackFile();

  void open();
  void close();
  uint64_t fileSize();

  ssize_t read(void *buf, size_t count, uint32_t index = 0);
  ssize_t write(const void *buf, size_t count, uint32_t index = 0);
  off_t seek(off_t offset, int whence, uint32_t index = 0);
  int vfprintf(unsigned int pos, int count);

private:
// bool trackRead(size_t count, uint32_t index, uint32_t startBlock, uint32_t endBlock);
//    uint64_t copyBlock(char *buf, char *blkBuf, uint32_t blk, uint32_t startBlock, uint32_t endBlock, uint32_t fpIndex, uint64_t count);

  std::mutex _openCloseLock;
  // std::atomic<uint64_t> 
  uint64_t _fileSize;
  uint32_t _numBlks;
  uint32_t _regFileIndex;
  int _fd_orig;
  std::string _filename;
  std::hash<int64_t> hashed;
  std::chrono::high_resolution_clock::time_point open_file_start_time;
  std::chrono::high_resolution_clock::time_point close_file_end_time;
  std::chrono::seconds total_time_spent_read;
  std::chrono::seconds total_time_spent_write;
};


#endif /* LOCALFILE_H */
