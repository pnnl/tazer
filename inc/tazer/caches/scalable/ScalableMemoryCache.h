#ifndef ScalableMemoryCache_H
#define ScalableMemoryCache_H
#include "ScalableCache.h"

#define SCALABLEMEMORYCACHENAME "privatememory"

class ScalableMemoryCache : public ScalableCache<MultiReaderWriterLock> {
  public:
    ScalableMemoryCache(std::string cacheName, CacheType type, uint64_t blockSize, ScalableRegistry *registry);
    ~ScalableMemoryCache();

    static Cache *addScalableMemoryCache (std::string cacheName, CacheType type, uint64_t blockSize, ScalableRegistry *registry);
    
    virtual int incBlkCnt(BlockEntry * entry, Request* req);
    virtual int decBlkCnt(BlockEntry * entry, Request* req);
    virtual bool anyUsers(BlockEntry * entry, Request* req);
    
    virtual void setBlockData(uint8_t *data, unsigned int blockIndex, uint64_t size);
    virtual BlockEntry* getBlockEntry(uint32_t entryId,  Request* req);
    virtual std::string blockEntryStr(BlockEntry *entry);
    virtual void blockSet(BlockEntry* blk,  uint32_t fileIndex, uint32_t blockIndex, uint8_t byte, CacheType type, int32_t prefetch, int activeUpdate,Request* req);
    virtual bool blockAvailable(unsigned int index, unsigned int fileIndex, bool checkFs = false, uint32_t cnt = 0, CacheType *origCache = NULL);
    //static Cache* addScalableMemoryCache(std::string cacheName, CacheType type, uint64_t blockSize, ScalableRegistry *registry);
};
#endif

  