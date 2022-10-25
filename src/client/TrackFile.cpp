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
#include <tuple>
#include <sys/stat.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <cassert>
#include <functional>
#include <chrono>

using namespace std::chrono;
// #define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

#define GATHERSTAT 1
// #define USE_HASH 1
#define ENABLE_TRACE 1

TrackFile::TrackFile(std::string name, int fd, bool openFile) : 
  TazerFile(TazerFile::Type::TrackLocal, name, name, fd),
  _fileSize(0),
  _numBlks(0),
  _fd_orig(fd),
  _filename(name),
  total_time_spent_read(0),
  total_time_spent_write(0)
{ 
  DPRINTF("In Trackfile constructor openfile bool: %d\n", openFile);
  _blkSize = Config::blockSizeForStat;
  open();
  _active.store(true);
}

TrackFile::~TrackFile() {
    *this << "Destroying file " << _metaName << std::endl;
    // close();
}

void TrackFile::open() {
  DPRINTF("[TAZER] TrackFile open: %s\n", _name.c_str()) ;
  // #if 0
  // _closed = false;
  if (track_file_blk_r_stat.find(_name) == track_file_blk_r_stat.end()) {
    track_file_blk_r_stat.insert(std::make_pair(_name, 
						std::map<int, 
						std::atomic<int64_t> >()));
  }
  if (track_file_blk_r_stat_size.find(_name) == track_file_blk_r_stat_size.end()) {
    track_file_blk_r_stat_size.insert(std::make_pair(_name, 
						std::map<int, 
						std::atomic<int64_t> >()));
  }
  if (track_file_blk_w_stat.find(_name) == track_file_blk_w_stat.end()) {
    track_file_blk_w_stat.insert(std::make_pair(_name, 
						std::map<int, 
						std::atomic<int64_t> >()));
  }

  if (track_file_blk_w_stat_size.find(_name) == track_file_blk_w_stat_size.end()) {
    track_file_blk_w_stat_size.insert(std::make_pair(_name, 
						std::map<int, 
						std::atomic<int64_t> >()));
  }


  if (trace_read_blk_seq.find(_name) == trace_read_blk_seq.end()) {
    trace_read_blk_seq.insert(std::make_pair(_name, std::vector<int>()));
  }

  if (trace_write_blk_seq.find(_name) == trace_write_blk_seq.end()) {
    trace_write_blk_seq.insert(std::make_pair(_name, std::vector<int>()));
  }

  open_file_start_time = high_resolution_clock::now();


  // #endif
  DPRINTF("Returning from trackfile open\n");

}

ssize_t TrackFile::read(void *buf, size_t count, uint32_t index) {
  DPRINTF("In trackfile read count %u \n", count);
  auto read_file_start_time = high_resolution_clock::now();
  unixread_t unixRead = (unixread_t)dlsym(RTLD_NEXT, "read");
  auto bytes_read = (*unixRead)(_fd_orig, buf, count);
  auto read_file_end_time = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(read_file_end_time - read_file_start_time); 
  total_time_spent_read += duration;
#ifdef GATHERSTAT
  if (bytes_read != -1) { // Only update stats if nonzero byte counts are read
    auto blockSizeForStat = Config::blockSizeForStat;
    auto diff = _filePos[index]; //- _filePos[0]; // technically index is always equal to 0 for us, assuming there is only one fp for a file open at a time.
    auto precNumBlocks = diff / blockSizeForStat;
    uint32_t startBlockForStat = precNumBlocks; 
    uint32_t endBlockForStat = (diff + bytes_read) / blockSizeForStat;
  
    if (((diff + bytes_read) % blockSizeForStat)) {
      endBlockForStat++;
    }

    for (auto i = startBlockForStat; i <= endBlockForStat; i++) {
      auto index = i;
#ifdef USE_HASH
      auto sample = hashed(index) % Config::hashtableSizeForStat;
      if (sample < Config::hashtableSizeForStat/2) { // Sample only 50%
	// index = sample;
#endif      
	if (track_file_blk_r_stat[_name].find(index) == 
	    track_file_blk_r_stat[_name].end()) {
	  track_file_blk_r_stat[_name].insert(std::make_pair(index, 1));
	//track_file_blk_r_stat_size[_name].insert(std::make_pair(i, bytes_read - ((i - startBlockForStat) * blockSizeForStat)));
	  track_file_blk_r_stat_size[_name].insert(std::make_pair(index, bytes_read));
	} else {
	  track_file_blk_r_stat[_name][index]++;
	}
	// For tracing order
	trace_read_blk_seq[_name].push_back(index);

#ifdef USE_HASH    
      } else {
	continue;
      }
#endif
    }
  }
#endif
  if (bytes_read != -1) {
    DPRINTF("Successfully read the TrackFile\n");
    _filePos[index] += bytes_read;
  }
  return bytes_read;
  //  return 0;
}

ssize_t TrackFile::write(const void *buf, size_t count, uint32_t index) {
  DPRINTF("In trackfile write count %u \n", count);
  auto write_file_start_time = high_resolution_clock::now();
  unixwrite_t unixWrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");
  DPRINTF("About to write %u count to file with fd %d and file_name: %s\n", 
	  count, _fd_orig, _filename.c_str());
  auto bytes_written = (*unixWrite)(_fd_orig, buf, count);
  auto write_file_end_time = high_resolution_clock::now();
  auto duration = duration_cast<seconds>(write_file_end_time - write_file_start_time);
  total_time_spent_write += duration;
#ifdef GATHERSTAT
  if (bytes_written != -1) {
    auto diff = _filePos[index]; //  - _filePos[0];
    auto precNumBlocks = diff / _blkSize;
    uint32_t startBlockForStat = precNumBlocks; 
    uint32_t endBlockForStat = (diff + bytes_written) / _blkSize; 
    if (((diff + bytes_written) % _blkSize)) {
      endBlockForStat++;
    }

    for (auto i = startBlockForStat; i <= endBlockForStat; i++) {
      auto index = i;
#ifdef USE_HASH
      auto sample = hashed(index) % Config::hashtableSizeForStat;
      if (sample < Config::hashtableSizeForStat/2) { // Sample only 50%
	// index = sample;
#endif      
	if (track_file_blk_w_stat[_name].find(index) == track_file_blk_w_stat[_name].end()) {
	  track_file_blk_w_stat[_name].insert(std::make_pair(index, 1)); // not thread-safe
	track_file_blk_w_stat_size[_name].insert(std::make_pair(i, bytes_written)); // not thread-safe
        }
        else {
	  track_file_blk_w_stat[_name][index]++;
        }

	trace_write_blk_seq[_name].push_back(index);

#ifdef USE_HASH    
      } else {
	continue;
      }
#endif
    }
  }
#endif
  if (bytes_written != -1) {
    DPRINTF("Successfully wrote to the TrackFile\n");
    _filePos[index] += bytes_written;
    // _fileSize += bytes_written;  
  }
  return bytes_written;
}

uint64_t TrackFile::fileSize() {
        return _fileSize;
}

off_t TrackFile::seek(off_t offset, int whence, uint32_t index) {
  struct stat sb;
  fstat(_fd_orig, &sb);
  auto _fileSize = sb.st_size;

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
  // _eof[index] = false;


  DPRINTF("Calling Seek in Trackfile\n");
  unixlseek_t unixLseek = (unixlseek_t)dlsym(RTLD_NEXT, "lseek");
  auto offset_loc = (*unixLseek)(_fd_orig, offset, whence);
  return  offset_loc; 
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
  close_file_end_time = high_resolution_clock::now();
  auto elapsed_time = duration_cast<seconds>(close_file_end_time - open_file_start_time);
   // write blk access stat in a file
  DPRINTF("Writing r blk access stat\n");
  std::fstream current_file_stat_r;
  std::string file_name_r = _filename;
  auto file_stat_r = file_name_r.append("_r_stat");
  current_file_stat_r.open(file_stat_r, std::ios::out);
  if (!current_file_stat_r) { DPRINTF("File for read stat collection not created!");}
  current_file_stat_r << _filename << " " << "Block no." << " " << "Frequency" << " " 
		      << "Access size in byte" << std::endl;

  auto sum_weight_r = 0;
  auto cumulative_weighted_sum_r = 0;
  for (auto& blk_info  : track_file_blk_r_stat[_name]) {
    cumulative_weighted_sum_r += blk_info.second * track_file_blk_r_stat_size[_name][blk_info.first];
    sum_weight_r += blk_info.second;
    current_file_stat_r << blk_info.first << " " << blk_info.second << " " 
			<< track_file_blk_r_stat_size[_name][blk_info.first] << std::endl;
  }

  auto read_io_rate = cumulative_weighted_sum_r / sum_weight_r; // per byte
  auto read_request_rate = elapsed_time / sum_weight_r;

  DPRINTF("Writing w blk access stat\n");
  // write blk access stat in a file
  std::fstream current_file_stat_w;
  std::string file_name_w = _filename;
  auto file_stat_w = file_name_w.append("_w_stat");
  current_file_stat_w.open(file_stat_w, std::ios::out);
  if (!current_file_stat_w) {DPRINTF("File for write stat collection not created!");}
  current_file_stat_w << _filename << " " << "Block no." << " " << "Frequency" << " " 
		      << "Access size in byte" << std::endl;
  
  auto sum_weight_w = 0;
  auto cumulative_weighted_sum_w = 0;
  for (auto& blk_info  : track_file_blk_w_stat[_name]) {
    cumulative_weighted_sum_w += blk_info.second * track_file_blk_w_stat_size[_name][blk_info.first];
    sum_weight_w += blk_info.second;
    current_file_stat_w << blk_info.first << " " << blk_info.second << " " << track_file_blk_w_stat_size[_name][blk_info.first] << std::endl;
    //current_file_stat_w << std::get<0>(blk_info) << " " << std::get<1>(blk_info) << " " << std::get<2>(blk_info) << std::endl;
  }
  
  auto write_io_rate = cumulative_weighted_sum_w / sum_weight_w; 
  auto write_request_rate =  elapsed_time / sum_weight_w;
  auto io_intensity = (total_time_spent_read + total_time_spent_write)/elapsed_time;

  DPRINTF("Writing r blk access order stat\n");
  // write blk access stat in a file
  std::fstream current_file_trace_r;
  std::string file_name_trace_r = _filename;
  auto file_trace_stat_r = file_name_trace_r.append("_r_trace_stat");
  current_file_trace_r.open(file_trace_stat_r, std::ios::out);
  if (!current_file_trace_r) {DPRINTF("File for read trace stat collection not created!");}
  auto const& blk_trace_info_r  = trace_read_blk_seq[_name]; 
  for (auto const& blk_: blk_trace_info_r) {
    current_file_trace_r << blk_ << std::endl;
  }
  

  DPRINTF("Writing w blk access order stat\n");
  // write blk access stat in a file
  std::fstream current_file_trace_w;
  std::string file_name_trace_w = _filename;
  auto file_trace_stat_w = file_name_trace_w.append("_w_trace_stat");
  current_file_trace_w.open(file_trace_stat_w, std::ios::out);
  if (!current_file_trace_w) {DPRINTF("File for write trace stat collection not created!");}
  // current_file_trace_w << _filename << " " << "Block no." << " " << "Frequency" << std::endl;
  auto const& blk_trace_info_w = trace_write_blk_seq[_name];
  for (auto const& blk_: blk_trace_info_w) {
    current_file_trace_w << blk_ << std::endl;
  }
}
