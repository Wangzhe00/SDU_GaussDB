/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 14:05:34
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-06 19:20:14
 * @FilePath: \gauss\src\memPool.cpp
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include "const.h"
#include "list.h"
#include "errcode.h"
#include "memPool.h"

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

uint8_t InitPool(Pool *pool, uint8_t pageType, uint32_t size)
{
    pool->size = size;
    pool->capacity = (pageType == 8) ? size * POOL_CHUNK_BLOCK : size * POOL_SMALL_BLOCK;
    INIT_LIST_HEAD(&pool->unused.memP);
    INIT_LIST_HEAD(&pool->used.memP);

    for (uint32_t i = 0; i < size; ++i) {
        Node *node = (Node *)malloc(sizeof(Node));
        node->pageType = pageType;
        node->blk = MallocZero(ps << 13);
        INIT_HLIST_NODE(&node->hash);
        INIT_LIST_HEAD(&node->arc.lru);
        list_add(&node->memP, &pool->unused.memP);
    }
    return ROK;
}

uint8_t DeInitPool(Pool *pool, uint8_t ps, uint32_t size)
{
    Node *pos, *next, *t;

    list_for_each_entry_safe(pos, next, &pool->unused.memP, memP) {
        t = pos;
        list_del(&pos->memP);
        free(t);
    }
    return ROK;
}




