/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-11 17:05:55
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-16 20:39:13
 * @FilePath: /src/inc/disk.h
 */
#ifndef DISK_H
#define DISK_H
#include "baseStruct.h"

inline uint64_t GetPageOffset(uint32_t pno, uint8_t sizeType);
void Fetch(Node *node, uint32_t fetchSize, uint64_t offset, int fd);
void WriteBack(Node *node, uint32_t fetchSize, int fd);

#endif /* DISK_H */