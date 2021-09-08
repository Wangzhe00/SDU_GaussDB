/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-06 13:23:02
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-06 16:39:14
 * @FilePath: \gauss\src\replacement.cpp
 */
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <map>

#include "errcode.h"
#include "const.h"
#include "list.h"
#include "memPool.h"
#include "replacement.h"

uint8_t InitARC(ARC *arc, uint32_t size)
{
    GhostNode *gNode = NULL;
    uint32_t lruSize = size * ARC_LRU_RATIO;
    uint32_t lfuSize = size - lruSize;
    arc->RSize = lruSize;
    arc->FSize = lfuSize;
    arc->gRSize = lruSize;
    arc->gFSize = lfuSize;
    arc->RCnt = 0;
    arc->FCnt = 0;
    arc->gRCnt = 0;
    arc->gFCnt = 0;
    INIT_LIST_HEAD(&arc->R);
    INIT_LIST_HEAD(&arc->gR);
    INIT_LIST_HEAD(&arc->F);
    INIT_LIST_HEAD(&arc->gF);
    for (uint32_t i = 0; i < lruSize; ++i) {
        gNode = (GhostNode *)malloc(sizeof(GhostNode));
        INIT_LIST_HEAD(&gNode->list);
        list_add(&gNode->list, &arc->gR);
    }
    for (uint32_t i = 0; i < lfuSize; ++i) {
        gNode = (GhostNode *)malloc(sizeof(GhostNode));
        INIT_LIST_HEAD(&gNode->list);
        list_add(&gNode->list, &arc->gF);
    }
    return ROK;
}


