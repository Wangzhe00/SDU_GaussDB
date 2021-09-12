/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 20:12:25
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-12 17:32:04
 * @FilePath: \src\src\hash_bucket.cpp
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
#include "hash_bucket.h"
#include "disk.h"


uint64_t pagePart[PART_CNT][PS_CNT];
std::unordered_map<uint32_t, uint8_t> size2Idx;

uint8_t InitHashBucket(HashBucket *hashBucket, uint32_t _size)
{
    hashBucket->size = _size;
    hashBucket->capSize = sizeof(BucketNode) * _size;
    hashBucket->miss = 0;
    hashBucket->query = 0;
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
    uint8_t pIdx = size2Idx[psize];
    return pagePart[0][pIdx - 1] + (pno - pagePart[1][pIdx - 1]) / pagePart[2][pIdx];
}

uint8_t HashBucketFind(Arch *arch, Node *dst, uint32_t pno, uint32_t psize, uint32_t &bucketIdx)
{
    assert(arch);
    HashBucket *hashBucket = &arch->bkt;
    uint32_t expectBktIdx = GetExpectBucketIdx(pno, psize);
    bucketIdx = expectBktIdx % hashBucket->size;
    Node *tar = NULL, *pos = NULL;
    if (!hlist_empty(&hashBucket->bkt[bucketIdx].hhead)) { /* have nodes */
        hlist_for_each_entry(pos, &hashBucket->bkt[bucketIdx].hhead, hash) {
            if (pos->pageFlg.layer == expectBktIdx / bucketIdx) {
                tar = pos;
                break;
            }
        }
        if (tar != NULL) {   /* find! 类似于LRU，保证热度递减，即刚访问的节点在链表首位 */
            // hlist_del(&tar->hash);
            // hlist_add_head(&tar->hash, &hashBucket->bkt[bucketIdx].hlist);
            dst = tar;
            assert(arch->rep.hit(dst, &arch->bkt, &arch->pool));
            return ROK;
        }
    }
    /* next fetch cache line from disk */
    return ERR;
}

static inline void InitNodePara(Node *node, uint8_t sizeType, uint8_t poolType, uint32_t bucketIdx)
{
    memset(&node->pageFlg, 0, sizeof(node->pageFlg));
    node->pageFlg.sizeType = sizeType;                      /* 初始化 pageFlg */
    node->pageFlg.poolType = poolType;
    node->bucketIdx = bucketIdx;
}

uint8_t HashBucketMiss(Arch *arch, Node *dst, uint32_t pno, uint32_t psize, uint32_t &bucketIdx, int fd)
{
    Node *node = NULL;
    Pool *pool = &arch->pool;
    uint8_t sizeType = size2Idx[psize];
    uint64_t offset = GetPageOffset(pno, sizeType);
    uint32_t fetchSize = pool->blkSize;
    if (bucketIdx == pagePart[CACHE_IDX][sizeType] && pagePart[LAST_NUM][sizeType] != 0) {
        fetchSize = pagePart[LAST_NUM][sizeType] * pagePart[PAGE_SIZE][sizeType];
    }
    /**
     * NOTE: 如何处理极限情况，RExpLen = 0，FExpLen = size
     *           初步分析： 与 gR hit 情况类似，也是需要扩大R，缩小F，但是概率极低，暂不考虑
    */
    assert(arch->rep.RExpLen != 0);
    /* lock begin */

    if (arch->rep.RRealLen < arch->rep.RExpLen) {                   /* 从 unused 池子里拿Node */
        assert(pool->unusedCnt > 0);
        pool->unusedCnt--;
        pool->usedCnt++;
        node = list_entry(pool->unused.next, Node, memP);      /* 获取 Node 头地址 */
        list_del(pool->unused.next);                           /* 将 Node 从 unused 池子移动到 used 池子 */
        list_add(&node->memP, &pool->used);
        InitNodePara(node, sizeType - 1, pool->poolType, bucketIdx);
        node->page_start = pno - ((pno - pagePart[PAGE_PREFIX_SUM][sizeType]) % pagePart[BLCOK_PAGE_COUNT][sizeType]);
        hlist_add_head(&node->hash, &arch->bkt.bkt[bucketIdx].hhead); /* 加入到对应的 hash 桶中 */
        list_add(&node->arc.lru, &arch->rep.R);                       /* 加入到 R 中 */
        arch->rep.RRealLen++;                                         /* 更新 R 实际长度 */
        Fetch(node, node->page_start, fetchSize, offset, fd);
    } else {
        /* ARC 专用， 根据不同的置换算法去变换 */
        WriteBack(list_entry(arch->rep.R.prev, Node, arc), fetchSize, fd);
        arch->rep.LRU_GhostShrink();
        arch->rep.LRU_Shrink(&arch->bkt);
        node = list_entry(&arch->rep.R.next, Node, arc);          /* R.next 节点已经放入 gR 中 */
        InitNodePara(node, sizeType - 1, pool->poolType, bucketIdx);
        node->page_start = pno - ((pno - pagePart[1][sizeType]) % pagePart[2][sizeType]);
        Fetch(node, node->page_start, fetchSize, offset, fd);
    }
    /* lock end */
}