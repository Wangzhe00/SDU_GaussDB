/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 20:12:25
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-17 16:38:39
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

#define DEBUG 1


std::mutex g_mlock; /* 内存池锁 */
std::mutex g_lock;  /* 全局锁 */

uint64_t pagePart[PART_CNT][PS_CNT];
extern uint64_t realMemStat;
extern uint64_t usefulMemStat;

uint8_t InitHashBucket(HashBucket *hashBucket, uint32_t _size)
{
    hashBucket->size = _size;
    hashBucket->capSize = sizeof(BucketNode) * _size;
    hashBucket->miss = 0;
    hashBucket->hit = 0;
    hashBucket->fetched = 0;
    hashBucket->hitWrite = 0;
    hashBucket->bkt = (BucketNode *)malloc(hashBucket->capSize);
    assert(hashBucket->bkt);
    for (uint32_t i = 0; i < _size; ++i) {
        INIT_HLIST_HEAD(&hashBucket->bkt[i].hhead);
    }
    realMemStat += hashBucket->capSize;
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

static inline void InitNodePara(Node *node, uint8_t sizeType, uint8_t poolType, uint32_t pno)
{
    node->pageFlg.sizeType = sizeType;
    node->pageFlg.dirty = 0;
    node->pageFlg.poolType = poolType;
    node->pageFlg.used = 1;
    node->pageFlg.pno = pno;
    INIT_HLIST_NODE(&node->hash);
}

Node *HashBucketFind(Arch *arch, uint32_t pno, uint32_t psize, int fd, uint8_t isW)
{
    HashBucket *hashBucket = &arch->bkt;
    Pool *pool = &arch->pool;
    Node *dst = NULL;  
    Node *node = NULL; /* 临时变量 */
    uint8_t sizeType = size2Idx(psize);
    // std::mutex &nodeLock = hashBucket->bkt[pno].bLock;
    Mutex &nodeLock = hashBucket->bkt[pno].bLock;
    nodeLock.lock();
    while (!g_lock.try_lock()) {
        nodeLock.unlock();
        std::this_thread::yield();
        nodeLock.lock();
        // printf("1111111111   fd = %d, pno = %d, isW = %d\n", fd, pno, isW);
    }
    if (!hlist_empty(&hashBucket->bkt[pno].hhead)) { /* Find! */
        hashBucket->hit++;
        dst = hlist_entry(hashBucket->bkt[pno].hhead.first, Node, hash);
        if (isW) {
            dst->pageFlg.dirty = 1;
            // hashBucket->hitWrite++;
        }
        list_del(&dst->lru);
        list_add(&dst->lru, &arch->rep.head);
        g_lock.unlock();
        return dst;
    }
    g_lock.unlock();
    /* 执行到这里说明当前页号MISS，手上拿着目标桶的锁 */
    hashBucket->miss++;
    
    g_mlock.lock();
    if (pool->unusedCnt > 0) {
        GetNodeFromMemPool(&node, pool);
        g_mlock.unlock();
    } else {
        g_mlock.unlock();
        while (!g_lock.try_lock()) {
            nodeLock.unlock();
            std::this_thread::yield();
            nodeLock.lock();
            // printf("222222222   fd = %d, pno = %d, isW = %d\n", fd, pno, isW);
        }
        node = list_entry(arch->rep.head.prev, Node, lru);

        // std::mutex &oldNodeLock = hashBucket->bkt[node->pageFlg.pno].bLock;
        Mutex &oldNodeLock = hashBucket->bkt[node->pageFlg.pno].bLock;
        // oldNodeLock.lock();  /* 可能和 nodeLock 导致死锁 */
        while (!oldNodeLock.try_lock()) {
            nodeLock.unlock();
            std::this_thread::yield();
            nodeLock.lock();
            // printf("3333333333   fd = %d, pno = %d, isW = %d\n", fd, pno, isW);
        }
        list_del(&node->lru);
        hlist_del(&node->hash);
        g_lock.unlock();
        if (node->pageFlg.dirty) {
            // hashBucket->fetched++;    /* atomic_uint32_t */
            node->pageFlg.dirty = 0;
            WriteBack(node, (uint32_t)pagePart[E_PAGE_SIZE_][node->pageFlg.sizeType + 1], fd);
        }
        oldNodeLock.unlock();
        // arch->rep.lruSize--;
    }
    /* node 不属于LRU，也不属于hashbucket */
    // arch->rep.lruSize++;                                         /* 更新 lru 实际长度 */
    while (!g_lock.try_lock()) {
        nodeLock.unlock();
        std::this_thread::yield();
        nodeLock.lock();
        // printf("444444444   fd = %d, pno = %d, isW = %d\n", fd, pno, isW);
    }
    list_add(&node->lru, &arch->rep.head);   
    g_lock.unlock();

    InitNodePara(node, sizeType - 1, pool->poolType, pno);
    if (isW) {
        node->pageFlg.dirty = 1;
    } else {
        Fetch(node, psize, GetPageOffset(pno, sizeType), fd);
    }
    hlist_add_head(&node->hash, &hashBucket->bkt[pno].hhead); /* 加入到对应的 hash 桶中 */
    /* MISS End */
    return node;
}

