/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-06 15:41:37
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-11 17:10:06
 * @FilePath: \src\inc\hash_bucket.h
 */
#ifndef HASH_BUCKET_H
#define HASH_BUCKET_H

#include <stdint.h>
#include <map>

#include "list.h"
#include "memPool.h"
#include "replacement.h"

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
    uint32_t query;
    BucketNode *bkt;
} HashBucket;

typedef struct {
    Pool pool;
    HashBucket bkt;
    Rep rep;
} Arch;


uint8_t InitHashBucket(HashBucket *hashBucket, uint32_t size, std::map<size_t, size_t> page_no_info);
uint8_t HashBucketFind(HashBucket *hashBucket, Node *dst, uint32_t pno, uint32_t psize);

extern uint64_t pagePart[5][PS_CNT];
extern std::unordered_map<uint32_t, uint8_t> size2Idx;

#endif /* HASH_BUCKET_H */