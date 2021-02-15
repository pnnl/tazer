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

#include <chrono>
#include <cstring>
#include <experimental/filesystem>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <thread>
#include <unistd.h>

#include "caches/Cache.h"
#include "caches/bounded/NewBoundedFilelockCache.h"
#include "caches/bounded/NewMemoryCache.h"
#include "caches/bounded/NewSharedMemoryCache.h"
#include "caches/unbounded/UrlCache.h"
#include "caches/LocalFileCache.h"
#include "caches/NetworkCache.h"
#include "caches/UrlFileCache.h"
#include "caches/UrlDownload.h"

#include "Config.h"
#include "FileCacheRegister.h"
#include "Message.h"
#include "Request.h"
#include "ServeFile.h"
#include "UnixIO.h"
#include "lz4.h"
#include "lz4hc.h"
#include "MetaFileParser.h"


#define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
//#define DPRINTF(...)
Cache ServeFile::_cache(BASECACHENAME,CacheType::base);

ThreadPool<std::function<void()>> ServeFile::_pool(Config::numServerCompThreads);
std::vector<Connection *> ServeFile::_connections;
PriorityThreadPool<std::packaged_task<std::shared_future<Request *>()>> ServeFile::_transferPool(Config::numClientTransThreads, "transfer pool");
PriorityThreadPool<std::packaged_task<Request *()>> ServeFile::_decompressionPool(Config::numClientDecompThreads, "decompress pool");

bool ServeFile::addConnections() {
    unixopen_t unixOpen = (unixopen_t)dlsym(RTLD_NEXT, "open");
    // std::cout << Config::ServerConnectionsPath.c_str() << std::endl;
    int fd = (*unixOpen)(Config::ServerConnectionsPath.c_str(), O_RDONLY);
    MetaFileParser parse(fd);
    // std::cout << fd << std::endl;
    uint32_t numServers = 0;
    Entry e;
    while (parse.getNext(&e)){
        Connection *connection = Connection::addNewClientConnection(e.host, e.port);
        std::cout << e.host << " " << e.port << " " << connection << std::endl;
        if (connection) {
            if (ConnectionPool::useCnt->count(connection->addrport()) == 0) {
                ConnectionPool::useCnt->emplace(connection->addrport(), 0);
                ConnectionPool::consecCnt->emplace(connection->addrport(), 0);
            }
            _connections.push_back(connection);
            numServers++;
        }
    }
    return (numServers > 0);
}

void ServeFile::cache_init(void) {
    uint64_t level = 0;
    Cache *c = NewMemoryCache::addNewMemoryCache(NEWMEMORYCACHENAME, CacheType::privateMemory, Config::serverCacheSize, Config::serverCacheBlocksize, Config::serverCacheAssociativity);
    std::cerr << "[TAZER] " << "mem cache: " << (void *)c << std::endl;
    ServeFile::_cache.addCacheLevel(c, ++level);

    if (Config::useSharedMemoryCache) {
        c = NewSharedMemoryCache::addNewSharedMemoryCache(NEWSHAREDMEMORYCACHENAME, CacheType::sharedMemory, Config::sharedMemoryCacheSize, Config::sharedMemoryCacheBlocksize, Config::sharedMemoryCacheAssociativity);
        std::cerr << "[TAZER] " << "shared mem  cache: " << (void *)c << std::endl;
        ServeFile::_cache.addCacheLevel(c, ++level);
    }

    #ifdef USE_CURL
        if(Config::urlFileCacheOn) {
             c = UrlFileCache::addNewUrlFileCache(URLFILECACHENAME, CacheType::local);
            std::cerr << "[TAZER] " << "Url file cache: " << (void *)c << std::endl;
            ServeFile::_cache.addCacheLevel(c, ++level);
        }
        else {
            c = UrlCache::addNewUrlCache(URLCACHENAME, CacheType::urlCache, Config::sharedMemoryCacheBlocksize);
            std::cerr << "[TAZER] " << "Url cache: " << (void *)c << std::endl;
            ServeFile::_cache.addCacheLevel(c, ++level);

            c = LocalFileCache::addNewLocalFileCache(LOCALFILECACHENAME, CacheType::local);
            std::cerr << "[TAZER] " << "Local file cache: " << (void *)c << std::endl;
            ServeFile::_cache.addCacheLevel(c, ++level);
        }
    #else
        c = LocalFileCache::addNewLocalFileCache(LOCALFILECACHENAME, CacheType::local);
        std::cerr << "[TAZER] " << "Local file cache: " << (void *)c << std::endl;
        ServeFile::_cache.addCacheLevel(c, ++level);
    #endif

    

    if (Config::useBoundedFilelockCache) {
        c = NewBoundedFilelockCache::addNewBoundedFilelockCache(NEWBOUNDEDFILELOCKCACHENAME, CacheType::boundedGlobalFile, Config::boundedFilelockCacheSize, Config::boundedFilelockCacheBlocksize, Config::boundedFilelockCacheAssociativity, Config::boundedFilelockCacheFilePath);
        std::cerr << "[TAZER] " << "bounded filelock cache: " << (void *)c << std::endl;
        ServeFile::_cache.addCacheLevel(c, ++level);
    }

    if (Config::useServerNetworkCache) {
        c = NetworkCache::addNewNetworkCache(NETWORKCACHENAME, CacheType::network, ServeFile::_transferPool, ServeFile::_decompressionPool);
        std::cerr << "[TAZER] " << "net cache: " << (void *)c << std::endl;
        ServeFile::_cache.addCacheLevel(c, ++level);
        addConnections();
        ServeFile::_transferPool.initiate();
        ServeFile::_decompressionPool.initiate();
    }
}

ServeFile::ServeFile(std::string name, bool compress, uint64_t blkSize, uint64_t initialCompressTasks, bool output, bool remove) : Loggable(Config::ServeFileLog, "ServeFile"),
                                                                                                                                   _name(name),
                                                                                                                                   _output(output),
                                                                                                                                   _remove(remove),
                                                                                                                                   _compress(compress),
                                                                                                                                   _blkSize(blkSize),
                                                                                                                                   _maxCompSize(0),
                                                                                                                                   _compLevel(0),
                                                                                                                                   _initialCompressTasks(initialCompressTasks),
                                                                                                                                   _size(0),
                                                                                                                                   _numBlks(0),
                                                                                                                                   _open(false),
                                                                                                                                   _outstandingWrites(0),
                                                                                                                                   _url(supportedUrlType(name)) {
    _pool.initiate();
    ConnectionPool * pool = NULL;

    log(this) << "file: " << _name << std::endl;
    if(_url) {
        _size = sizeUrlPath(name);
    }
    else {
        // unsigned int retry = 0;
        struct stat sbuf;
        sbuf.st_size = 0;
        if (stat(_name.c_str(), &sbuf) == 0) {
            if (!output) {
                _size = sbuf.st_size;
            }
        }

        if (_size == 0) {
            if (Config::useServerNetworkCache && !output) {
                // std::cout << "in  net: " << _name << std::endl;
                bool created;
                pool = ConnectionPool::addNewConnectionPool(_name, _compress, _connections, created);
                _size = pool->openFileOnAllServers();
            }
        }
    }
    log(this) << "size: " << _size << std::endl;

    if (_size || output) {

        if (output) {
            // std::cout <<"creating file for writing"<<std::endl;
            bool created = std::experimental::filesystem::create_directories(std::experimental::filesystem::path(_name).parent_path());
            std::ofstream file;
            file.open(_name, std::fstream::binary);
            file.close();
        }
        else {
            if (_blkSize > _size) {
                _blkSize = _size;
            }
            _maxCompSize = LZ4_compressBound(_blkSize);
            _numBlks = _size / _blkSize;
            if (_size % _blkSize != 0) {
                _numBlks++;
            }

            log(this) << "about to create file cache register" << std::endl;
            FileCacheRegister *reg = FileCacheRegister::openFileCacheRegister();
            _regFileIndex = reg->registerFile(_name);
            _cache.addFile(_regFileIndex, _name, _blkSize, _size);

            if (Config::useServerNetworkCache) {
                NetworkCache *nc = (NetworkCache *)_cache.getCacheByName(NETWORKCACHENAME);
                if (nc) {
                    nc->setFileCompress(_regFileIndex, _compress);
                    nc->setFileConnectionPool(_regFileIndex, pool);
                }
            }

            unsigned int toCompress = initialCompressTasks;
            if (toCompress > _numBlks)
                toCompress = _numBlks;

            for (unsigned int i = 0; i < initialCompressTasks; i++) {
                addCompressTask(i);
            }
        }
        std::cout << "Opened " << _name << " " << output << " size: " << _size << std::endl;
        _open = true;
    }

    else {
        std::cout << "ERROR: file " << _name << " does not exists" << std::endl;
    }
}

ServeFile::~ServeFile() {
    //Make sure outstanding prefetches are done first!!!
    _prefetchLock.writerLock();

    while (_outstandingWrites) {
        sched_yield();
    }
    _pool.terminate();

    if (_output && _remove) {
        std::cout<<"removing: "<<_name<<std::endl;
        remove(_name.c_str());
    }
    if(!_output){ //output files arent actually present withing the caching structure
    #ifdef USE_CURL
        if(Config::urlFileCacheOn)
            ((UrlFileCache*)(_cache.getCacheByName(URLFILECACHENAME)))->removeFile(_regFileIndex);
        else 
        {
            if(_url)
                ((UrlCache*)(_cache.getCacheByName(URLCACHENAME)))->removeFile(_regFileIndex);
            else
                ((LocalFileCache*)_cache.getCacheByName(LOCALFILECACHENAME))->removeFile(_regFileIndex);
        }
    #else
        ((LocalFileCache*)_cache.getCacheByName(LOCALFILECACHENAME))->removeFile(_regFileIndex);
    #endif 
    }
    log(this) << _name << " closed" << std::endl;
}

void ServeFile::addCompressTask(uint32_t blk) {
    log(this) << "addCompressTask " << blk << std::endl;
    // int zero = 0;
    // if (_prefetchLock.tryReaderLock()) { //This makes sure the file isn't deleted
    //     if (std::atomic_compare_exchange_strong(&_blocks[blk].status, &zero, 1)) {
    //         _pool.addTask([this, blk] {
    //             transferBlk(NULL, blk);
    //             //                        compress(blk);
    //             if (blk + _initialCompressTasks < _numBlks && _cache.freeSpace() >= _blkSize)
    //                 addCompressTask(blk + _initialCompressTasks);
    //         });
    //     }
    //     _prefetchLock.readerUnlock();
    // }
}

uint64_t ServeFile::compress(uint64_t blk, uint8_t *blkData, uint8_t *&msgData) {
    log(this) << "Compress " << std::endl;

    int64_t size = _blkSize;
    if ((blk + 1) * _blkSize > _size) {
        size = _size - (blk * _blkSize);
    }

    int64_t compSize;
    msgData = new uint8_t[_maxCompSize];

    if (_compLevel < 0) {
        compSize = LZ4_compress_fast((char *)blkData, (char *)msgData, size, _maxCompSize, -_compLevel);
    }
    else if (_compLevel == 0) {
        compSize = LZ4_compress_default((char *)blkData, (char *)msgData, size, _maxCompSize);
    }
    else {
        compSize = LZ4_compress_HC((char *)blkData, (char *)msgData, size, _maxCompSize, _compLevel);
    }
    size = compSize;
    return size;
}

bool ServeFile::sendData(Connection *connection, uint64_t blk, Request *request) {
    uint8_t *msgData;
    uint64_t msgSize = request->size;
    if (_compress) {
        msgSize = compress(blk, request->data, msgData);
    }
    else {
        msgData = request->data;
    }
    sendBlkMsg packet;
    fillMsgHeader((char *)&packet, SEND_BLK_MSG, _name.size() + 1, msgSize + sizeof(sendBlkMsg) + _name.size() + 1);
    packet.compression = _compLevel; //I think this makes sense...
    packet.blk = blk;
    packet.dataSize = msgSize;
    bool ret = false;
    if (connection) {
        ret = serverSendCloseNew(connection, &packet, _name, (char *)msgData, msgSize);
    }
    ServeFile::_cache.bufferWrite(request);
    log(this) << "sending: " << blk << " size: " << msgSize << " " << ret << std::endl;
    return ret;
}

bool ServeFile::transferBlk(Connection *connection, uint32_t blk) {
    if (!_output && blk < _numBlks) {
        log(this) << "Transfer blk " << blk << " of " << _numBlks << std::endl;
        while (1) {
            //See if it is in the cache or someone is in the process of loading it

            std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> reads;

            auto request = _cache.requestBlock(blk, _blkSize, _regFileIndex, reads, 0);
            if (request->ready) {
                request->originating->stats.addAmt(false, CacheStats::Metric::read, _blkSize);
                // std::cout << "cache: " << _name << " " << blk << std::endl;
                return sendData(connection, blk, request);
            }
            else {
                auto pending = reads[blk];
                auto stallTime = Timer::getCurrentTime();
                auto request = pending.get().get();
                if (request->waitingCache != CacheType::empty){
                    _cache.getCacheByType(request->waitingCache)->stats.addTime(0, CacheStats::Metric::stalls, Timer::getCurrentTime() - stallTime, 1);
                }
                else{
                    err(this) << "waiting cache is empty"<<std::endl;
                }
                request->originating->stats.addTime(0, CacheStats::Metric::stalled, Timer::getCurrentTime() - stallTime, 1);
                if (request->ready) {
                    // std::cout << "net: " << _name << " " << blk << std::endl;
                    if (request->waitingCache != CacheType::empty){
                        _cache.getCacheByType(request->waitingCache)->stats.addAmt(0, CacheStats::Metric::stalls, _blkSize);
                    }
                    else{
                        err(this) << "waiting cache is empty"<<std::endl;
                    }

                    request->originating->stats.addAmt(false, CacheStats::Metric::stalled, _blkSize);
                    return sendData(connection, blk, request);
                }
                else {
                    std::cout << "REQUEST failure" << std::endl;
                    return false;
                }
            }

            //could easily do a prefetching algorithm here....
            sched_yield();
        }
    }
    return false;
}

bool ServeFile::writeData(char *data, uint64_t size, uint64_t fp) {
    if (_output) {
        if (_prefetchLock.tryReaderLock()) {
            _outstandingWrites++;
            char *odata = new char[size];
            log(this)<<"writing: "<<size<<" into "<<_name<<std::endl;
            memcpy(odata, data, size);
            _pool.addTask([this, odata, size, fp] {
                std::ofstream file;
                file.open(_name, std::fstream::binary|std::ofstream::app);
                std::unique_lock<std::mutex> flock(_fileMutex);
                file.seekp(fp, file.beg);
                file.write(odata, size);
                file.flush();
                flock.unlock();
                file.close();
                delete[] odata;
                _outstandingWrites--;
            });
            _prefetchLock.readerUnlock();
        }
        return true;
    }
    *this << "Not an output file... no writing!!!" << std::endl;
    return false;
}

std::string ServeFile::name() {
    return _name;
}

uint64_t ServeFile::size() {
    return _size;
}

uint64_t ServeFile::blkSize() {
    return _blkSize;
}

bool ServeFile::compress() {
    return _compress;
}

bool ServeFile::open() {
    return _open;
}

ServeFile *ServeFile::addNewServeFile(std::string name, bool compress, uint64_t blkSize, uint64_t initialCompressTask, bool output, bool remove) {
    return Trackable<std::string, ServeFile *>::AddTrackable(name, [=] {
        // std::cout<<"add new server file: "<<name<<std::endl;
        ServeFile *newFile = new ServeFile(name, compress, blkSize, initialCompressTask, output, remove);
        if (newFile->open()) {
            return newFile;
        }
        delete newFile;
        return (ServeFile *)NULL;
    });
}

ServeFile *ServeFile::getServeFile(std::string fileName) {
    return Trackable<std::string, ServeFile *>::LookupTrackable(fileName);
}

bool ServeFile::removeServeFile(std::string fileName) {
    return Trackable<std::string, ServeFile *>::RemoveTrackable(fileName);
}

bool ServeFile::removeServeFile(ServeFile *file) {
    return removeServeFile(file->name());
}
