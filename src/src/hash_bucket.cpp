/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 20:12:25
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-13 21:53:32
 * @FilePath: /src/src/hash_bucket.cpp
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <map>
#include <unordered_map>
#include <iostream>
#include <mutex>

#include "errcode.h"
#include "const.h"
#include "hash_bucket.h"
#include "disk.h"

using namespace std;

#define DEBUG 1


recursive_mutex g_lruMutex, g_lock;

uint64_t pagePart[PART_CNT][PS_CNT];
// std::unordered_map<uint32_t, uint8_t> size2Idx;

uint8_t InitHashBucket(HashBucket *hashBucket, uint32_t _size)
{
    hashBucket->size = _size;
    hashBucket->capSize = sizeof(BucketNode) * _size;
    hashBucket->miss = 0;
    hashBucket->hit = 0;
    hashBucket->query = 0;
    hashBucket->fetched = 0;
    hashBucket->hitWrite = 0;
    hashBucket->bkt = (BucketNode *)malloc(hashBucket->capSize);
    assert(hashBucket->bkt);
    for (uint32_t i = 0; i < _size; ++i) {
        // hashBucket->bkt[i].size = 0;                /* can del */
        INIT_HLIST_HEAD(&hashBucket->bkt[i].hhead);
    }
    return ROK;
}

/* idx may exceed the size of bucket */
static inline uint32_t GetExpectBucketIdx(uint32_t pno, uint32_t psize)
{
    if (psize == PS_2M) {
        return (pno - pagePart[1][3]) / CHUNK_CACHE_SIZE;
    }
    
    uint8_t pIdx = size2Idx(psize);
    return pagePart[0][pIdx - 1] + (pno - pagePart[1][pIdx - 1]) / pagePart[2][pIdx];
}


static void GetNodeFromMemPool(Node **node, Pool *pool) {
    pool->usedCnt++;
    pool->unusedCnt--;
    *node = list_entry(pool->unused.next, Node, memP);
    list_del(pool->unused.next);
    list_add(&(*node)->memP, &pool->used);
}

static inline void InitNodePara(Node *node, uint8_t sizeType, uint8_t poolType, uint32_t bucketIdx)
{
    memset(&node->pageFlg, 0, sizeof(node->pageFlg));
    node->pageFlg.used = 1;
    node->pageFlg.sizeType = sizeType;                      /* 初始化 pageFlg */
    node->pageFlg.poolType = poolType;
    node->bucketIdx = bucketIdx;
}

void Miss(Arch *arch, Node **dst, uint32_t pno, uint32_t psize, int fd, uint32_t bucketIdx, uint8_t isW) {
    Node *node = NULL;
    Pool *pool = &arch->pool;
    uint8_t sizeType = size2Idx(psize);
    // uint64_t offset = GetPageOffset(pno, sizeType);
    uint32_t fetchSize = pool->blkSize;
    if (bucketIdx == pagePart[CACHE_IDX][sizeType] && pagePart[LAST_NUM][sizeType] != 0) {
        fetchSize = pagePart[LAST_NUM][sizeType] * pagePart[E_PAGE_SIZE_][sizeType];
    }
    // g_lruMutex.lock();
   
    if (arch->pool.unusedCnt > 0) {
        GetNodeFromMemPool(&node, pool);
    } else {
        node = list_entry(arch->rep.head.prev, Node, lru);
        list_del(&node->lru);
        hlist_del(&node->hash);
        if (node->pageFlg.dirty) {
            arch->bkt.fetched++;
            node->pageFlg.dirty = 0;
            WriteBack(node, fetchSize, fd);
        }
        arch->rep.lruSize--;
    }
    arch->rep.lruSize++;                                         /* 更新 lru 实际长度 */
    InitNodePara(node, sizeType - 1, pool->poolType, bucketIdx);
    INIT_HLIST_NODE(&node->hash);
    node->page_start = pno - ((pno - pagePart[PAGE_PREFIX_SUM][sizeType]) % pagePart[BLCOK_PAGE_COUNT][sizeType]);
    node->blkSize = fetchSize;
    hlist_add_head(&node->hash, &(arch->bkt.bkt[bucketIdx].hhead)); /* 加入到对应的 hash 桶中 */
    list_add(&node->lru, &arch->rep.head);                       
    Fetch(node, node->page_start, fetchSize, GetPageOffset(node->page_start, sizeType), fd);
    *dst = node;
    if (isW) {
        (*dst)->pageFlg.dirty = 1;
    }
    // g_lruMutex.unlock();
}

Node *HashBucketFind(Arch *arch, uint32_t pno, uint32_t psize, int fd, uint8_t isW)
{
    // g_lruMutex.lock();
    HashBucket *hashBucket = &arch->bkt;
    uint32_t bucketIdx = GetExpectBucketIdx(pno, psize);
    Node *newNode = NULL;
    // g_lock.lock();
    Node *dst = NULL;
    if (!hlist_empty(&hashBucket->bkt[bucketIdx].hhead)) { /* have nodes */
#if DEBUG
        // printf("[%d] \t Yes! \t size = %d \t ratio = %.2f \t Fetched = %d \t unusedCnt = %d  \t usedCnt = %d\n", 
        //     pno, arch->rep.lruSize, arch->bkt.hit * 1.0 / (arch->bkt.hit + arch->bkt.miss), 
        //     arch->bkt.fetched, arch->pool.unusedCnt, arch->pool.usedCnt);
#endif 
        arch->bkt.hit++;
        dst = hlist_entry(hashBucket->bkt[bucketIdx].hhead.first, Node, hash);
        if (isW) {
            dst->pageFlg.dirty = 1;
            hashBucket->hitWrite++;
        }
        // if (dst->pageFlg.dirty && isW) {
        //     printf("pno = %X, fd = %d\n", pno, fd);
        //     uint8_t *ptr = (uint8_t*)dst->blk;
        //     for (int i = 0; i < 5; ++i) {
        //         printf("[%08X] ", *(uint8_t*)(ptr + i));
        //     }puts("");
        // }
        list_del(&(dst->lru));
        list_add(&(dst->lru), &arch->rep.head);
        return dst;
    } else {
        // printf("[%d] \t No! \t size = %d \t ratio = %.2f \t Fetched = %d \t unusedCnt = %d  \t usedCnt = %d\n", 
            // pno, arch->rep.lruSize, arch->bkt.hit * 1.0 / (arch->bkt.hit + arch->bkt.miss), 
            // arch->bkt.fetched, arch->pool.unusedCnt, arch->pool.usedCnt);
        hashBucket->miss++;
        Miss(arch, &dst, pno, psize, fd, bucketIdx, isW);
    }
    // g_lruMutex.unlock();
    // printf("[%d]-[%d]:page_start = %d\n", fd, pno, dst->page_start);
    assert(dst);
    // g_lock.unlock();
    return dst;
}

