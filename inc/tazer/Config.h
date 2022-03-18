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

#ifndef CONFIG_H
#define CONFIG_H
#include <string>

namespace Config {

#ifndef TEST_SERVER_PORT
const int serverPort = 6023;
#else
const int serverPort = TEST_SERVER_PORT;
#endif

//This will break for rsockets...
#ifndef TEST_SERVER_IP
const std::string serverIpString("127.0.0.1");
#else
const std::string serverIpString(TEST_SERVER_IP);
#endif

#ifndef INPUTS_DIR
const std::string InputsDir(".");
#else
const std::string InputsDir(INPUTS_DIR);
#endif

const int maxSystemFd = 19459526;

#define NUMTHREADS 1

//Thread Pools
const unsigned int numServerThreads = 16;
const unsigned int numServerCompThreads = 1;
const unsigned int numClientTransThreads = NUMTHREADS;
const unsigned int numClientDecompThreads = 1;
const unsigned int numWriteBufferThreads = 1;
const unsigned int numPrefetchThreads = 1;

//Connection Parameters
const unsigned int defaultBufferSize = 1024;
const unsigned int maxConRetry = 10;
const unsigned int messageRetry = (unsigned int)100; //Simulates infinite retry...
const unsigned int socketsPerConnection =  getenv("TAZER_SOCKETS_PER_CONN") ? atoi(getenv("TAZER_SOCKETS_PER_CONN")) : 1;
const unsigned int socketStep = 1024;
const unsigned int socketRetry = 1;

//Input file Parameters
const unsigned int fileOpenRetry = 1;

//Serve file Parameters
const unsigned int numCompressTask = 0;
const unsigned int removeOutput = 0;

static int64_t referenceTime = getenv("TAZER_REF_TIME") ? atol(getenv("TAZER_REF_TIME")) : 0;

//architecure Parameters
const bool enableSharedMem = getenv("TAZER_ENABLE_SHARED_MEMORY") ? atoi(getenv("TAZER_ENABLE_SHARED_MEMORY")) : 1;

//----------------------cache parameters-----------------------------------

const std::string tazer_id(getenv("USER") ? "tazer" + std::string(getenv("USER")) : "tazer");

const uint64_t maxBlockSize = getenv("TAZER_BLOCKSIZE") ? atol(getenv("TAZER_BLOCKSIZE")) : 1 * 1024UL * 1024UL;
#define BOUNDEDCACHENAME "boundedcache"
#define NETWORKCACHENAME "network"

//Memory Cache Parameters
const bool useMemoryCache = true;
static uint64_t memoryCacheSize = getenv("TAZER_PRIVATE_MEM_CACHE_SIZE") ? atol(getenv("TAZER_PRIVATE_MEM_CACHE_SIZE")) : 64 * 1024 * 1024UL;
const uint32_t memoryCacheAssociativity = 16UL;
const uint64_t memoryCacheBlocksize = maxBlockSize;

//Scalable Cache Parameters
const bool useScalableCache = getenv("TAZER_SCALABLE_CACHE") ? atoi(getenv("TAZER_SCALABLE_CACHE")) : 0;
const uint32_t scalableCacheNumBlocks = getenv("TAZER_SCALABLE_CACHE_NUM_BLOCKS") ? atoi(getenv("TAZER_SCALABLE_CACHE_NUM_BLOCKS")) : 16;
//0:addAdaptiveAllocator, 1:addStealingAllocator (LRU), 2:addRandomStealingAllocator (Random File, Oldest Block), 3:addRandomStealingAllocator (Random File, Random Block), 4:LargestStealingAllocator, 5:FirstTouchAllocator, 6:addSimpleAllocator
const uint32_t scalableCacheAllocator = getenv("TAZER_SCALABLE_CACHE_ALLOCATOR") ? atoi(getenv("TAZER_SCALABLE_CACHE_ALLOCATOR")) : 0;


//Shared Memory Cache Parameters
const bool useSharedMemoryCache = getenv("TAZER_SHARED_MEM_CACHE") ? atoi(getenv("TAZER_SHARED_MEM_CACHE")) : 0;
static uint64_t sharedMemoryCacheSize = getenv("TAZER_SHARED_MEM_CACHE_SIZE") ? atol(getenv("TAZER_SHARED_MEM_CACHE_SIZE")) : 1 * 1024 * 1024 * 1024UL;
const uint32_t sharedMemoryCacheAssociativity = 16UL;
const uint64_t sharedMemoryCacheBlocksize = maxBlockSize;

//BurstBuffer Cache Parameters
const bool useBurstBufferCache = getenv("TAZER_BB_CACHE") ? atoi(getenv("TAZER_BB_CACHE")) : 0;
static uint64_t burstBufferCacheSize = getenv("TAZER_BB_CACHE_SIZE") ? atol(getenv("TAZER_BB_CACHE_SIZE")) : 1 * 1024 * 1024 * 1024UL;
const uint32_t burstBufferCacheAssociativity = 16UL;
const uint64_t burstBufferCacheBlocksize = maxBlockSize;
const std::string burstBufferCacheFilePath("/tmp/" + tazer_id + "/tazer_cache/bbc"); // TODO: have option to pass from environment variable

//File Cache Parameters
const bool useFileCache = getenv("TAZER_FILE_CACHE") ? atoi(getenv("TAZER_FILE_CACHE")) : 0;
static uint64_t fileCacheSize = getenv("TAZER_FILE_CACHE_SIZE") ? atol(getenv("TAZER_FILE_CACHE_SIZE")) : 1 * 1024 * 1024 * 1024UL;
const uint32_t fileCacheAssociativity = 16UL;
const uint64_t fileCacheBlocksize = maxBlockSize;
const std::string fileCacheFilePath("/tmp/" + tazer_id + "/tazer_cache/fc"); // TODO: have option to pass from environment variable

//Bounded Filelock Cache Parameters
const bool useBoundedFilelockCache = getenv("TAZER_BOUNDED_FILELOCK_CACHE") ? atoi(getenv("TAZER_BOUNDED_FILELOCK_CACHE")) : 1;
static uint64_t boundedFilelockCacheSize = getenv("TAZER_BOUNDED_FILELOCK_CACHE_SIZE") ? atol(getenv("TAZER_BOUNDED_FILELOCK_CACHE_SIZE")) : 1 * 1024 * 1024 * 1024UL;
const uint32_t boundedFilelockCacheAssociativity = 16UL;
const uint64_t boundedFilelockCacheBlocksize = maxBlockSize;
const std::string boundedFilelockCacheFilePath = getenv("TAZER_BOUNDED_FILELOCK_CACHE_PATH") ? std::string(getenv("TAZER_BOUNDED_FILELOCK_CACHE_PATH")) : std::string("/tmp/" + tazer_id + "/tazer_cache/gc"); // TODO: have option to pass from environment variable

//Filelock Cache Parameters
const bool useFilelockCache = getenv("TAZER_FILELOCK_CACHE") ? atoi(getenv("TAZER_FILELOCK_CACHE")) : 0;
const uint64_t filelockCacheBlocksize = maxBlockSize;
const std::string filelockCacheFilePath = getenv("TAZER_FILELOCK_CACHE_PATH") ? std::string(getenv("TAZER_FILELOCK_CACHE_PATH")) : std::string("/tmp/" + tazer_id + "/tazer_cache/gc");

//Network Cache Parameters
const bool useNetworkCache = getenv("TAZER_NETWORK_CACHE") ? atoi(getenv("TAZER_NETWORK_CACHE")) : 1;
const uint64_t networkBlockSize = maxBlockSize;

//LocalFile Cache Parameters
const bool useLocalFileCache = getenv("TAZER_LOCAL_FILE_CACHE") ? atoi(getenv("TAZER_LOCAL_FILE_CACHE")) : 0;
const uint64_t localFileBlockSize = maxBlockSize;

//--------------------------------------------------------------------------------

const bool timerOn = true;
const bool printThreadMetric = false;
const bool printNodeMetric = true;
const bool printHits = true;
const int printStats = getenv("TAZER_PRINT_STATS") ? atoi(getenv("TAZER_PRINT_STATS")) : 1;

//-----------------------------------------------------

// server parameters
const bool useServerNetworkCache = getenv("TAZER_NETWORK_CACHE") ? atoi(getenv("TAZER_NETWORK_CACHE")) : 0;
const uint64_t serverCacheSize = getenv("TAZER_SERVER_CACHE_SIZE") ? atol(getenv("TAZER_SERVER_CACHE_SIZE")) : 20UL * 1024 * 1024 * 1024;
const uint64_t serverCacheBlocksize = maxBlockSize;
const uint32_t serverCacheAssociativity = 16UL;
const std::string ServerConnectionsPath(getenv("TAZER_SERVER_CONNECTIONS") ? getenv("TAZER_SERVER_CONNECTIONS") : "");

const bool prefetchEvict = getenv("TAZER_PREFETCH_EVICT") ? atoi(getenv("TAZER_PREFETCH_EVICT")) : 0; //When evicting a block, choose prefetched blocks first.

//-----------------------------------------------------
// Prefetching config
const unsigned int prefetcherType = getenv("TAZER_PREFETCHER_TYPE") ? atoi(getenv("TAZER_PREFETCHER_TYPE")) : 0;
const unsigned int numPrefetchBlks = getenv("TAZER_PREFETCH_NUM_BLKS") ? atoi(getenv("TAZER_PREFETCH_NUM_BLKS")) : 1;
const int prefetchDelta = getenv("TAZER_PREFETCH_DELTA") ? atoi(getenv("TAZER_PREFETCH_DELTA")) : 1;
const std::string prefetchFileDir = getenv("TAZER_PREFETCH_FILEDIR") ? getenv("TAZER_PREFETCH_FILEDIR") : "./";

//const bool prefetchGlobal = getenv("TAZER_PREFETCH_GLOBAL") ? atoi(getenv("TAZER_PREFETCH_GLOBAL")) : 1;
//const unsigned int prefetchGap = getenv("TAZER_PREFETCH_GAP") ? atoi(getenv("TAZER_PREFETCH_GAP")) : 0;
//const bool prefetchAllBlks = getenv("TAZER_PREFETCH_ALLBLKS") ? atoi(getenv("TAZER_PREFETCH_ALLBLKS")) : 0;
//const bool prefetchNextBlks = getenv("TAZER_PREFETCH") ? atoi(getenv("TAZER_PREFETCH")) : 1;

//-----------------------------------------------------
//Curl parameters
const unsigned int numCurlHandles = getenv("TAZER_CURL_HANDLES") ? atoi(getenv("TAZER_CURL_HANDLES")) : 10;
const long UrlTimeOut = getenv("TAZER_URL_TIMEOUT") ? atol(getenv("TAZER_URL_TIMEOUT")) : 1; //Seconds
const std::string DownloadPath(getenv("TAZER_DOWNLOAD_PATH") ? getenv("TAZER_DOWNLOAD_PATH") : ".");
const bool deleteDownloads = getenv("TAZER_DELETE_DOWNLOADS") ? atoi(getenv("TAZER_DELETE_DOWNLOADS")) : 1;
const bool downloadForSize = getenv("TAZER_DOWNLOAD_FOR_SIZE") ? atoi(getenv("TAZER_DOWNLOAD_FOR_SIZE")) : 1;
const bool curlOnStartup = getenv("TAZER_CURL_ON_START") ? atoi(getenv("TAZER_CURL_ON_START")) : 0;
const bool urlFileCacheOn = getenv("TAZER_URL_FILE_CACHE_ON") ? atoi(getenv("TAZER_URL_FILE_CACHE_ON")) : 0;
//-----------------------------------------------------

const uint64_t outputFileBufferSize = 16UL * 1024 * 1024;

const bool reuseFileCache = true; //josh, what is the point of a reusable cache if you dont reuse it!?!?! ;)
const bool bufferFileCacheWrites = true;

const unsigned int ReservationTimeOut = -1;

const std::string sharedMemName("/" + tazer_id + "ioCache");




const bool ThreadStats = getenv("TAZER_THREAD_STATS") ? atoi(getenv("TAZER_THREAD_STATS")) : 0;
const bool TrackBlockStats = false;
const bool TrackReads = false;
const unsigned int FdsPerLocalFile = 10;

const bool WriteFileLog = false;
const bool TazerFileLog = false;
const bool CacheLog = false;
const bool ServerConLog = false;
const bool ClientConLog = false;
const bool ServeFileLog = false;
const bool CacheFileLog = false;
const bool FileCacheLog = false;
const bool PrefetcherLog = false;

#define FILE_CACHE_WRITE_THROUGH 1

//#define PRINTF(...) fprintf(stderr, __VA_ARGS__)
#define PRINTF(...)

} // namespace Config

#endif /* CONFIG_H */
