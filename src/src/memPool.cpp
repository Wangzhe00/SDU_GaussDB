/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 14:05:34
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-16 21:07:42
 * @FilePath: /src/src/memPool.cpp
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

extern uint64_t pagePart[PART_CNT][PS_CNT];
extern uint64_t realMemStat;
extern uint64_t usefulMemStat;

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
    uint8_t isLarge = (GET_LEFT_BIT(32, 4, pageFlg) > 0);
    uint32_t blockSize = isLarge ? POOL_LARGE_BLOCK : POOL_SMALL_BLOCK;
    pool->size = size;
    pool->capacity = isLarge ? size * 1ll * POOL_LARGE_BLOCK : size * 1ll *  POOL_SMALL_BLOCK;
    pool->blkSize = isLarge ? POOL_LARGE_BLOCK : POOL_SMALL_BLOCK;
    pool->usedCnt = 0;
    pool->unusedCnt = size;
    pool->poolType = isLarge;
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
    realMemStat += (sizeof(Node) + blockSize) * 1ll * size;
    usefulMemStat += blockSize * 1ll * size;
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
            WriteBack(t, (uint32_t)pagePart[E_PAGE_SIZE_][t->pageFlg.sizeType + 1], fd);
            dirtyCnt++;
        }
        free(t->blk);
        free(t);
    }
    printf("dirtyCnt = %d\n", dirtyCnt);
    return ROK;
}




