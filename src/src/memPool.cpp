/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 14:05:34
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-20 23:47:18
 * @FilePath: /src/src/memPool.cpp
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <thread>
#include <sys/mman.h>
#include "const.h"
#include "errcode.h"
#include "memPool.h"
#include "disk.h"

extern uint64_t pagePart[PART_CNT][PS_CNT];
extern uint64_t realMemStat;
extern uint64_t usefulMemStat;
// extern std::atomic_uint_fast32_t runCnt;

/* prevent lazy operation */
void *MallocZero(uint32_t size)
{
    uint8_t *ret = (uint8_t *)malloc(size);
    assert(ret);
    memset(ret, 0, size);
    mlock(ret, size);
    return (void *)ret;
}

void InitLfuNode(LFUNode *node) {
    node->head = NULL;
    INIT_LIST_HEAD(&node->hnode);
}

uint8_t InitPool(Pool *pool, uint32_t pageFlg, uint32_t size)
{
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
        InitLfuNode(&node->lfu);
        // INIT_LIST_HEAD(&node->lru);
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
    // while (runCnt.load()) {
    //     printf("False, runCnt = %d\n", runCnt.load());
    //     std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // }
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
    printf("dirtyCnt = [%d]\n", dirtyCnt);
    return ROK;
}




