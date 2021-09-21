/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-12 13:09:47
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-19 01:16:14
 * @FilePath: /src/inc/baseStruct.h
 */
#ifndef BASE_STRUCT_H
#define BASE_STRUCT_H

#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include "const.h"
#include "list.h"


class Mutex
{
  std::atomic_bool flag;
public:
  inline void lock(void) {
    bool e = false;
    while (!flag.compare_exchange_weak(e, true, std::memory_order_acquire)) { 
      std::this_thread::yield(); // do not spin
      e = false; 
    }
  }
  
  inline void unlock(void) {
    flag.store(false, std::memory_order_release);
  }

  inline bool try_lock(void) {
    bool e = false;
    if (flag.compare_exchange_strong(e, true)) {
        return true;
    }
    return false;
  }
};

/* node size = 6 * sizeof(ptr) + 4*/
typedef struct {
    uint16_t freq;              /* frequency */
    uint32_t len;               /* vertical real len */
    struct list_head hlist;     /* horizontal list */
    struct list_head vlist;     /* vertical list, need O(1) get tail in shrink phase */
    struct list_head pool;      /* list of lfu memory pool */
} LFUHead;

/* node size = 3 * sizeof(ptr) */
typedef struct {
    LFUHead *head;
    struct list_head hnode;
} LFUNode;


/* sizeof(Node) = sizeof(ptr) * 6 + sizeof(uint32_t) */
typedef struct {
    struct {
        uint32_t sizeType : 2;  /* 0: 8K, 1:16K, 2:32K, 3:2MB */
        uint32_t dirty : 1;     /* 1:dirty */
        uint32_t poolType : 1;  /* 0:small, 1:chunk */
        uint32_t used : 1;      /* 1:used, is belong mempool */
        uint32_t pno : 24;      /* hash bucket index */
    } pageFlg;
    void *blk;                  /* first address */
    struct list_head memP;      /* list for mempool */
    struct hlist_node hash;     /* list for hash bucket */
    // struct list_head lru;       /* list for lru */
    LFUNode lfu;                /* list for lfu */
} Node;

typedef struct {
    uint8_t poolType;
    uint32_t size;
    uint64_t capacity;
    uint32_t blkSize;
    uint32_t usedCnt;
    uint32_t unusedCnt;
    struct list_head used;
    struct list_head unused;
}Pool;

// #pragma pack(1)
typedef struct {
    Mutex bLock;
    // std::mutex bLock;
    uint16_t freq;
    struct hlist_head hhead;
} BucketNode;
// #pragma pack(pop)

/* deepest bucket is 4 layers */
typedef struct {
    uint32_t size;
    uint32_t capSize;
    std::atomic_uint32_t miss;
    std::atomic_uint32_t hit;
    std::atomic_uint32_t fetched;
    std::atomic_uint32_t hitWrite;
    std::atomic_uint32_t readCnt;
    std::atomic_uint32_t writeCnt;
    BucketNode *bkt;
} HashBucket;

class ARC {
public:
    uint32_t RRealLen;
    uint32_t RExpLen;
    uint32_t gRRealLen;
    uint32_t gRExpLen;
    uint32_t FRealLen;
    uint32_t FExpLen;
    uint32_t gFRealLen;
    uint32_t gFExpLen;
    uint32_t lfuUsedPoolSize;
    uint32_t lfuUnusedPoolSize;
    struct list_head R;
    struct list_head gR;
    struct list_head gF;
    LFUHead F;
    struct list_head lfuHeadUsedPool;
    struct list_head lfuHeadUnusedPool;
public:
    ARC();

    void InitRep(uint32_t size, uint8_t flg);
    void LFUHeaderClear(LFUHead *lfuHead);
    void LFU_GetFreq2(LFUHead *freq2);
    void ReturnMemPool(Node *node, Pool *pool);
    void GetNodeFromMemPool(Node *node, Pool *pool);
    void LFU_Shrink(HashBucket *bkt, Pool *pool);
    void LFU_GhostShrink();
    void ARC_RHit(Node *node, HashBucket* bkt, Pool *pool);
    void ARC_gRHit(Node *node);
    LFUHead *LFU_GetNewHeadNode();
    void ARC_FHit(Node *node);
    void ARC_gFHit(Node *node);
    void LRU_GhostShrink();
    void LRU_Shrink(HashBucket *bkt);

    uint8_t hit(Node *node, HashBucket* bkt, Pool *pool);
    void FULLMiss();
};

typedef struct {
    uint32_t lruSize;
    struct list_head head;
} LRU;



typedef struct {
    Pool pool;
    HashBucket bkt;
    LRU rep;
    LFUHead lfu;
    struct list_head lfuHeadUsedPool;
    struct list_head lfuHeadUnusedPool;
} Arch;

#endif /* BASE_STRUCT_H */