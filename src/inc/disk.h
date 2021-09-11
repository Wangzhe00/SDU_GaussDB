/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-11 17:05:55
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-11 17:07:38
 * @FilePath: \src\inc\disk.h
 */
#ifndef DISK_H
#define DISK_H
#include <stdint.h>
#include "const.h"
#include "list.h"
#include "memPool.h"


extern uint64_t pagePart[5][PS_CNT];

inline uint64_t GetPageOffset(uint32_t pno, uint8_t sizeType)
{
    return pagePart[SUM_BYTES][sizeType - 1] + (pno - pagePart[PAGE_PREFIX_SUM][sizeType - 1]) * pagePart[PAGE_SIZE][sizeType];
}

void Fetch(Node *node, uint32_t pno, uint32_t fetchSize, uint64_t offset, int fd);
void WriteBack(Node *node, uint32_t fetchSize, int fd);

#endif /* DISK_H */