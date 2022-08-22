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
#include "ConnectionPool.h"
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <linux/limits.h>
#include <mutex>
#include <sstream>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fnmatch.h>
#include <unordered_map>
#include <map>
#include <atomic>
#include <string>
#include <vector>
#include <unordered_set>
//#include "ErrorTester.h"
#include "InputFile.h"
#include "OutputFile.h"
#include "RSocketAdapter.h"
#include "Request.h"
#include "ReaderWriterLock.h"
#include "TazerFile.h"
#include "TazerFileDescriptor.h"
#include "TazerFileStream.h"
#include "Timer.h"
#include "Trackable.h"
#include "UnixIO.h"
#include "ThreadPool.h"
#include "PriorityThreadPool.h"


#define ADD_THROW __THROW

#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
// #define DPRINTF(...)
// #define MYPRINTF(...) fprintf(stderr, __VA_ARGS__)

#define TRACKFILECHANGES 1

static Timer* timer;

std::once_flag log_flag;
bool init = false;
ReaderWriterLock vLock;

std::unordered_set<std::string>* track_files = NULL; //this is a pointer cause we access in ((attribute)) constructor and initialization isnt guaranteed
static std::unordered_set<int> track_fd;
static std::unordered_set<FILE *> track_fp;
static std::unordered_set<int> ignore_fd;
static std::unordered_set<FILE *> ignore_fp;

// // The following map stores the filename, as well as the file descriptors, mode. We 
// // Can extend this easily to save more info as part of the tuple. 
// std::map<std::string, std::multimap<int, std::tuple<int>>> file_info;

// std::map<int, std::string> file_info; // to reverse-lookup filename from fd
// The following map stores the block information/ access freqency for each file
std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_r_stat;
std::map<std::string, std::map<int, std::atomic<int64_t> > > track_file_blk_w_stat;

unixopen_t unixopen = NULL;
unixopen_t unixopen64 = NULL;
unixclose_t unixclose = NULL;
unixread_t unixread = NULL;
unixwrite_t unixwrite = NULL;
unixlseek_t unixlseek = NULL;
unixlseek64_t unixlseek64 = NULL;
unixxstat_t unixxstat = NULL;
unixxstat64_t unixxstat64 = NULL;
unixxstat_t unixlxstat = NULL;
unixxstat64_t unixlxstat64 = NULL;
unixfsync_t unixfsync = NULL;
unixfopen_t unixfopen = NULL;
unixfopen_t unixfopen64 = NULL;
unixfclose_t unixfclose = NULL;
unixfread_t unixfread = NULL;
unixfwrite_t unixfwrite = NULL;
unixftell_t unixftell = NULL;
unixfseek_t unixfseek = NULL;
unixrewind_t unixrewind = NULL;
unixfgetc_t unixfgetc = NULL;
unixfgets_t unixfgets = NULL;
unixfputc_t unixfputc = NULL;
unixfputs_t unixfputs = NULL;
unixflockfile_t unixflockfile = NULL;
unixftrylockfile_t unixftrylockfile = NULL;
unixfunlockfile_t unixfunlockfile = NULL;
unixfflush_t unixfflush = NULL;
unixfeof_t unixfeof = NULL;
unixreadv_t unixreadv = NULL;
unixwritev_t unixwritev = NULL;

bool write_printf = false;
bool open_printf = false;

int removeStr(char *s, const char *r);

/*Templating*************************************************************************************************/

inline bool checkMeta(const char *pathname, std::string &path, std::string &file, TazerFile::Type &type) {

#ifdef TRACKFILECHANGES
  char pattern[] = "*.h5";
  auto ret_val = fnmatch(pattern, pathname, 0);
  if (ret_val == 0) {
    // DPRINTF("Filename matched with fnmatch %s\n", pathname);
    std::string filename(pathname);
    DPRINTF("Will be calling HDF5 branch for file %s\n", pathname);
    type = TazerFile::TrackLocal;
    file = filename;
    path = filename;
    return true;// tazerFun(filename, filename, type, args...);
  }
#endif

  DPRINTF("Checkmeta calling open on file %s\n", pathname);
    int fd = (*unixopen)(pathname, O_RDONLY);

    if(fd >= 0)
    {
        const std::string tazerVersion("TAZER0.1");
        std::string types[3] = {"input", "output", "local"};
        TazerFile::Type tokType[3] = {TazerFile::Input, TazerFile::Output, TazerFile::Local};
        int bufferSize = (tazerVersion.length() + 13); //need space for tazerVersion + \n + type= + (the type) + \0
        char *meta = new char[bufferSize+1];

        int ret = (*unixread)(fd, (void *)meta, bufferSize);
	// MYPRINTF("Will be calling close on file %s\n", pathname);
        (*unixclose)(fd);
        if (ret <= 0) {
            delete[] meta;
            return false;
        }
        meta[bufferSize] = '\0';

        std::stringstream ss(meta);
        std::string curLine;

        std::getline(ss, curLine);
        if(curLine.compare(0, tazerVersion.length(), tazerVersion) != 0) {
            delete[] meta;
	    return false;
        }

        std::getline(ss, curLine);
        if(curLine.compare(0, 5, "type=") != 0) {
            delete[] meta;
            return false;
        }

        std::string typeStr = curLine.substr(5, curLine.length() - 5);
        for(int i = 0; i < 3; i++) {
            if(typeStr.compare(types[i]) == 0) {
                path = pathname;
                file = pathname;
                type = tokType[i];
                // DPRINTF("Path: %s File: %s\n", path.c_str(), file.c_str());
                delete[] meta;
                return true;
            }
        }
        delete[] meta;
    }
    return false;
}

inline bool trackFile(int fd) { return init ? track_fd.count(fd) : false; }
inline bool trackFile(FILE *fp) { return init ? track_fp.count(fp) : false; }
inline bool trackFile(const char *name) { return init ? track_files->count(name) : false; }

inline bool ignoreFile(uint64_t fd) { return init ? ignore_fd.count(fd) : false; }
inline bool ignoreFile(FILE *fp) { return init ? ignore_fp.count(fp) : false; }
inline bool ignoreFile(std::string pathname) {
    if (init) {
        if (pathname.find(Config::filelockCacheFilePath) != std::string::npos) {
            return true;
        }
        if (pathname.find(Config::fileCacheFilePath) != std::string::npos) {
            return true;
        }
        if (pathname.find(Config::burstBufferCacheFilePath) != std::string::npos) {
            return true;
        }
    }
    return false;
}

template <typename T>
inline void removeFileStream(T posixFun, FILE *fp) {}
inline void removeFileStream(unixfclose_t posixFun, FILE *fp) {
    if (posixFun == unixfclose)
        TazerFileStream::removeStream(fp);
}

template <typename Func, typename FuncLocal, typename... Args>
inline auto innerWrapper(int fd, bool &isTazerFile, Func tazerFun, FuncLocal localFun, Args... args) {
  // DPRINTF("[Tazer] in innerwrapper 3\n");

    TazerFile *file = NULL;
    unsigned int fp = 0;

    // DPRINTF("[Tazer] in innerwrapper 3 init val: %d , fd val %d \n", init, fd);

    
    if (init && TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, fp)) {
      // DPRINTF("Found a file with fd %d\n", fd);
        isTazerFile = true;
	DPRINTF("calling Tazer function\n");	
        return tazerFun(file, fp, args...);
    }
    // else if (not internal write) {
    // do track
    
    //}
    // DPRINTF("[Tazer] in innerwrapper 3 for write calling localfun\n");

    return localFun(args...);
}

template <typename Func, typename FuncLocal, typename... Args>
inline auto innerWrapper(FILE *fp, bool &isTazerFile, Func tazerFun, FuncLocal localFun, Args... args) {
  // if (write_printf == true) {
  //   DPRINTF("[Tazer] in innerwrapper 2 for write\n");
  // }

  if (init) {
        ReaderWriterLock *lock = NULL;
        int fd = TazerFileStream::lookupStream(fp, lock);
        if (fd != -1) {
            isTazerFile = true;
            lock->writerLock();
            TazerFile *file = NULL;
            unsigned int pos = 0;
            TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
	    // if (write_printf == true) {
	    //   DPRINTF("[Tazer] in innerwrapper 2 for write calling Tazerfun\n");
	    // }
            auto ret = tazerFun(file, pos, fd, args...);
            lock->writerUnlock();
            removeFileStream(localFun, fp);
            return ret;
        }
    }
  return localFun(args...);
}

template <typename Func, typename FuncPosix, typename... Args>
inline auto innerWrapper(const char *pathname, bool &isTazerFile, Func tazerFun, FuncPosix posixFun, Args... args) {
  std::string path;
  std::string file;
  TazerFile::Type type;
  
  std::string test_tty(pathname);
  if(test_tty.find("tty") != std::string::npos) {
    return posixFun(args...);
  }
  if (init && checkMeta(pathname, path, file, type)) {
    isTazerFile = true;
    // DPRINTF("tazerfun With file %s\n", pathname);
    return tazerFun(file, path, type, args...);
  }
  // DPRINTF("[Tazer] in innerwrapper calling posix\n");
  

  return posixFun(args...);
}

template <typename T1, typename T2>
inline void addToSet(std::unordered_set<int> &set, T1 value, T2 posixFun) {}
inline void addToSet(std::unordered_set<int> &set, int value, unixopen_t posixFun) {
    if (posixFun == unixopen || posixFun == unixopen64)
        set.emplace(value);
}
inline void addToSet(std::unordered_set<FILE *> &set, FILE *value, unixfopen_t posixFun) {
    if (posixFun == unixfopen)
        set.emplace(value);
}

template <typename T1, typename T2>
inline void removeFromSet(std::unordered_set<int> &set, T1 value, T2 posixFun) {}
inline void removeFromSet(std::unordered_set<int> &set, int value, unixclose_t posixFun) {
    if (posixFun == unixclose)
        set.erase(value);
}
inline void removeFromSet(std::unordered_set<FILE *> &set, FILE *value, unixfclose_t posixFun) {
    if (posixFun == unixfclose)
        set.erase(value);
}

template <typename FileId, typename Func, typename FuncPosix, typename... Args>
auto outerWrapper(const char *name, FileId fileId, Timer::Metric metric, Func tazerFun, FuncPosix posixFun, Args... args) {
  // DPRINTF("command %s\n", name);

  if (!init) {
    // if (strcmp(name, "write") == 0) {
    //   DPRINTF("In init for write calling in posix\n");
    // }

      posixFun = (FuncPosix)dlsym(RTLD_NEXT, name);
      return posixFun(args...);
    }

    timer->start();

    //Check if this is a special file to track (from environment variable)
    bool track = trackFile(fileId);

    //Check for files internal to tazer
    bool ignore = ignoreFile(fileId);

    //Check if a tazer meta-file
    bool isTazerFile = false;

    //Do the work
    // if (strcmp(name, "write") == 0) {
    //   DPRINTF("Before calling innerwrapper for write\n");
    // }

    auto retValue = innerWrapper(fileId, isTazerFile, tazerFun, posixFun, args...);
    if (ignore) {
        //Maintain the ignore_fd set
        addToSet(ignore_fd, retValue, posixFun);
        removeFromSet(ignore_fd, retValue, posixFun);
        timer->end(Timer::MetricType::local, Timer::Metric::dummy); //to offset the call to start()
    }
    else { //End Timers!
        if (track) {
            //Maintain the track_fd set
            addToSet(track_fd, retValue, posixFun);
            removeFromSet(track_fd, retValue, posixFun);
            if (std::string("read").compare(std::string(name)) == 0 ||
            std::string("write").compare(std::string(name)) == 0){
                ssize_t ret = *reinterpret_cast<ssize_t*> (&retValue);
                if (ret != -1) {
                    timer->addAmt(Timer::MetricType::local, metric, ret);
                }
            }
            timer->end(Timer::MetricType::local, metric);
        }
        else if (isTazerFile){
            timer->end(Timer::MetricType::tazer, metric);
        }
        else{
            if (std::string("read").compare(std::string(name)) == 0 ||
            std::string("write").compare(std::string(name)) == 0){
                ssize_t ret = *reinterpret_cast<ssize_t*> (&retValue);
                if (ret != -1) {
                    timer->addAmt(Timer::MetricType::system, metric, ret);
                }
            }
            timer->end(Timer::MetricType::system, metric);
        }
    }
    return retValue;
}

/*Posix******************************************************************************************************/

int tazerOpen(std::string name, std::string metaName, TazerFile::Type type, const char *pathname, int flags, int mode);
int open(const char *pathname, int flags, ...);
int open64(const char *pathname, int flags, ...);

int tazerClose(TazerFile *file, unsigned int fp, int fd);
int close(int fd);

ssize_t tazerRead(TazerFile *file, unsigned int fp, int fd, void *buf, size_t count);
ssize_t read(int fd, void *buf, size_t count);

ssize_t tazerWrite(TazerFile *file, unsigned int fp, int fd, const void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

template <typename T>
T tazerLseek(TazerFile *file, unsigned int fp, int fd, T offset, int whence);
off_t lseek(int fd, off_t offset, int whence) ADD_THROW;
off64_t lseek64(int fd, off64_t offset, int whence) ADD_THROW;

int innerStat(int version, const char *filename, struct stat *buf);
int innerStat(int version, const char *filename, struct stat64 *buf);
template <typename T>
int tazerStat(std::string name, std::string metaName, TazerFile::Type type, int version, const char *filename, T *buf);
int __xstat(int version, const char *filename, struct stat *buf) ADD_THROW;
int __xstat64(int version, const char *filename, struct stat64 *buf) ADD_THROW;
int __lxstat(int version, const char *filename, struct stat *buf) ADD_THROW;
int __lxstat64(int version, const char *filename, struct stat64 *buf) ADD_THROW;

int tazerFsync(TazerFile *file, unsigned int fp, int fd);
int fsync(int fd);

template <typename Func, typename FuncLocal>
ssize_t tazerVector(const char *name, Timer::Metric metric, Func tazerFun, FuncLocal localFun, int fd, const struct iovec *iov, int iovcnt);
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
/*Streaming**************************************************************************************************/

FILE *tazerFopen(std::string name, std::string metaName, TazerFile::Type type, const char *__restrict fileName, const char *__restrict modes);
FILE *fopen(const char *__restrict fileName, const char *__restrict modes);
FILE *fopen64(const char *__restrict fileName, const char *__restrict modes);

int tazerFclose(TazerFile *file, unsigned int pos, int fd, FILE *fp);
int fclose(FILE *fp);

size_t tazerFread(TazerFile *file, unsigned int pos, int fd, void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp);
size_t fread(void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp);

size_t tazerFwrite(TazerFile *file, unsigned int pos, int fd, const void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp);
size_t fwrite(const void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp);

long int tazerFtell(TazerFile *file, unsigned int pos, int fd, FILE *fp);
long int ftell(FILE *fp);

int tazerFseek(TazerFile *file, unsigned int pos, int fd, FILE *fp, long int off, int whence);
int fseek(FILE *fp, long int off, int whence);

int tazerFgetc(TazerFile *file, unsigned int pos, int fd, FILE *fp);
int fgetc(FILE *fp);

char *tazerFgets(TazerFile *file, unsigned int pos, int fd, char *__restrict s, int n, FILE *__restrict fp);
char *fgets(char *__restrict s, int n, FILE *__restrict fp);

int tazerFputc(TazerFile *file, unsigned int pos, int fd, int c, FILE *fp);
int fputc(int c, FILE *fp);

int tazerFputs(TazerFile *file, unsigned int pos, int fd, const char *__restrict s, FILE *__restrict fp);
int fputs(const char *__restrict s, FILE *__restrict fp);

int tazerFeof(TazerFile *file, unsigned int pos, int fd, FILE *fp);
int feof(FILE *fp) ADD_THROW;

off_t tazerRewind(TazerFile *file, unsigned int pos, int fd, int fd2, off_t offset, int whence);
void rewind(FILE *fp);
