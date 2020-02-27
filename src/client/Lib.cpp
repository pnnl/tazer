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
#include <mutex>
#include <sstream>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
//#include "ErrorTester.h"
#include "InputFile.h"
#include "RSocketAdapter.h"
#include "ReaderWriterLock.h"
#include "TazerFile.h"
#include "TazerFileDescriptor.h"
#include "TazerFileStream.h"
#include "Timer.h"
#include "Trackable.h"
#include "UnixIO.h"

//#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

static Timer timer;

std::once_flag log_flag;
bool init = false;
ReaderWriterLock vLock;

static std::unordered_set<std::string> track_files;
static std::unordered_set<int> track_fd;
static std::unordered_set<FILE *> track_fp;
static std::unordered_set<int> ignore_fd;
static std::unordered_set<FILE *> ignore_fp;

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

void __attribute__((constructor)) tazerInit(void) {
    std::call_once(log_flag, []() {
        timer.start();
        Loggable::mtx_cout = new std::mutex();
        InputFile::_cache = new Cache(BASECACHENAME, CacheType::base);
        ConnectionPool::useCnt = new std::unordered_map<std::string, uint64_t>();
        ConnectionPool::consecCnt = new std::unordered_map<std::string, uint64_t>();
        ConnectionPool::stats = new std::unordered_map<std::string, std::pair<double, double>>();

        char *temp = getenv("TAZER_LOCAL_FILES");
        if (temp) {
            std::stringstream files(temp);
            while (!files.eof()) {
                std::string f;
                getline(files, f, ' ');
                track_files.emplace(f);
            }
        }

        unixopen = (unixopen_t)dlsym(RTLD_NEXT, "open");
        unixopen64 = (unixopen_t)dlsym(RTLD_NEXT, "open64");
        unixclose = (unixclose_t)dlsym(RTLD_NEXT, "close");
        unixread = (unixread_t)dlsym(RTLD_NEXT, "read");
        unixwrite = (unixwrite_t)dlsym(RTLD_NEXT, "write");
        unixlseek = (unixlseek_t)dlsym(RTLD_NEXT, "lseek");
        unixlseek64 = (unixlseek64_t)dlsym(RTLD_NEXT, "lseek64");
        unixxstat = (unixxstat_t)dlsym(RTLD_NEXT, "__xstat");
        unixxstat64 = (unixxstat64_t)dlsym(RTLD_NEXT, "__xstat64");
        unixlxstat = (unixxstat_t)dlsym(RTLD_NEXT, "__lxstat");
        unixlxstat64 = (unixxstat64_t)dlsym(RTLD_NEXT, "__lxstat64");
        unixfsync = (unixfsync_t)dlsym(RTLD_NEXT, "fsync");
        unixfopen = (unixfopen_t)dlsym(RTLD_NEXT, "fopen");
        unixfopen64 = (unixfopen_t)dlsym(RTLD_NEXT, "fopen64");
        unixfclose = (unixfclose_t)dlsym(RTLD_NEXT, "fclose");
        unixfread = (unixfread_t)dlsym(RTLD_NEXT, "fread");
        unixfwrite = (unixfwrite_t)dlsym(RTLD_NEXT, "fwrite");
        unixftell = (unixftell_t)dlsym(RTLD_NEXT, "ftell");
        unixfseek = (unixfseek_t)dlsym(RTLD_NEXT, "fseek");
        unixrewind = (unixrewind_t)dlsym(RTLD_NEXT, "rewind");
        unixfgetc = (unixfgetc_t)dlsym(RTLD_NEXT, "fgetc");
        unixfgets = (unixfgets_t)dlsym(RTLD_NEXT, "fgets");
        unixfputc = (unixfputc_t)dlsym(RTLD_NEXT, "fputs");
        unixfputs = (unixfputs_t)dlsym(RTLD_NEXT, "fputs");
        unixflockfile = (unixflockfile_t)dlsym(RTLD_NEXT, "flockfile");
        unixftrylockfile = (unixftrylockfile_t)dlsym(RTLD_NEXT, "ftrylockfile");
        unixfunlockfile = (unixfunlockfile_t)dlsym(RTLD_NEXT, "funlockfile");
        unixfflush = (unixfflush_t)dlsym(RTLD_NEXT, "fflush");
        unixfeof = (unixfeof_t)dlsym(RTLD_NEXT, "feof");
        unixreadv = (unixreadv_t)dlsym(RTLD_NEXT, "readv");
        unixwritev = (unixwritev_t)dlsym(RTLD_NEXT, "writev");

        //unsetenv("LD_PRELOAD"); //uncomment me if running into issues with an application that launches child shells
        timer.end(Timer::MetricType::tazer, Timer::Metric::constructor);
    });
    init = true;
}

void __attribute__((destructor)) tazerCleanup(void) {
    timer.start();
    init = false; //set to false because we cant ensure our static members have not already been deleted.

    if (Config::printStats) {
        std::cout << "[TAZER] "
                  << "Exiting Client" << std::endl;

        if (ConnectionPool::useCnt->size() > 0) {
            for (auto conUse : *ConnectionPool::useCnt) {
                //if (conUse.second > 1) {
                std::cout << "[TAZER] connection: " << conUse.first << " num_tx: " << conUse.second << " amount: " << (*ConnectionPool::stats)[conUse.first].first << " B time: " << (*ConnectionPool::stats)[conUse.first].second << " s avg BW: " << ((*ConnectionPool::stats)[conUse.first].first / (*ConnectionPool::stats)[conUse.first].second) / 1000000 << "MB/s" << std::endl;
                //}
            }
        }
    }

    timer.end(Timer::MetricType::tazer, Timer::Metric::destructor);
    delete InputFile::_cache; //desturctor time tracked by each cache...
    timer.start();
    FileCacheRegister::closeFileCacheRegister();
    ConnectionPool::removeAllConnectionPools();
    Connection::closeAllConnections();
    timer.end(Timer::MetricType::tazer, Timer::Metric::destructor);
}

int removeStr(char *s, const char *r) {
    char *ptr = strstr(s, r);
    if (ptr == NULL)
        return -1;
    strcpy(ptr, ptr + strlen(r));
    return 0;
}

/*Templating*************************************************************************************************/

inline bool splitter(std::string tok, std::string full, std::string &path, std::string &file) {
    size_t pos = full.find(tok);
    if (pos != std::string::npos) {
        path = full.substr(0, pos + tok.length());
        if (path.length() < full.length())
            file = full.substr(path.length() + 1);
        else
            file = full;
        return true;
    }
    path = full;
    file = full;
    return false;
}

inline bool checkMeta(const char *pathname, std::string &path, std::string &file, TazerFile::Type &type) {
    if (strstr(pathname, ".meta.")) {
        std::string full(pathname);
        TazerFile::Type tokType[3] = {TazerFile::Input, TazerFile::Output, TazerFile::Local};
        std::string tok[3] = {".meta.in", ".meta.out", ".meta.local"};
        for (unsigned int i = 0; i < 3; i++) {
            if (splitter(tok[i], full, path, file)) {
                type = tokType[i];
                DPRINTF("Path: %s File: %s\n", path.c_str(), file.c_str());
                return true;
            }
        }
    }
    DPRINTF("~ %s Path: %s File: %s\n", pathname, path.c_str(), file.c_str());
    return false;
}

inline bool trackFile(int fd) { return init ? track_fd.count(fd) : false; }
inline bool trackFile(FILE *fp) { return init ? track_fp.count(fp) : false; }
inline bool trackFile(const char *name) { return init ? track_files.count(name) : false; }

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
    TazerFile *file = NULL;
    unsigned int fp = 0;
    if (init && TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, fp)) {
        isTazerFile = true;

        return tazerFun(file, fp, args...);
    }
    return localFun(args...);
}

template <typename Func, typename FuncLocal, typename... Args>
inline auto innerWrapper(FILE *fp, bool &isTazerFile, Func tazerFun, FuncLocal localFun, Args... args) {
    if (init) {
        ReaderWriterLock *lock = NULL;
        int fd = TazerFileStream::lookupStream(fp, lock);
        if (fd != -1) {
            isTazerFile = true;
            lock->writerLock();
            TazerFile *file = NULL;
            unsigned int pos = 0;
            TazerFileDescriptor::lookupTazerFileDescriptor(fd, file, pos);
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
    if (init && checkMeta(pathname, path, file, type)) {
        isTazerFile = true;
        return tazerFun(file, path, type, args...);
    }
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
    if (!init) {
        posixFun = (FuncPosix)dlsym(RTLD_NEXT, name);
        return posixFun(args...);
    }

    timer.start();

    //Check if this is a special file to track (from environment variable)
    bool track = trackFile(fileId);

    //Check for files internal to tazer
    bool ignore = ignoreFile(fileId);

    //Check if a tazer meta-file
    bool isTazerFile = false;

    //Do the work
    auto retValue = innerWrapper(fileId, isTazerFile, tazerFun, posixFun, args...);

    if (ignore) {
        //Maintain the ignore_fd set
        addToSet(ignore_fd, retValue, posixFun);
        removeFromSet(ignore_fd, retValue, posixFun);
        timer.end(Timer::MetricType::local, Timer::Metric::dummy); //to offset the call to start()
    }
    else { //End Timers!
        if (track) {
            //Maintain the track_fd set
            addToSet(track_fd, retValue, posixFun);
            removeFromSet(track_fd, retValue, posixFun);
            timer.end(Timer::MetricType::local, metric);
        }
        else if (isTazerFile)
            timer.end(Timer::MetricType::tazer, metric);
        else
            timer.end(Timer::MetricType::system, metric);
    }
    return retValue;
}

/*Posix******************************************************************************************************/

int tazerOpen(std::string name, std::string metaName, TazerFile::Type type, const char *pathname, int flags, int mode) {
    DPRINTF("tazerOpen: %s %s %u\n", name.c_str(), metaName.c_str(), type);
    int fd = (*unixopen64)(metaName.c_str(), O_RDONLY, 0);
    TazerFile *file = TazerFile::addNewTazerFile(type, name, metaName, fd);
    if (file)
        TazerFileDescriptor::addTazerFileDescriptor(fd, file, file->newFilePosIndex());
    return fd;
}

int open(const char *pathname, int flags, ...) {
    int mode = 0;
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);

    Timer::Metric metric = (flags & O_WRONLY || flags & O_RDWR) ? Timer::Metric::out_open : Timer::Metric::in_open;
    return outerWrapper("open", pathname, metric, tazerOpen, unixopen, pathname, flags, mode);
}

int open64(const char *pathname, int flags, ...) {
    int mode = 0;
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, int);
    va_end(arg);

    Timer::Metric metric = (flags & O_WRONLY || flags & O_RDWR) ? Timer::Metric::out_open : Timer::Metric::in_open;
    return outerWrapper("open64", pathname, metric, tazerOpen, unixopen64, pathname, flags, mode);
}

int tazerClose(TazerFile *file, unsigned int fp, int fd) {
    TazerFile::removeTazerFile(file);
    TazerFileDescriptor::removeTazerFileDescriptor(fd);
    return (*unixclose)(fd);
}

int close(int fd) {
    return outerWrapper("close", fd, Timer::Metric::close, tazerClose, unixclose, fd);
}

ssize_t tazerRead(TazerFile *file, unsigned int fp, int fd, void *buf, size_t count) {
    ssize_t ret = file->read(buf, count, fp);
    timer.addAmt(Timer::MetricType::tazer, Timer::Metric::read, ret);
    return ret;
}

ssize_t read(int fd, void *buf, size_t count) {
    vLock.readerLock();
    auto ret = outerWrapper("read", fd, Timer::Metric::read, tazerRead, unixread, fd, buf, count);
    vLock.readerUnlock();
    return ret;
}

ssize_t tazerWrite(TazerFile *file, unsigned int fp, int fd, const void *buf, size_t count) {
    auto ret = file->write(buf, count, fp);
    timer.addAmt(Timer::MetricType::tazer, Timer::Metric::write, ret);
    return ret;
}

ssize_t write(int fd, const void *buf, size_t count) {
    vLock.readerLock();
    auto ret = outerWrapper("write", fd, Timer::Metric::write, tazerWrite, unixwrite, fd, buf, count);
    vLock.readerUnlock();
    return ret;
}

template <typename T>
T tazerLseek(TazerFile *file, unsigned int fp, int fd, T offset, int whence) {
    return (T)file->seek(offset, whence, fp);
}

off_t lseek(int fd, off_t offset, int whence) {
    vLock.readerLock();
    auto ret = outerWrapper("lseek", fd, Timer::Metric::seek, tazerLseek<off_t>, unixlseek, fd, offset, whence);
    vLock.readerUnlock();
    return ret;
}

off64_t lseek64(int fd, off64_t offset, int whence) {
    vLock.readerLock();
    auto ret = outerWrapper("lseek64", fd, Timer::Metric::seek, tazerLseek<off64_t>, unixlseek64, fd, offset, whence);
    vLock.readerUnlock();
    return ret;
}

thread_local unixxstat_t whichStat = NULL;
int innerStat(int version, const char *filename, struct stat *buf) { return whichStat(version, filename, buf); }

thread_local unixxstat64_t whichStat64 = NULL;
int innerStat(int version, const char *filename, struct stat64 *buf) { return whichStat64(version, filename, buf); }

template <typename T>
int tazerStat(std::string name, std::string metaName, TazerFile::Type type, int version, const char *filename, T *buf) {
    auto ret = innerStat(_STAT_VER, metaName.c_str(), buf);
    TazerFile *file = TazerFile::lookUpTazerFile(filename);
    if (file)
        buf->st_size = (off_t)file->fileSize();
    else {
        int fd = (*unixopen)(metaName.c_str(), O_RDONLY, 0);
        InputFile tempFile(name, metaName, fd, false);
        buf->st_size = tempFile.fileSize();
        (*unixclose)(fd);
    }
    return ret;
}

int __xstat(int version, const char *filename, struct stat *buf) {
    whichStat = unixxstat;
    return outerWrapper("__xstat", filename, Timer::Metric::stat, tazerStat<struct stat>, unixxstat, version, filename, buf);
}

int __xstat64(int version, const char *filename, struct stat64 *buf) {
    whichStat64 = unixxstat64;
    return outerWrapper("__xstat64", filename, Timer::Metric::stat, tazerStat<struct stat64>, unixxstat64, version, filename, buf);
}

int __lxstat(int version, const char *filename, struct stat *buf) {
    whichStat = unixxstat;
    return outerWrapper("__lxstat", filename, Timer::Metric::stat, tazerStat<struct stat>, unixlxstat, version, filename, buf);
}

int __lxstat64(int version, const char *filename, struct stat64 *buf) {
    whichStat64 = unixlxstat64;
    return outerWrapper("__lxstat64", filename, Timer::Metric::stat, tazerStat<struct stat64>, unixlxstat64, version, filename, buf);
}

int tazerFsync(TazerFile *file, unsigned int fp, int fd) {
    return 0;
}

int fsync(int fd) {
    vLock.readerLock();
    auto ret = outerWrapper("fsync", fd, Timer::Metric::stat, tazerFsync, unixfsync, fd);
    vLock.readerUnlock();
    return ret;
}

template <typename Func, typename FuncLocal>
ssize_t tazerVector(const char *name, Timer::Metric metric, Func tazerFun, FuncLocal localFun, int fd, const struct iovec *iov, int iovcnt) {
    ssize_t ret = 0;
    vLock.writerLock();
    for (int i = 0; i < iovcnt && ret < iovcnt; i++) {
        auto temp = outerWrapper(name, fd, metric, tazerFun, localFun, fd, iov[i].iov_base, iov[i].iov_len);
        if (temp == (ssize_t)-1) {
            ret = -1;
            break;
        }
        else
            ret += temp;
    }
    vLock.writerUnlock();
    return ret;
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return tazerVector("read", Timer::Metric::readv, tazerRead, unixread, fd, iov, iovcnt);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return tazerVector("write", Timer::Metric::writev, tazerWrite, unixwrite, fd, iov, iovcnt);
}

/*Streaming**************************************************************************************************/

FILE *tazerFopen(std::string name, std::string metaName, TazerFile::Type type, const char *__restrict fileName, const char *__restrict modes) {
    DPRINTF("tazerOpen: %s %s %u\n", name.c_str(), metaName.c_str(), type);
    char m = 'r';
    FILE *fp = (*unixfopen)(fileName, &m);
    if (fp) {
        int fd = fileno(fp);
        TazerFile *file = TazerFile::addNewTazerFile(type, name, metaName, fd);
        if (file) {
            TazerFileDescriptor::addTazerFileDescriptor(fd, file, file->newFilePosIndex());
            TazerFileStream::addStream(fp, fd);
        }
    }
    return fp;
}

FILE *fopen(const char *__restrict fileName, const char *__restrict modes) {
    Timer::Metric metric = (modes[0] == 'r') ? Timer::Metric::in_fopen : Timer::Metric::out_fopen;
    return outerWrapper("fopen", fileName, metric, tazerFopen, unixfopen, fileName, modes);
}

FILE *fopen64(const char *__restrict fileName, const char *__restrict modes) {
    Timer::Metric metric = (modes[0] == 'r') ? Timer::Metric::in_fopen : Timer::Metric::out_fopen;
    return outerWrapper("fopen64", fileName, metric, tazerFopen, unixfopen64, fileName, modes);
}

int tazerFclose(TazerFile *file, unsigned int pos, int fd, FILE *fp) {
    TazerFile::removeTazerFile(file);
    TazerFileDescriptor::removeTazerFileDescriptor(fd);
    return (*unixfclose)(fp);
}

int fclose(FILE *fp) {
    return outerWrapper("fclose", fp, Timer::Metric::close, tazerFclose, unixfclose, fp);
}

size_t tazerFread(TazerFile *file, unsigned int pos, int fd, void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
    return (size_t)read(fd, ptr, size * n);
}

size_t fread(void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
    return outerWrapper("fread", fp, Timer::Metric::read, tazerFread, unixfread, ptr, size, n, fp);
}

size_t tazerFwrite(TazerFile *file, unsigned int pos, int fd, const void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
    return (size_t)write(fd, ptr, size * n);
}

size_t fwrite(const void *__restrict ptr, size_t size, size_t n, FILE *__restrict fp) {
    return outerWrapper("fwrite", fp, Timer::Metric::read, tazerFwrite, unixfwrite, ptr, size, n, fp);
}

long int tazerFtell(TazerFile *file, unsigned int pos, int fd, FILE *fp) {
    return (long int)lseek(fd, 0, SEEK_CUR);
}

long int ftell(FILE *fp) {
    return outerWrapper("ftell", fp, Timer::Metric::ftell, tazerFtell, unixftell, fp);
}

int tazerFseek(TazerFile *file, unsigned int pos, int fd, FILE *fp, long int off, int whence) {
    return lseek(fd, off, whence);
}

int fseek(FILE *fp, long int off, int whence) {
    return outerWrapper("fseek", fp, Timer::Metric::seek, tazerFseek, unixfseek, fp, off, whence);
}

int tazerFgetc(TazerFile *file, unsigned int pos, int fd, FILE *fp) {
    if (!file->eof()) {
        unsigned char buffer;
        read(fd, &buffer, 1);
        return (int)buffer;
    }
    return EOF;
}

int fgetc(FILE *fp) {
    return outerWrapper("fgetc", fp, Timer::Metric::fgetc, tazerFgetc, unixfgetc, fp);
}

char *tazerFgets(TazerFile *file, unsigned int pos, int fd, char *__restrict s, int n, FILE *__restrict fp) {
    char *ret = NULL;
    if (!file->eof()) {
        int prior = file->filePos(pos);
        size_t res = read(fd, s, n - 1);
        if (res) {
            unsigned int index;
            for (index = 0; index < res; index++) {
                if (s[index] == '\n') {
                    index++;
                    break;
                }
            }

            if (index < res) {
                file->seek(prior + index, SEEK_SET, pos);
            }

            s[index] = '\0';
            ret = s;
        }
    }
    return ret;
}

char *fgets(char *__restrict s, int n, FILE *__restrict fp) {
    return outerWrapper("fgets", fp, Timer::Metric::fgets, tazerFgets, unixfgets, s, n, fp);
}

int tazerFputc(TazerFile *file, unsigned int pos, int fd, int c, FILE *fp) {
    write(fd, (void *)&c, 1);
    return c;
}

int fputc(int c, FILE *fp) {
    return outerWrapper("fputc", fp, Timer::Metric::fputc, tazerFputc, unixfputc, c, fp);
}

int tazerFputs(TazerFile *file, unsigned int pos, int fd, const char *__restrict s, FILE *__restrict fp) {
    unsigned int index = 0;
    while (1) {
        if (s[index] == '\0')
            break;
        index++;
    }

    int res = -1;
    if (index)
        res = write(fd, s, index);

    if (res == -1)
        return EOF;
    return res;
}

int fputs(const char *__restrict s, FILE *__restrict fp) {
    return outerWrapper("fputs", fp, Timer::Metric::fputs, tazerFputs, unixfputs, s, fp);
}

int tazerFeof(TazerFile *file, unsigned int pos, int fd, FILE *fp) {
    return file->eof();
}

int feof(FILE *fp) {
    return outerWrapper("feof", fp, Timer::Metric::feof, tazerFeof, unixfeof, fp);
}

off_t tazerRewind(TazerFile *file, unsigned int pos, int fd, int fd2, off_t offset, int whence) {
    auto ret = (off_t)file->seek(offset, whence, pos);
    return ret;
}

void rewind(FILE *fp) {
    // ReaderWriterLock * lock = NULL;
    // int fd = TazerFileStream::lookupStream(fp, lock);
    // outerWrapper(fp, Timer::Metric::rewind, tazerRewind, unixlseek, fd, 0L, SEEK_SET);
    fseek(fp, 0L, SEEK_SET);
}
