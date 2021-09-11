/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-11 17:05:44
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-11 17:12:21
 * @FilePath: \src\src\disk.cpp
 */
#include <sys/types.h>
#include <unistd.h>
#include "disk.h"


void Fetch(Node *node, uint32_t pno, uint32_t fetchSize, uint64_t offset, int fd)
{
    lseek(fd, offset, SEEK_SET);
    size_t ret = read(fd, node->blk, fetchSize);
    assert(ret == fetchSize);
}

void WriteBack(Node *node, uint32_t fetchSize, int fd)
{
    if (!node->pageFlg.dirty) {
        return ;
    }
    uint64_t offset = GetPageOffset(node->page_start, node->pageFlg.sizeType + 1); /* [0:3] -> [1:4] */
    lseek(fd, offset, SEEK_SET);
    size_t ret = write(fd, node->blk, fetchSize);
    assert(ret == fetchSize);
}