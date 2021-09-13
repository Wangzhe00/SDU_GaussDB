/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-06 15:41:37
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-13 19:38:06
 * @FilePath: /src/inc/hash_bucket.h
 */
#ifndef HASH_BUCKET_H
#define HASH_BUCKET_H

#include <map>
#include <unordered_map>
#include "baseStruct.h"

uint8_t InitHashBucket(HashBucket *hashBucket, uint32_t _size);
Node *HashBucketFind(Arch *arch, uint32_t pno, uint32_t psize, int fd, uint8_t isW);

#endif /* HASH_BUCKET_H */