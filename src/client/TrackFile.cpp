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

#include "TrackFile.h"

#include "UnixIO.h" // to invoke posix read
#include "Config.h"
#include "Connection.h"
#include "ConnectionPool.h"
#include "FileCacheRegister.h"
#include "Message.h"
#include "Request.h"
#include "Timer.h"
#include "UnixIO.h"
#include "lz4.h"
#include "xxhash.h"

#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <cassert>

// #define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

TrackFile::TrackFile(std::string name, int fd, bool openFile) : 
  TazerFile(TazerFile::Type::TrackLocal, name, name, fd),
  _fileSize(0),
  _numBlks(0),
  _fd_orig(fd),
  _filename(name)
  // ,_regFileIndex(id()) 
{ //This is if there is no file cache...
  // std::unique_lock<std::mutex> lock(_openCloseLock);
  // if(_fileSize == (uint64_t) -1)
  //   std::cout << "Failed to open file " << _name << std::endl;
  // else 
  assert(_fd_orig > 0);
  DPRINTF("In Trackfile constructor openfile bool: %d\n", openFile);
  // if(openFile)
    open();
  // else {
   _active.store(true);
  // }
  // lock.unlock();
}

TrackFile::~TrackFile() {
    *this << "Destroying file " << _metaName << std::endl;
    // close();
}

void TrackFile::open() {
  DPRINTF("[TAZER] TrackFile open: %s\n", _name.c_str()) ;
#if 0
  // _closed = false;
  if (track_file_blk_r_stat.find(_name) == track_file_blk_r_stat.end()) {
    track_file_blk_r_stat.insert(std::make_pair(_name, 
						std::map<int, 
						std::atomic<int64_t> >()));
  }
  if (track_file_blk_w_stat.find(_name) == track_file_blk_w_stat.end()) {
    track_file_blk_w_stat.insert(std::make_pair(_name, 
						std::map<int, 
						std::atomic<int64_t> >()));
  }
#endif
  DPRINTF("Returning from trackfile open\n");

    // _blkSize = Config::blockSizeForStat;

    // if (_fileSize < _blkSize)
    //     _blkSize = _fileSize;

    // _numBlks = _fileSize / _blkSize;
    // if (_fileSize % _blkSize != 0)
    //     _numBlks++;

    // bool prev = false;
    // if (_active.compare_exchange_strong(prev, true)) {
    //     FileCacheRegister *reg = FileCacheRegister::openFileCacheRegister();
    //     if (reg) {
    //         _regFileIndex = reg->registerFile(_name);
    //         // _cache->addFile(_regFileIndex, _name, _blkSize, _fileSize);                
    //     }
    // }
    // #endif
}

void TrackFile::close() {
  DPRINTF("Calling TrackFile close \n");
  // if (!_closed) {
  unixclose_t unixClose = (unixclose_t)dlsym(RTLD_NEXT, "close");
  auto close_success = (*unixClose)(_fd_orig);
  if (close_success) {
    // _closed = true;
    DPRINTF("Closed file with fd %d with name %s successfully\n", _fd_orig, _name.c_str());
  }
    // }
   // write blk access stat in a file
  // DPRINTF("Writing r blk access stat\n");
  //   std::fstream current_file_stat_r;
  //   std::string file_name_r = _filename;
  //   auto file_stat_r = file_name_r.append("_r_stat");
  //   current_file_stat_r.open(file_stat_r, std::ios::out);
  //   if (!current_file_stat_r) { DPRINTF("File for read stat collection not created!");}
  //   current_file_stat_r << _filename << " " << "Block no." << " " << "Frequency" << std::endl;
  //   for (auto& blk_info  : track_file_blk_r_stat[_name]) {
  //     current_file_stat_r << blk_info.first << " " << blk_info.second << std::endl;
  //   }

  //   DPRINTF("Writing w blk access stat\n");
  //   // write blk access stat in a file
  //   std::fstream current_file_stat_w;
  //   std::string file_name_w = _filename;
  //   auto file_stat_w = file_name_w.append("_w_stat");
  //   current_file_stat_w.open(file_stat_w, std::ios::out);
  //   if (!current_file_stat_w) {DPRINTF("File for read stat collection not created!");}
  //   current_file_stat_w << _filename << " " << "Block no." << " " << "Frequency" << std::endl;
  //   for (auto& blk_info  : track_file_blk_w_stat[_name]) {
  //     current_file_stat_w << blk_info.first << " " << blk_info.second << std::endl;
  //   }

}


ssize_t TrackFile::read(void *buf, size_t count, uint32_t index) {
  std::cout << "In trackfile read\n";

  // #if 0
  if (_active.load() && _numBlks) {
    if (_filePos[index] >= _fileSize) {
      // std::cerr << "[TAZER]" << _name << " " << _filePos[index] << " " << _fileSize << " " << count << std::endl;
      _eof[index] = true;
      return 0;
    }
        
    char *localPtr = (char *)buf;
    if ((uint64_t)count > _fileSize - _filePos[index])
      count = _fileSize - _filePos[index];

    uint32_t startBlock = _filePos[index] / _blkSize;
    uint32_t endBlock = ((_filePos[index] + count) / _blkSize);

    if (((_filePos[index] + count) % _blkSize))
      endBlock++;

    if (endBlock > _numBlks)
      endBlock = _numBlks;


    std::cout << "[TAZER] Read file" << Timer::printTime() << _name << " " << _filePos[index] << " " << _fileSize << " " << count << " " << startBlock << " " << endBlock << std::endl;
    auto blockSizeForStat = Config::blockSizeForStat;
    auto diff = _filePos[index] - _filePos[0];
    auto precNumBlocks = diff / blockSizeForStat;
    uint32_t startBlockForStat = precNumBlocks; 
    uint32_t endBlockForStat = (diff + count) / blockSizeForStat;

    if (((diff + count) % blockSizeForStat)) {
      endBlockForStat++;
    }

    // auto seek_success = unixlseek(_fd, index, SEEK_SET); // TODO: check
    unixread_t unixRead = (unixread_t)dlsym(RTLD_NEXT, "read");
    auto read_success = (*unixRead)(_fd_orig, buf, count);
    _filePos[index] += count;
    if (read_success) {
      DPRINTF("Successfully read the TrackFile\n");
      return count;
    }
  }
  return 0;
}
ssize_t TrackFile::write(const void *buf, size_t count, uint32_t index) {
  DPRINTF("In trackfile write\n");
#if 0
  uint32_t _blkSize = 1;
    
  _blkSize = Config::blockSizeForStat;

  DPRINTF("TrackFile write Filesize %ld ", _fileSize);
  DPRINTF(" blksize %ld ", _blkSize);   
  DPRINTF(" count %ld", count); 
  DPRINTF(" index %ld\n" , index) ; 

  auto diff = _filePos[index] - _filePos[0];
  auto precNumBlocks = diff / _blkSize;
  uint32_t startBlock = precNumBlocks; 
  uint32_t endBlock = (diff + count) / _blkSize; 

  // std::cout <<"[Tazer write]:"  << "startblock " << startBlock 
  // 	    << " endBlock " << endBlock << " Index " << index 
  // 	    << " count " << count << std::endl;

  if (((diff + count) % _blkSize)) {
    endBlock++;
  }

  for (auto i = startBlock; i <= endBlock; i++) {
    if (track_file_blk_w_stat[_name].find(i) == track_file_blk_w_stat[_name].end()) {
      track_file_blk_w_stat[_name].insert(std::make_pair(i, 1)); // not thread-safe
    }
    else {
      track_file_blk_w_stat[_name][i]++;
    }
  }
#endif

  unixwrite_t unixWrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");
  DPRINTF("About to write %u count to file with fd %d and file_name: %s", 
	  count, _fd_orig, _filename.c_str());
  auto write_success = (*unixWrite)(_fd_orig, buf, count);

#if 0  
  _filePos[index] += count;
  if (_filePos[index] > _fileSize)
    _fileSize = _filePos[index] + 1;
#endif
  if (write_success) {
    DPRINTF("Successfully wrote to the TrackFile\n");
    return count;
  }
  return 0;
}

uint64_t TrackFile::fileSize() {
        return _fileSize;
}

off_t TrackFile::seek(off_t offset, int whence, uint32_t index) {
    switch (whence) {
    case SEEK_SET:
        _filePos[index] = offset;
        break;
    case SEEK_CUR:
        _filePos[index] += offset;
        if (_filePos[index] > _fileSize) {
            _filePos[index] = _fileSize;
        }
        break;
    case SEEK_END:
        _filePos[index] = _fileSize + offset;
        break;
    }
    _eof[index] = false;
    return _filePos[index];
}
