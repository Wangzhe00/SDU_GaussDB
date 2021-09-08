/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 20:12:25
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-07 22:37:53
 * @FilePath: \gauss\src\hash_bucket.cpp
 */
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <map>
#include <unordered_map>

#include "errcode.h"
#include "const.h"
#include "list.h"
#include "memPool.h"
#include "hash_bucket.h"


uint32_t pagePart[4][PS_CNT];
std::unordered_map<uint32_t, uint8_t> size2Idx;

uint8_t InitHashBucket(HashBucket *hashBucket, uint32_t _size)
{
    

    hashBucket->size = _size;
    hashBucket->capSize = sizeof(BucketNode) * _size;
    hashBucket->bkt = (BucketNode *)malloc(hashBucket->capSize);
    assert(hashBucket->bkt);
    for (uint32_t i = 0; i < _size; ++i) {
        // hashBucket->bkt[i].size = 0;                /* can del */
        INIT_HLIST_HEAD(&hashBucket->bkt[i].hlist);
    }
    return ROK;
}

/* idx may exceed the size of bucket */
static inline uint32_t GetExpectBucketIdx(uint32_t pno, uint32_t psize)
{
    if (psize == PS_2M) {
        return (pno - pagePart[1][3]) / CHUNK_CACHE_SIZE;
    }
    uint8_t pIdx = size2Idx[psize];
    return pagePart[0][pIdx - 1] + (pno - pagePart[1][pIdx - 1]) / pagePart[2][pIdx];
}

uint8_t HashBucketFind(HashBucket *hashBucket, Node *dst, uint32_t pno, uint32_t psize)
{
    assert(hashBucket);
    uint32_t expectBktIdx = GetExpectBucketIdx(pno, psize);
    uint32_t bucketIdx = expectBktIdx % hashBucket->size;
    Node *tar = NULL, *pos = NULL;
    if (!hlist_empty(&hashBucket->bkt[bucketIdx].hlist)) { /* have nodes */
        hlist_for_each_entry(pos, &hashBucket->bkt[bucketIdx].hlist, hash) {
            if (pos->pageFlg.layer == expectBktIdx / bucketIdx) {
                tar = pos;
                break;
            }
        }
        if (tar != NULL) {   /* find! 类似于LRU，保证热度递减，即刚访问的节点在链表首位 */
            hlist_del(&tar->hash);
            hlist_add_head(&tar->hash, &hashBucket->bkt[bucketIdx].hlist);
            dst = tar;
            // TODO: go ARC hit
            return ROK;
        }
    }
    /* next fetch cache line from disk */
    return ERR;
}



