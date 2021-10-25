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

#ifndef NewBoundedLinkfileCache_H
#define NewBoundedLinkfileCache_H
#include "NewBoundedCache.h"
#include "FcntlReaderWriterLock.h"
#include "FileLinkReaderWriterLock.h"
#include "ReaderWriterLock.h"

#define BOUNDEDFILELINKCACHENAME "boundedfilelock"
#define BFL_FILENAME_LEN 1024

class NewBoundedLinkfileCache : public NewBoundedCache<FileLinkReaderWriterLock> {
  public:
    NewBoundedLinkfileCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string cachePath);
    virtual ~NewBoundedLinkfileCache();

    virtual bool writeBlock(Request *req);
    static Cache *addNewBoundedLinkfileCache(std::string cacheName, CacheType type, uint64_t cacheSize, uint64_t blockSize, uint32_t associativity, std::string cachePath);

  private:
    struct FileBlockEntry : BlockEntry {
        uint16_t activeCnt;
        uint32_t pid;
        char fileName[BFL_FILENAME_LEN];
        FileBlockEntry(){
          activeCnt = 0;
          pid = 0;
          memset(fileName,0,BFL_FILENAME_LEN);
        }
        FileBlockEntry(NewBoundedCache* c, uint32_t entryId){
          BlockEntry::init(c, entryId);
          activeCnt = 0;
          pid = 0;
          memset(fileName,0, BFL_FILENAME_LEN);
        }
        FileBlockEntry(FileBlockEntry* old){
          *(BlockEntry*)this = *(BlockEntry*)old;
          activeCnt = old->activeCnt;
          pid = old->pid;
          memcpy(fileName,old->fileName,BFL_FILENAME_LEN);
        }
        void init(NewBoundedCache* c,uint32_t entryId){
          BlockEntry::init(c, entryId);
          activeCnt = 0;
          pid = 0;
          memset(fileName,0, BFL_FILENAME_LEN);
        }
    };

    void readFromFile(int fd, uint64_t size, uint8_t *buff);
    void writeToFile(int fd, uint64_t size, uint8_t *buff);

    void preadFromFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset);
    void pwriteToFile(int fd, uint64_t size, uint8_t *buff, uint64_t offset);
    
    void readFileBlockEntry(FileBlockEntry *entry, Request* req);
    void writeFileBlockEntry(FileBlockEntry *entry);

    virtual std::shared_ptr<BlockEntry> getCompareBlkEntry(uint32_t index, uint32_t fileIndex);
    virtual bool sameBlk(BlockEntry *blk1, BlockEntry *blk2);
    

    virtual void blockSet(BlockEntry* blk,  uint32_t fileIndex, uint32_t blockIndex, uint8_t byte, CacheType type, int32_t prefetch, int activeUpdate,Request* req);
    virtual bool blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs = false, uint32_t cnt = 0, CacheType *origCache = NULL);
   
    virtual uint8_t *getBlockData(unsigned int blockIndex);
    virtual void setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size);
    virtual BlockEntry* getBlockEntry(uint32_t blockIndex,  Request* req);
    virtual std::vector<BlockEntry*> readBin(uint32_t binIndex);
    virtual std::string blockEntryStr(BlockEntry *entry);

    virtual int incBlkCnt(BlockEntry * entry, Request* req);
    virtual int decBlkCnt(BlockEntry * entry, Request* req);
    virtual bool anyUsers(BlockEntry * entry, Request* req);

    virtual void cleanUpBlockData(uint8_t *data);

    unixopen_t _open;
    unixclose_t _close;
    unixread_t _read;
    unixwrite_t _write;
    unixlseek_t _lseek;
    unixfsync_t _fsync;
    unixxstat_t _stat;

    ReaderWriterLock *_shmLock;
    FileLinkReaderWriterLock *_blkLock;

    int _binFd;
    int _blkFd;

    uint32_t _pid;

    std::string _cachePath;
    std::string _lockPath;
    std::string _entriesPath;

    FileBlockEntry *_cacheEntries; // essentially a shared memory shadow cache for the node... 
                               // although care has to be taken cause this doesnt necessarily 
                               // reflect the stated of the file cache, a.k.a only gauaranteed acurate when bin is locked


    ThreadPool<std::function<void()>> *_writePool;
    std::atomic<std::uint64_t> _myOutstandingWrites;
};

#endif /* NewBoundedLinkfileCache_H */
