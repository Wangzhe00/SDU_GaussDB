/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 20:12:25
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-05 23:11:40
 * @FilePath: \gauss\src\hash.cpp
 */
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <map>

#include "errcode.h"
#include "const.h"
#include "list.h"
#include "memPool.h"

typedef struct {
    // uint16_t size;            /* can del */
    struct hlist_head hlist;
// TODO:
    /* add lock or mutex */
    /* add stat ... e.g. hits, timestamp */
} BucketNode;

typedef struct {
    uint32_t size;
    uint32_t capSize;
    BucketNode *bkt;
} HashBucket;

uint32_t pagePart[4][PS_CNT];
unordered_map<uint32_t, uint8_t> size2Idx;

static inline int GetPageType(uint32_t pno)
{
    if ()
}

uint8_t InitHashBucket(HashBucket *hashBucket, uint32_t size, map<size_t, size_t> page_no_info)
{
    hashBucket->size = size;
    hashBucket->capSize = sizeof(BucketNode) * size;
    hashBucket->bkt = (BucketNode *)malloc(hashBucket->capSize);
    assert(hashBucket->bkt);
    for (uint32_t i = 0; i < size; ++i) {
        hashBucket->bkt[i].size = 0;                /* can del */
        HLIST_HEAD_INIT(&hashBucket->bkt[i].hlist);
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
    return pagePart[0][pIdx] + (pno - pagePart[1][pIdx]) / pagePart[2][idx];
}


/**
 * TODO: hit后，直接返回Node，另一个线程专门去处理hit逻辑，榨干CPU
 * 
 **/
uint8_t HashBucketFind(HashBucket *hashBucket, Node *dst, uint32_t pno, uint32_t psize)
{
    assert(hashBucket);
    uint32_t expectBktIdx = GetExpectBucketIdx(pno, psize);
    uint32_t bucketIdx = expectBktIdx % hashBucket->size;
    Node *tar = NULL, *pos;
    if (!hlist_empty(hashBucket->bkt[bucketIdx].hlist)) { /* have nodes */
        hlist_for_each_entry(pos, hashBucket->bkt[bucketIdx].hlist, hlist) {
            if (pos->pageFlg.layer == expectBktIdx / bucketIdx) {   /* deep = 1 */
                tar = pos;
                break;
            }
        }
        
        if (tar != NULL) {   /* find! 类似于LRU，保证热度递减，即刚访问的节点在链表首位 */
            // hlist_del(tar->hash);
            // hlist_add_head(&tar->hash, &hashBucket->bkt[bucketIdx].hlist);
            if (tar->pageFlg.isG) {

            } else {
                dst = tar;
                // TODO: go ARC hit
                return ROK;
            }
        }
    }
    return ERR;
}



