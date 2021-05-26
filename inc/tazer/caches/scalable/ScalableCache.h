#ifndef ScalableCache_H
#define ScalableCache_H
#include "Cache.h"
#include "Loggable.h"
#include "ReaderWriterLock.h"
#include "ScalableRegistry.h"
#include "Trackable.h"
#include "UnixIO.h"
#include <future>
#include <memory>

#define BLK_EMPTY 0
#define BLK_PRE 1
#define BLK_RES 2
#define BLK_WR 3
#define BLK_AVAIL 4
#define BLK_EVICT 5

template <class Lock>
class ScalableCache : public Cache {
  public:
    ScalableCache(std::string cacheName, CacheType type, uint64_t blockSize, ScalableRegistry* registry);
    virtual ~ScalableCache();

    virtual bool writeBlock(Request *req);
    virtual void readBlock(Request *req, std::unordered_map<uint32_t, std::shared_future<std::shared_future<Request *>>> &reads, uint64_t priority);
    virtual void addFile(uint32_t index, std::string filename, uint64_t blockSize, std::uint64_t fileSize);

    //TODO: merge/reimplement from old cache structure...
    virtual void cleanReservation();
    virtual void sendCloseSignal(uint32_t index);
    virtual void sendOpenSignal(uint32_t index);
    virtual uint8_t * sendBlock();
    void updateMaxBlocks(uint32_t blockSize);
    uint32_t getMaxBlocks();
  protected:
    struct BlockEntry {
        uint32_t id;
        uint32_t fileIndex;
        uint32_t blockIndex;
        uint32_t timeStamp;
        uint32_t status;
        uint32_t prefetched;
        std::atomic<uint32_t> activeCnt;
        std::atomic<CacheType> origCache; 
        uint8_t* blkAddr;
        std::atomic<uint32_t> currentHits;

        void init(ScalableCache* c,uint32_t entryId){
          // memset(this,0,sizeof(BlockEntry));
          id=entryId;
          fileIndex =0;
          blockIndex = 0;
          timeStamp = 0;
          status = 0;
          prefetched= 0;
          blkAddr= NULL;
          std::atomic_init(&origCache,CacheType::empty);
          std::atomic_init(&activeCnt, (uint32_t)0);
          std::atomic_init(&currentHits, (uint32_t)0);
        }
        BlockEntry& operator= (const BlockEntry &entry){
          if (this == &entry){
            return *this;
          }
          id = entry.id;
          fileIndex = entry.fileIndex;
          blockIndex = entry.blockIndex;
          timeStamp = entry.timeStamp;
          status = entry.status;
          prefetched= entry.prefetched;
          blkAddr = entry.blkAddr;
          origCache.store(entry.origCache.load());
          activeCnt.store(entry.activeCnt.load());
          return *this;
        }
        std::string str(){
          std::stringstream ss;
          ss<<"id: "<<id<<" fi: "<<fileIndex<<" bi: "<<blockIndex<<" status: "<<status<<" oc: "<<cacheTypeName(origCache);
          return ss.str();
        }
    };

    virtual BlockEntry* getBlock(uint32_t index, uint32_t fileIndex, Request* req);
    virtual BlockEntry* oldestBlock(uint32_t index, uint32_t fileIndex, Request* req);


    virtual int incBlkCnt(BlockEntry * entry, Request* req) = 0;
    virtual int decBlkCnt(BlockEntry * entry, Request* req) = 0;
    virtual bool anyUsers(BlockEntry * entry, Request* req) = 0;

    //TODO: eventually when we create a flag for stat keeping we probably dont need to store the cacheName...
    virtual void blockSet(BlockEntry* blk, uint32_t fileIndex, uint32_t blockIndex, uint8_t byte,  CacheType type, int32_t prefetch, int32_t activeUpdate,Request* req) = 0;
    virtual std::string blockEntryStr(BlockEntry *entry) = 0;
    virtual bool blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs = false, uint32_t cnt = 0, CacheType *origCache = NULL) = 0;


    // virtual uint8_t *getBlockData(uint32_t blockIndex) = 0;
    virtual void setBlockData(uint8_t *data, uint32_t blockIndex, uint64_t size) = 0;
    virtual BlockEntry* getBlockEntry(uint32_t blockIndex, Request* req) = 0;
 
   
    // uint64_t _cacheSize;
    // uint32_t _associativity;
    // uint32_t _numBins;
    std::atomic<uint64_t> _collisions;
    std::atomic<uint64_t> _prefetchCollisions;

    // std::unordered_map<uint64_t, uint64_t> _blkMap;

    // T *_lock;
    Lock *_blkMapLock;


    uint64_t _blockSize;
    ScalableRegistry* _registry;
    uint32_t _maxNumBlocks;
    uint32_t _curNumBlocks;

    //key = fileIndex + Index, value = blockMeta
    std::unordered_map<uint64_t, BlockEntry*> _blkMap;

    ReaderWriterLock *_localLock;

    enum ReadPattern {
      RANDOM = 0,
      LINEAR
    };

    ReadPattern _pattern;
    bool _onLinearTrack = false; 
    std::atomic<uint32_t> _hitsSinceLastMiss;
    BlockEntry* _lastAccessedBlock = NULL;

  private:
    void trackBlock(std::string cacheName, std::string action, uint32_t fileIndex, uint32_t blockIndex, uint64_t priority);
    void checkPattern();
};

#endif /* ScalableCache_H */
