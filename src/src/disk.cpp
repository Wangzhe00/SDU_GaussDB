/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-11 17:05:44
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-16 20:42:01
 * @FilePath: /src/src/disk.cpp
 */
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <cstdio>
#include "disk.h"

extern uint64_t pagePart[PART_CNT][PS_CNT];

inline uint64_t GetPageOffset(uint32_t pno, uint8_t sizeType)
{
    return pagePart[SUM_BYTES][sizeType - 1] + (pno - pagePart[PAGE_PREFIX_SUM][sizeType - 1]) * pagePart[E_PAGE_SIZE_][sizeType];
}

void Fetch(Node *node, uint32_t fetchSize, uint64_t offset, int fd)
{
    lseek(fd, offset, SEEK_SET);
    size_t ret = read(fd, node->blk, fetchSize);
    assert(ret == fetchSize);
}

void WriteBack(Node *node, uint32_t fetchSize, int fd)
{
    uint64_t offset = GetPageOffset(node->pageFlg.pno, node->pageFlg.sizeType + 1); /* [0:3] -> [1:4] */
    lseek(fd, offset, SEEK_SET);
    size_t ret = write(fd, node->blk, fetchSize);
    assert(ret == fetchSize);
}