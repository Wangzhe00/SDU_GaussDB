/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-12 13:09:47
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-13 19:25:55
 * @FilePath: /src/inc/baseStruct.h
 */
#ifndef BASE_STRUCT_H
#define BASE_STRUCT_H

#include <cstdint>
#include "const.h"
#include "list.h"

typedef struct {
    uint16_t freq;            /* frequency */
    uint16_t len;             /* bucket real len */
    struct list_head list;    /* list of head node */
    struct list_head pool;    /* list of lfu memory pool */
    struct list_head hhead;   /* hash head, need O(1) get tail in shrink F phase */
} LFUHead;

/* node size = 4 * sizeof(ptr) */
typedef struct {
    LFUHead *head;
    struct list_head hnode;
} LFUNode;


/* size per blk = size * 8K */
typedef struct {
    /**
     * [pageFlg] descrition
     *       +-------------------------------------------+
     *       | 0123 4567 8901 2345                       |
     * bit:  | xxxx_xxxx_xxxx_xxxx xxxx_xxxx xxxx_xxxx   |
     *       +-------------------------------------------+
     * 
     * [0:1] : sizeType, 0: 8K, 1:16K, 2:32K, 3:2MB
     * [2:2] : schedule strategy, 0:LRU, 1:LFU
     * [3:3] : dirty flag, 1:dirty
     * [4:4] : poolType, 0:small, 1:chunk
     * [5:5] : used, 1:used, is belong mempool
     * [6:6] : multiple of bucket size, deepest bucket is 2 layers
     * [7:7] : is ghost
     * 
     **/
    struct {
        uint32_t sizeType : 2;
        uint32_t rep : 1;
        uint32_t dirty : 1;
        uint32_t poolType : 1;
        uint32_t used : 1;
        uint32_t layer : 1;
        uint32_t isG : 1;
    } pageFlg;
    uint32_t page_start;     /* first page num */
    uint32_t bucketIdx;      /* hash bucket index */
    
    uint32_t blkSize;        /* block real size */
    void *blk;               /* first address */

    struct list_head memP;   /* list for mempool */
    struct hlist_node hash;  /* list for hash bucket */
    struct list_head lru;    /* list for lru */

} Node;

typedef struct {
    uint8_t poolType;
    uint16_t size;
    uint32_t capacity;
    uint32_t blkSize;
    uint32_t usedCnt;
    uint32_t unusedCnt;
    struct list_head used;
    struct list_head unused;
}Pool;

typedef struct {
    // uint16_t size;            /* can del */
    struct hlist_head hhead;
// TODO:
    /* add lock or mutex */
    /* add stat ... e.g. hits, timestamp */
} BucketNode;

/* deepest bucket is 4 layers */
typedef struct {
    uint32_t size;
    uint32_t capSize;
    uint32_t miss;
    uint32_t hit;
    uint32_t query;
    uint32_t fetched;
    uint32_t hitWrite;
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
} Arch;

#endif /* BASE_STRUCT_H */