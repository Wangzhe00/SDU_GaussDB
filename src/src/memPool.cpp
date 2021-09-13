/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 14:05:34
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-09-13 11:07:49
 * @FilePath: \sftp\src\src\memPool.cpp
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include "const.h"
#include "errcode.h"
#include "memPool.h"
#include "disk.h"

void *Malloc(uint32_t size)
{
    void *ret = malloc(size);
    assert(ret);
    return ret;
}

/* prevent lazy operation */
void *MallocZero(uint32_t size)
{
    uint8_t *ret = (uint8_t *)malloc(size);
    assert(ret);
    mlock(ret, size);
    return (void *)ret;
}


uint8_t InitPool(Pool *pool, uint32_t pageFlg, uint32_t size)
{
    printf("Init memory pool, pageFlg = %d, size = %d\n", pageFlg, size);
    uint32_t blockSize = GET_LEFT_BIT(32, 4, pageFlg) ? POOL_LARGE_BLOCK : POOL_SMALL_BLOCK;
    pool->size = size;
    pool->capacity = GET_LEFT_BIT(32, 4, pageFlg) ? size * POOL_LARGE_BLOCK : size * POOL_SMALL_BLOCK;
    pool->blkSize = GET_LEFT_BIT(32, 4, pageFlg) ? POOL_LARGE_BLOCK : POOL_SMALL_BLOCK;
    pool->usedCnt = 0;
    pool->unusedCnt = size;
    pool->poolType = (GET_LEFT_BIT(32, 4, pageFlg) > 0);
    INIT_LIST_HEAD(&pool->used);
    INIT_LIST_HEAD(&pool->unused);

    for (uint32_t i = 0; i < size; ++i) {
        Node *node = (Node *)malloc(sizeof(Node));
        memset(node, 0, sizeof(Node));
        node->pageFlg.poolType = pool->poolType;
        node->pageFlg.used = 1;        
        node->blk = MallocZero(blockSize);
        INIT_HLIST_NODE(&node->hash);
        INIT_LIST_HEAD(&node->lru);
        INIT_LIST_HEAD(&node->memP);
        list_add(&node->memP, &pool->unused);
    }
    return ROK;
}

uint8_t DeInitPool(Pool *pool, int fd)
{
    Node *pos, *next, *t;
    int dirtyCnt = 0;
    list_for_each_entry_safe(pos, next, &pool->used, memP) {
        t = pos;
        pool->usedCnt--;
        list_del(&pos->memP);
        if (t->pageFlg.dirty) {
            WriteBack(t, t->blkSize, fd);
            dirtyCnt++;
        }
        free(t->blk);
        free(t);
    }
    printf("dirtyCnt = %d\n", dirtyCnt);
    return ROK;
}




