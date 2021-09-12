/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-06 13:23:02
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-12 17:33:23
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
#include "baseStruct.h"
#include "replacement.h"

enum {
    LRU = 0,
    LFU = 1,
};


ARC::ARC() {
    
}
    
void ARC::InitRep(uint32_t size, uint8_t flg) {
    LFUHead *lfuHead = NULL;
    Node *node = NULL;
    uint32_t lruSize = size * ARC_LRU_RATIO;
    uint32_t lfuSize = size - lruSize;
    this->RRealLen = this->gRRealLen = this->FRealLen = this->gFRealLen = 0;
    this->RExpLen = this->gFExpLen = lruSize;
    this->FExpLen = this->gRExpLen = lfuSize;
    INIT_LIST_HEAD(&this->R);
    INIT_LIST_HEAD(&this->gR);
    INIT_LIST_HEAD(&this->gF);
    INIT_LIST_HEAD(&this->lfuHeadUsedPool);
    INIT_LIST_HEAD(&this->lfuHeadUnusedPool);
    INIT_LIST_HEAD(&this->F.list);
    INIT_LIST_HEAD(&this->F.pool);
    INIT_LIST_HEAD(&this->F.hhead);
    F.freq = _LIST_HEAD;
    F.len = 0;
    uint32_t lfuHeadPoolSize = flg ? (HASH_BUCKET_CHUNK_SIZE >> 2) : (HASH_BUCKET_SMALL_SIZE >> 2);
    for (int i = 0; i < lfuHeadPoolSize; ++i) {
        lfuHead = (LFUHead *)malloc(sizeof(LFUHead));
        lfuHead->freq = 0;
        lfuHead->len = 0;
        INIT_LIST_HEAD(&lfuHead->list);
        INIT_LIST_HEAD(&lfuHead->pool);
        INIT_LIST_HEAD(&lfuHead->hhead);
        list_add(&lfuHead->pool, &this->lfuHeadUnusedPool);
    }
    for (int i = 0; i < size; ++i) {
        node = (Node *)malloc(sizeof(Node));
        memset(node, 0, sizeof(Node));
        node->pageFlg.isG = 1;
        node->pageFlg.poolType = flg;
        node->bucketIdx = BUCKET_MAX_IDX;
        if (i < lfuSize) {                       /* 注意前 lfuSize 个是 gR 的长度 */
            list_add(&node->arc.lru, &this->gR);
        } else {
            list_add(&node->arc.gLfu, &this->gF);
        }
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
    INIT_LIST_HEAD(&lfuHead->hhead);
}

/**
 * @description: 从 unused 池子中获取新的节点，并且插入到used池子中
 * @param {*}
 * @return {*}
 * @Date: 2021-09-10 22:43:01
 */
LFUHead *ARC::LFU_GetNewHeadNode() {
    LFUHead *newHead = list_entry(this->lfuHeadUnusedPool.next, LFUHead, pool); /* 根据成员拿到头指针 */
    list_del(this->lfuHeadUnusedPool.next);
    list_add(&newHead->pool, &this->lfuHeadUsedPool);                  /* 将当前头结点加入 used 池中 */
    return newHead;
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
    LFUHead *nextHead = list_entry(lfuHead->list.next, LFUHead, list);
    LFUHead *newHead  = NULL;
    if (lfuHead->len == 1) {                                                /* 当前lfu头结点只有一个node */
        if (nextHead->freq == _LIST_HEAD) {                                 /* 当前头结点的频数最高，即处在链表表尾，直接频数加一 */
            lfuHead->freq++;
        } else {
            if (nextHead->freq - lfuHead->freq > 1) {                       /* 下一个频数比当前节点频数至少大2，直接频数加一 */
                lfuHead->freq++;
            } else {                                                        /* 需要将节点移到下一个频数中，将此头结点移入缓冲池 */
                node->arc.lfu.head = nextHead;                              /* 修改节点的lfu头指针，指向新的 */
                list_add(&node->arc.lfu.hnode, &nextHead->hhead);           /* 更新节点的hash指针，即将从当前链移入后一个链中 */
                LFUHeaderClear(lfuHead);
                nextHead->len++;
            }
        }
    } else {
        list_del(&node->arc.lfu.hnode);                                     /* 从当前lfu桶中删除节点 */
        lfuHead->len--;
        if (nextHead->freq != _LIST_HEAD) {
            if (nextHead->freq - lfuHead->freq == 1) {
                node->arc.lfu.head = nextHead;                              /* 修改节点的lfu头指针，指向新的 */
                list_add(&node->arc.lfu.hnode, &nextHead->hhead);           /* 更新节点的hash指针，即将从当前桶移入后一个桶中 */
                nextHead->len++;
                return ;
            }
        }
        /* 能到这的只有两种情况，当前 lfuHead 要么没有next结点，要么next头结点的频数比lfuHead频数至少大2，均需要插入节点 */
        newHead = LFU_GetNewHeadNode();                                     
        newHead->freq = lfuHead->freq + 1;                                 /* 更新频数 */
        newHead->len = 1;                                                  /* 初始化lfu桶的长度 */
        list_add(&newHead->list, &lfuHead->list);                          /* 将当前头结点插入到F的链表中，即插入到 lfuHead 之后 */
        list_add(&node->arc.lfu.hnode, &newHead->hhead);                   /* 更新节点的hash指针，即将从当前桶移入后一个桶中 */
        node->arc.lfu.head = newHead;                                      /* 修改节点的lfu头指针，指向新的 */
    }
}

/**
 * @description: 找到LFU中频数为 2 的头结点，找不到的话在原地插入一个头结点
 * @param {LFUHead} *freq2
 * @return {*}
 * @Date: 2021-09-10 22:28:21
 */
void ARC::LFU_GetFreq2(LFUHead *freq2) {
    LFUHead *preNode = &this->F;
    LFUHead *firstLfuHead = NULL;
    LFUHead *secondLfuHead = NULL;
    LFUHead *newHead = NULL;

    if (this->FRealLen != 0) {
        firstLfuHead = list_entry(this->F.list.next, LFUHead, list);
        if (firstLfuHead->freq == 1) {       /* 如果首节点频数为1，继续向后查找，找到返回dst，否则插入 */
            if (this->FRealLen > 1) {
                secondLfuHead = list_entry(firstLfuHead->list.next, LFUHead, list);
                if (firstLfuHead->freq == 2) {
                    freq2 = secondLfuHead;
                    return ;
                }
            }
            preNode = firstLfuHead;
        } else if (firstLfuHead->freq == 2) {
            freq2 = firstLfuHead;
            return ;
        } else {  /* > 2 */
            /* preNode = &this->F; */
        }
    } else {
        /* preNode = &this->F; */
    }
    newHead = LFU_GetNewHeadNode();
    newHead->freq = 2;
    newHead->len = 0;
    list_add(&newHead->list, &preNode->list);
    INIT_LIST_HEAD(&newHead->hhead);
}

/**
 * @description: Ghost LFU 缩减一个 key，由于 LRU  [头结点      ->       尾节点] 
 *                                           遵循 [最近一次访问 -> 最远一次访问]
 *               接下来的操作务必是 F 的某个Node插入到 gF 中
 * @param {*}
 * @return {*}
 * @Date: 2021-09-11 13:57:35
 */
void ARC::LFU_GhostShrink() {
    struct list_head *gFTail = this->gF.prev;
    Node *node = NULL;
    list_del(gFTail);
    list_add(gFTail, &this->gF);
    if (this->gFRealLen < this->gFExpLen) {
        this->gRRealLen++;
    } else {                                   /* 释放 hash 映射 */
        node = list_entry(gFTail, Node, arc);
        hlist_del(&node->hash);
    }    
}

void ARC::ReturnMemPool(Node *node, Pool *pool) {
    pool->usedCnt--;
    pool->unusedCnt++;
    list_del(&node->memP);                                /* 从 used 归还至 unused 缓冲池 */
    list_add(&node->memP, &pool->unused);
}

void ARC::GetNodeFromMemPool(Node *node, Pool *pool) {
    pool->usedCnt++;
    pool->unusedCnt--;
    node = list_entry(pool->unused.next, Node, memP);
    list_del(pool->unused.next);
    list_add(&node->memP, pool->used);
}


/**
 * @description: LFU 需要踢出一个Node塞到 ghost LFU中，目前实现的LFU为基础版的LFU，踢出频数最小且最老的节点
 * 
 *               只有两种情况会收缩 LFU，将收缩的node交还给内存池去管理，并且 key 交给 ghost LFU
 *                     1. 当 F 满，且命中 R
 *                     2. 当 F 满，且命中 gR
 * @param {HashBucket} *bkt
 * @param {Pool} *pool
 * @return {*}
 * @Date: 2021-09-11 14:00:10
 */
void ARC::LFU_Shrink(HashBucket *bkt, Pool *pool) {
    assert(this->FRealLen != 0);
    LFUHead *firstLfuHead = list_entry(this->F.list.next, LFUHead, list);    /* 获取 F 的头节点，即频数最小的头节点 */
    struct list_head *goGhostListNode = firstLfuHead->hhead.prev;
    list_del(goGhostListNode);
    LFUNode *goGhostLfuNode = list_entry(goGhostListNode, LFUNode, hnode);   /* 取出最老的lfuNode节点 */
    goGhostLfuNode->head = NULL;
    Node *removeNode = list_entry(goGhostLfuNode, Node, arc);                /* 从F中获取到需要调度出去的Node */
    hlist_del(&removeNode->hash);                                            /* 解除 hash 映射 */
    this->ReturnMemPool(removeNode, pool);
    Node *gFHead = list_entry(this->gF.next, Node, arc);
    gFHead->bucketIdx = removeNode->bucketIdx;
    hlist_add_head(&gFHead->hash, &bkt->bkt[gFHead->bucketIdx].hhead);       /* hash对应的 bucketIdx 绑定到新的 Node */
}

void ARC::ARC_RHit(Node *node, HashBucket* bkt, Pool *pool) {
    assert(node);
    node->pageFlg.rep = LFU;
    list_del(&node->arc.lru);
    this->RRealLen--;
    LFUHead *freq2 = NULL;
    LFUHead *newHead = NULL;
    if (this->FRealLen < this->FExpLen) {
        this->FRealLen++;
    } else {
        this->LFU_GhostShrink();
        this->LFU_Shrink(bkt, pool);
    }
    this->LFU_GetFreq2(freq2);
    freq2->len++;
    node->arc.lfu.head = freq2;                                         /* 修改节点的lfu头指针，指向新的 */
    list_add(&node->arc.lfu.hnode, &freq2->hhead);                      /* 更新节点的hash指针，指向新的LfuHeadNode */
}

void ARC::InitNodePara(Node *node, uint8_t sizeType, uint8_t poolType, uint32_t bucketIdx)
{
    memset(&node->pageFlg, 0, sizeof(node->pageFlg));
    node->pageFlg.sizeType = sizeType;                      /* 初始化 pageFlg */
    node->pageFlg.poolType = poolType;
    node->bucketIdx = bucketIdx;
}

void ARC::FULLMiss(Arch *arch, Node *dst, uint32_t pno, uint32_t psize, uint32_t &bucketIdx, int fd) {
    assert(this->RExpLen != 0);
    uint8_t sizeType = size2Idx[psize];
    uint64_t offset = GetPageOffset(pno, sizeType);
    uint32_t fetchSize = pool->blkSize;
    if (bucketIdx == pagePart[CACHE_IDX][sizeType] && pagePart[LAST_NUM][sizeType] != 0) {
        fetchSize = pagePart[LAST_NUM][sizeType] * pagePart[PAGE_SIZE][sizeType];
    }
    Node *newNode = NULL;
    if (this->RRealLen < this->RExpLen) {
        newNode = GetNodeFromMemPool();
        InitNodePara(newNode, sizeType - 1, pool->poolType, bucketIdx);
        node->page_start = pno - ((pno - pagePart[PAGE_PREFIX_SUM][sizeType]) % pagePart[BLCOK_PAGE_COUNT][sizeType]);
        hlist_add_head(&node->hash, &arch->bkt.bkt[bucketIdx].hhead); /* 加入到对应的 hash 桶中 */
        list_add(&node->arc.lru, &arch->rep.R);                       /* 加入到 R 中 */
        arch->rep.RRealLen++;                                         /* 更新 R 实际长度 */
        Fetch(node, node->page_start, fetchSize, offset, fd);
    } else {
        /* ARC 专用， 根据不同的置换算法去变换 */
        WriteBack(list_entry(arch->rep.R.prev, Node, arc), fetchSize, fd);
        arch->rep.LRU_GhostShrink();
        arch->rep.LRU_Shrink(&arch->bkt);
        node = list_entry(&arch->rep.R.next, Node, arc);          /* R.next 节点已经放入 gR 中 */
        InitNodePara(node, sizeType - 1, pool->poolType, bucketIdx);
        node->page_start = pno - ((pno - pagePart[1][sizeType]) % pagePart[2][sizeType]);
        Fetch(node, node->page_start, fetchSize, offset, fd);
    }                   
}


/**
 * @description: Ghost LRU 缩减一个 key，由于 LRU  [头结点      ->       尾节点] 
 *                                           遵循 [最近一次访问 -> 最远一次访问]
 *               接下来的操作务必是 R 的某个Node插入到 gR 中
 * @param {*}
 * @return {*}
 * @Date: 2021-09-09 22:41:12
 */
void ARC::LRU_GhostShrink() {
    

    if (this->gFExpLen == 1) { /*  */

    }







    struct list_head *tail = this->gR.prev;
    Node *node = NULL;
    list_del(tail);
    list_add(tail, &this->gR);
    if (this->gRRealLen == this->gRExpLen) {      /* ghost 满了，需要擦掉 hash 链表 */
        node = list_entry(tail, Node, arc);
        hlist_del(&node->hash);
    } else {
        this->gRRealLen++;
    }

}

void ARC::LRU_Shrink(HashBucket *bkt) {
    struct list_head *Rtail = this->R.prev;
    Node *RHeadNode = NULL, *gRHeadNode = NULL;
    list_del(Rtail);                                    /* R 上执行 LRU 调度 */
    list_add(Rtail, &this->R);
    RHeadNode = list_entry(Rtail, Node, arc);
    hlist_del(&RHeadNode->hash);                        /* hash断链 */
    gRHeadNode = list_entry(this->gR.next, Node, arc);
    gRHeadNode->bucketIdx = RHeadNode->bucketIdx;       /* TODO: 剩余可能还有一些变量需要更新 */
    hlist_add_head(&gRHeadNode->hash, &bkt->bkt[gRHeadNode->bucketIdx].hhead);  /* hash对应的 bucketIdx 绑定到新的 Node */
}


void ARC::ARC_gRHit(Node *node) {

}

void ARC::ARC_gFHit(Node *node) {

}


uint8_t ARC::hit(Node *node, HashBucket* bkt, Pool *pool) {
    assert(node);
    if (node->pageFlg.isG) {
        if (node->pageFlg.rep == LRU) {
            ARC_gRHit(node);
        } else {
            ARC_gFHit(node);
        }
    } else {
        if (node->pageFlg.rep == LRU) {
            ARC_RHit(node, bkt, pool);
        } else {
            ARC_FHit(node);
        }
    }
    return ROK;
}



