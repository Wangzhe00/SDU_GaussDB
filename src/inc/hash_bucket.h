/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-06 15:41:37
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-12 13:28:43
 * @FilePath: \sftp\src\inc\hash_bucket.h
 */
#ifndef HASH_BUCKET_H
#define HASH_BUCKET_H

#include <map>
#include <unordered_map>
#include "baseStruct.h"

uint8_t InitHashBucket(HashBucket *hashBucket, uint32_t _size);
uint8_t HashBucketFind(Arch *arch, Node *dst, uint32_t pno, uint32_t psize, uint32_t &bucketIdx);
uint8_t HashBucketMiss(Arch *arch, Node *dst, uint32_t pno, uint32_t psize, uint32_t &bucketIdx, int fd);

#endif /* HASH_BUCKET_H */