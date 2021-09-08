/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-06 13:23:02
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-08 23:28:56
 * @FilePath: \src\src\replacement.cpp
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

enum {
    LRU = 0,
    LFU = 1,
};


ARC::ARC(uint32_t size, uint8_t flg) {
    LFUHead *lfuHead = NULL;
    uint32_t lruLen = size * ARC_LRU_RATIO;
    uint32_t lfuSize = size - lruSize;
    this->RRealLen = this->gRRealLen = this->FRealLen = this->gFRealLen = 0;
    this->RExpLen = this->gRExpLen = lruLen;
    this->FExpLen = this->gFExpLen = lfuSize;
    INIT_LIST_HEAD(this->R);
    INIT_LIST_HEAD(this->gR);
    INIT_LIST_HEAD(this->lfuHeadUsedPool);
    INIT_LIST_HEAD(this->lfuHeadUnusedPool);
    uint32_t lfuHeadPoolSize = flg ? (HASH_BUCKET_CHUNK_SIZE >> 2) : (HASH_BUCKET_SMALL_SIZE >> 2);
    for (int i = 0; i < lfuHeadPoolSize; ++i) {
        lfuHead = (LFUHead *)malloc(sizeof(LFUHead));
        lfuHead->freq = 0;
        lfuHead->len = 0;
        INIT_LIST_HEAD(&lfuHead->pool);
        INIT_HLIST_NODE(&lfuHead->hhead);
        list_add(lfuHead, &this->lfuHeadUnusedPool);
    }
}
    
void ARC::InitRep(uint32_t size, uint8_t flg) {
    LFUHead *lfuHead = NULL;
    uint32_t lruLen = size * ARC_LRU_RATIO;
    uint32_t lfuSize = size - lruSize;
    this->RRealLen = this->gRRealLen = this->FRealLen = this->gFRealLen = 0;
    this->RExpLen = this->gRExpLen = lruLen;
    this->FExpLen = this->gFExpLen = lfuSize;
    INIT_LIST_HEAD(this->R);
    INIT_LIST_HEAD(this->gR);
    INIT_LIST_HEAD(this->lfuHeadUsedPool);
    INIT_LIST_HEAD(this->lfuHeadUnusedPool);
    uint32_t lfuHeadPoolSize = flg ? (HASH_BUCKET_CHUNK_SIZE >> 2) : (HASH_BUCKET_SMALL_SIZE >> 2);
    for (int i = 0; i < lfuHeadPoolSize; ++i) {
        lfuHead = (LFUHead *)malloc(sizeof(LFUHead));
        lfuHead->freq = 0;
        lfuHead->len = 0;
        INIT_LIST_HEAD(&lfuHead->pool);
        INIT_HLIST_NODE(&lfuHead->hhead);
        list_add(lfuHead, &this->lfuHeadUnusedPool);
    }
}
    
/**
 * @description: 清洗 LFU 的头结点，放入 unused 池子中
 * @param {LFUHead} *lfuHead
 * @return {*}
 * @Date: 2021-09-08 21:44:17
 */
void ARC::LFUHeaderClear(LFUHead *lfuHead) {
    lfuHead->freq = 0;
    lfuHead->len = 0;
    list_del(&lfuHead->list);                                 /* 将当前头结点从F中删除 */
    list_del(&lfuHead->pool);                                 /* 将当前头结点从 used 池移除 */
    list_add(&lfuHead->pool, &this->lfuHeadUnusedPool);       /* 将当前头结点加入 unused 池中 */
    INIT_LIST_HEAD(&lfuHead->list);
    INIT_HLIST_HEAD(&lfuHead->hhead);
}

/**
 * @description: 命中 ARC中 LFU 链表实现
 * @param {Node} *node Cache Line Node
 * @return {*}
 * @Date: 2021-09-08 21:40:28
 */
void ARC::ARC_FHit(Node *node) {
    assert(node);
    LFUHead *lfuHead  = node->arc.lfu.head;
    LFUHead *nextHead = NULL;
    LFUHead *newHead  = NULL;

    if (lfuHead->len == 1) {                                          /* 当前lfu头结点只有一个node */
        if (lfuHead->list.next == NULL) {                             /* 当前头结点的频数最高，直接频数加一 */
            lfuHead->freq++;
        } else if (lfuHead->list.next->freq - lfuHead->freq > 1) {    /* 下一个频数比当前节点频数至少大2，直接频数加一 */
            lfuHead->freq++;
        } else {                                                      /* 需要将节点移到下一个频数中，将此头结点移入缓冲池 */
            nextHead = list_entry(lfuHead->list.next, LFUHead, list);
            node->arc.lfu.head = nextHead;                            /* 修改节点的lfu头指针，指向新的 */
            hlist_add_head(&node->arc.lfu.hnode, &nextHead->hhead);   /* 更新节点的hash指针，即将从当前桶移入后一个桶中 */
            LFUHeaderClear(lfuHead);
            nextHead->len++;
        }
    } else {
        hlist_del(node->arc.lfu.hnode);                               /* 从当前lfu桶中删除节点 */
        lfuHead->len--;
        if (lfuHead->list.next != NULL) {
            nextHead = list_entry(lfuHead->list.next, LFUHead, list);
            if (nextHead->freq - lfuHead->freq == 1) {
                node->arc.lfu.head = nextHead;                          /* 修改节点的lfu头指针，指向新的 */
                hlist_add_head(&node->arc.lfu.hnode, &nextHead->hhead); /* 更新节点的hash指针，即将从当前桶移入后一个桶中 */
                nextHead->len++;
                return ;
            }
        }
        /* 能到这的只有两种情况，要么没有next头结点，要么next头结点的频数比lfuHead频数至少大2 */
        newHead = list_entry(this->lfuHeadUnusedPool.next, LFUHead, pool); /* 根据成员拿到头指针 */
        list_del(&this->lfuHeadUnusedPool.next);                           /* 将当前头结点从 unused 池移除 */
        newHead->freq = lfuHead->freq + 1;                                 /* 更新频数 */
        newHead->len = 1;                                                  /* 初始化lfu桶的长度 */
        list_add(&newHead->list, &lfuHead->list);                          /* 将当前头结点插入到F的桶中，即插入到 lfuHead 之后 */
        list_add(&newHead->pool, &this->lfuHeadUsedPool);                  /* 将当前头结点加入 used 池中 */
        hlist_add_head(&node->arc.lfu.hnode, &newHead->hhead);             /* 更新节点的hash指针，即将从当前桶移入后一个桶中 */
        node->arc.lfu.head = newHead;                                      /* 修改节点的lfu头指针，指向新的 */
    }
}

uint8_t ARC::hit(Node *node) {
    assert(node);
    if (node->pageFlg.isG) {
        if (node->pageFlg.rep == LRU) {
            ARC_gRHit(node);
        } else {
            ARC_gFHit(node);
        }
    } else {
        if (node->pageFlg.rep == LRU) {
            ARC_RHit(node);
        } else {
            ARC_FHit(node);
        }
    }
    return ROK;
}



Rep::Rep(uint32_t size): ARC(size) {

}

Rep::~Rep(uint32_t size) {

}


