#include "Config.h"
#include "ReaderWriterLock.h"
#include "../inc/tazer/caches/bounded/SharedMemoryCache.h"

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <future>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SCALEABLE_METRIC_FILE_MAX 1000
#define PRINTF(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

int main() {
    uint64_t _numBlocks = Config::sharedMemoryCacheSize / Config::sharedMemoryCacheBlocksize;
    uint32_t _numBins = _numBlocks / Config::sharedMemoryCacheAssociativity;
    unsigned int memBlockEntrySize = SharedMemoryCache::getSizeOfMemBlockEntry();
    unsigned int scalableSize = sizeof(ReaderWriterLock) + (sizeof(double) + sizeof(unsigned int)) * SCALEABLE_METRIC_FILE_MAX;
    unsigned int memSize = sizeof(uint32_t) + Config::sharedMemoryCacheSize + _numBlocks * memBlockEntrySize     + MultiReaderWriterLock::getDataSize(_numBins) + scalableSize;
                        // sizeof(uint32_t) + _cacheSize +                    _numBlocks * sizeof(MemBlockEntry) + MultiReaderWriterLock::getDataSize(_numBins)
    std::string filePath("/" + Config::tazer_id + "_sharedmemory_" + std::to_string(Config::sharedMemoryCacheSize) + "_" + std::to_string(Config::sharedMemoryCacheBlocksize) + "_" + std::to_string(Config::sharedMemoryCacheAssociativity));
    
    auto fd = shm_open(filePath.c_str(), O_RDWR, 0644);
    PRINTF("Trying to open %s %d\n", filePath.c_str(), fd);
    if (fd != -1) {
        PRINTF("Opened Sharmed Memory %s\n", filePath.c_str());
        ftruncate(fd, memSize);
        void * ptr = mmap(NULL, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        uint32_t * init = (uint32_t*) ptr;
        if(*init) {
            PRINTF("Shared Memory was already initialized %s\n", filePath.c_str());
            *init = 2;
            PRINTF("New init %s : %u\n", filePath.c_str(), *init);
            // uint8_t * startPtr = (uint8_t*)ptr;
            // uint8_t * endPtr = startPtr + memSize;
            // uint8_t * lockAddr = endPtr - scalableSize;
            // uint8_t * UMBAddr = lockAddr + sizeof(ReaderWriterLock);
            // uint8_t * UMBCAddr = UMBAddr + sizeof(double) * SCALEABLE_METRIC_FILE_MAX;
            // if(UMBCAddr + sizeof(uint32_t) * SCALEABLE_METRIC_FILE_MAX != endPtr) {
            //     PRINTF("JS: ERROR ON POINTER ARITHMETIC!!! %s\n", filePath.c_str());
            //     PRINTF("JS: %p - %p = %u\n", endPtr, UMBCAddr + sizeof(uint32_t) * SCALEABLE_METRIC_FILE_MAX, endPtr - (UMBCAddr + sizeof(uint32_t) * SCALEABLE_METRIC_FILE_MAX));
            // }
            // else {
            //     ReaderWriterLock * check = (ReaderWriterLock*)lockAddr;
            //     check->print();

            //     PRINTF("Reseting UMB Shared Memory %s\n", filePath.c_str());
            //     ReaderWriterLock * _UMBLock = new (lockAddr) ReaderWriterLock();
            //     double * _UMB = (double*)UMBAddr;
            //     unsigned int * _UMBC = (unsigned int*) UMBCAddr;
            //     for(unsigned int i=0; i<SCALEABLE_METRIC_FILE_MAX; i++) {
            //         _UMB = 0;
            //         _UMBC = 0;
            //     }
            //     _UMBLock->print();
            // }
        }
        shm_unlink(filePath.c_str());
    }
    return 0;
}