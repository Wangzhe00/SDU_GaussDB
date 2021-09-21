/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 20:12:25
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-20 23:48:16
 * @FilePath: /src/src/hash_bucket.cpp
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <map>
#include <unordered_map>
#include <iostream>
#include <mutex>

#include "errcode.h"
#include "const.h"
#include "hash_bucket.h"
#include "disk.h"

#define DEBUG 1

LFUHead *lfuHash[LFU_HEAD_HASH + 1];

std::mutex g_mlock; /* 内存池锁 */
std::mutex g_lock;  /* 全局锁 */

uint64_t pagePart[PART_CNT][PS_CNT];
extern uint64_t realMemStat;
extern uint64_t usefulMemStat;

extern int cntTest;
// extern std::atomic_uint_fast32_t runCnt;

/**
 * @description: 从 unused 池子中获取新的节点，并且插入到used池子中
 * @param {*}
 * @return {*}
 * @Date: 2021-09-10 22:43:01
 */
LFUHead *LFU_GetNewHeadNode(Arch *arch) {
    LFUHead *newHead = list_entry(arch->lfuHeadUnusedPool.next, LFUHead, pool); /* 根据成员拿到头指针 */
    list_del(arch->lfuHeadUnusedPool.next);
    list_add(&newHead->pool, &arch->lfuHeadUsedPool);                  /* 将当前头结点加入 used 池中 */
    INIT_LIST_HEAD(&newHead->hlist);
    INIT_LIST_HEAD(&newHead->vlist);    
    return newHead;
}

uint8_t InitHashBucket(Arch *arch, uint32_t _size)
{
    HashBucket *hashBucket = &arch->bkt;
    hashBucket->size = _size;
    hashBucket->capSize = sizeof(BucketNode) * _size;
    hashBucket->miss = 0;
    hashBucket->hit = 0;
    hashBucket->fetched = 0;
    hashBucket->hitWrite = 0;
    hashBucket->bkt = (BucketNode *)malloc(hashBucket->capSize);
    assert(hashBucket->bkt);
    memset(hashBucket->bkt, 0, hashBucket->capSize);
    for (uint32_t i = 0; i < _size; ++i) {
        INIT_HLIST_HEAD(&hashBucket->bkt[i].hhead);
    }
    for (size_t i = 0; i < LFU_HEAD_HASH; ++i) {
        lfuHash[i] = NULL;
    }
    /* 初始化频数为1的头结点,加入到横向的链表中 */
    lfuHash[1] = LFU_GetNewHeadNode(arch);
    lfuHash[1]->freq = 1;
    lfuHash[1]->len = 0;
    list_add(&lfuHash[1]->hlist, &arch->lfu.hlist);
    realMemStat += hashBucket->capSize;
    return ROK;
}

/* idx may exceed the size of bucket */
static inline uint32_t GetExpectBucketIdx(uint32_t pno, uint32_t psize)
{
    if (psize == PS_2M) {
        return (pno - pagePart[1][3]) / CHUNK_CACHE_SIZE;
    }
    
    uint8_t pIdx = size2Idx(psize);
    return pagePart[0][pIdx - 1] + (pno - pagePart[1][pIdx - 1]) / pagePart[2][pIdx];
}


static Node *GetNodeFromMemPool(Pool *pool) {
    Node *newNode = NULL;
    pool->usedCnt++;
    pool->unusedCnt--;
    newNode = list_entry(pool->unused.next, Node, memP);
    list_del(pool->unused.next);
    list_add(&newNode->memP, &pool->used);
    INIT_LIST_HEAD(&newNode->lfu.hnode);
    INIT_HLIST_NODE(&newNode->hash);
    newNode->lfu.head = NULL;
    return newNode;
}

static inline void InitNodePara(Node *node, uint8_t sizeType, uint8_t poolType, uint32_t pno)
{
    node->pageFlg.sizeType = sizeType;
    node->pageFlg.dirty = 0;
    node->pageFlg.poolType = poolType;
    node->pageFlg.used = 1;
    node->pageFlg.pno = pno;
    INIT_HLIST_NODE(&node->hash);
    INIT_LIST_HEAD(&node->lfu.hnode);
    node->lfu.head = NULL;
}

void checkVCnt(LFUHead *lfuHead) {
    int cnt = 0;
    LFUNode *pos = list_entry(&lfuHead->vlist, LFUNode, hnode);
    list_for_each_entry(pos, &lfuHead->vlist, hnode) {
        cnt++;
    }
    if (cnt != lfuHead->len) {
        assert(0);
    }
}

/**
 * @description: 清洗 LFU 的头结点，放入 unused 池子中
 * @param {LFUHead} *lfuHead
 * @return {*}
 * @Date: 2021-09-08 21:44:17
 */
void LFUHeaderClear(LFUHead *lfuHead, Arch *arch) {
    lfuHead->freq = 0;
    lfuHead->len = 0;
    list_del(&lfuHead->hlist);                                 /* 将当前头结点从F中删除 */
    /* 可能需要单独的给内存池加锁 begin */
    list_del(&lfuHead->pool);                                 /* 将当前头结点从 used 池移除 */
    list_add(&lfuHead->pool, &arch->lfuHeadUnusedPool);       /* 将当前头结点加入 unused 池中 */
    /* 可能需要单独的给内存池加锁 end */
    INIT_LIST_HEAD(&lfuHead->hlist);
    INIT_LIST_HEAD(&lfuHead->vlist);
}



void LFU_Hit(Node *node, Arch *arch) {
    assert(node);
    LFUHead *lfuHead  = node->lfu.head;
    LFUHead *nextHead = list_entry(lfuHead->hlist.next, LFUHead, hlist);
    LFUHead *newHead  = NULL;
    assert(lfuHead->len > 0);
    // checkVCnt(lfuHead);
    // checkVCnt(nextHead);
    
    if (lfuHead->len == 1) {                                            /* 当前lfu头结点只有一个node */
        if (nextHead->freq == _LIST_HEAD) {                             /* 当前头结点的频数最高，即处在链表表尾，直接频数加一 */
            lfuHead->freq++;
            if (lfuHead->freq - 1 <  LFU_HEAD_HASH) {
                lfuHash[lfuHead->freq - 1] = NULL;
            }
            if (lfuHead->freq <  LFU_HEAD_HASH) {
                lfuHash[lfuHead->freq] = lfuHead;
            }
        } else {
            if (nextHead->freq - lfuHead->freq > 1) {                   /* 下一个频数比当前节点频数至少大2，直接频数加一 */
                lfuHead->freq++;
                if (lfuHead->freq - 1 <  LFU_HEAD_HASH) {
                    lfuHash[lfuHead->freq - 1] = NULL;
                }
                if (lfuHead->freq <  LFU_HEAD_HASH) {
                    lfuHash[lfuHead->freq] = lfuHead;
                }
            } else {                                                    /* 需要将节点移到下一个频数中，将此头结点移入缓冲池 */
                list_del(&node->lfu.hnode);
                lfuHead->len--;
                list_add(&node->lfu.hnode, &nextHead->vlist);           /* 更新节点的hash指针，即将从当前链移入后一个链中 */
                nextHead->len++;
                node->lfu.head = nextHead;                              /* 修改节点的lfu头指针，指向新的 */
                if (lfuHead->freq >= LFU_HEAD_HASH) {
                    LFUHeaderClear(lfuHead, arch);
                }
                // checkVCnt(lfuHead);
                // checkVCnt(nextHead);
            }
        }
    } else {
        list_del(&node->lfu.hnode);     
        lfuHead->len--;
        // checkVCnt(lfuHead);
        if (nextHead->freq != _LIST_HEAD) {
            if (nextHead->freq - lfuHead->freq == 1) {
                list_add(&node->lfu.hnode, &nextHead->vlist);           /* 更新节点的hash指针，即将从当前桶移入后一个桶中 */
                nextHead->len++;
                node->lfu.head = nextHead;                              /* 修改节点的lfu头指针，指向新的 */
                // checkVCnt(nextHead);
                return ;
            }
        }
        /* 能到这的只有两种情况，当前 lfuHead 要么没有next结点，要么next头结点的频数比lfuHead频数至少大2，均需要插入节点 */
        newHead = LFU_GetNewHeadNode(arch);                                     
        newHead->freq = lfuHead->freq + 1;                              /* 更新频数 */
        newHead->len = 1;                                               /* 初始化lfu桶的长度 */
        list_add(&newHead->hlist, &lfuHead->hlist);                     /* 将当前头结点插入到F的链表中，即插入到 lfuHead 之后 */
        list_add(&node->lfu.hnode, &newHead->vlist);                    /* 更新节点的hash指针，即将从当前桶移入后一个桶中 */
        node->lfu.head = newHead;                                       /* 修改节点的lfu头指针，指向新的 */
        assert(lfuHash[newHead->freq] == NULL);
        lfuHash[newHead->freq] = newHead;
        // checkVCnt(newHead);
    }
}

/**
 * @description: LFU拿到频数最小且最老的节点，不会对其修改，
 *               及时后面删除此节点，也不会将LFUHead删除，就算LFUHead.len = 0
 *               因为没必要，一般删除都是频率较低的，不值得去删除LFUHead
 * @param {Arch} *arch
 * @return {*}
 * @Date: 2021-09-17 22:59:45
 */
Node *LFU_Shrink(Arch *arch) {
    LFUHead *firstLfuHead = list_entry(arch->lfu.hlist.next, LFUHead, hlist);   /* 获取 F 的头节点，即频数最小的头节点 */
    while (firstLfuHead->len == 0) {                                            /* 防止拿到的LFU头结点纵向没有Node */
        firstLfuHead = list_entry(firstLfuHead->hlist.next, LFUHead, hlist);
    }
    struct list_head *outListNode = firstLfuHead->vlist.prev;        
    LFUNode *outLfuNode = list_entry(outListNode, LFUNode, hnode);              /* 取出最老的lfuNode节点 */
    return list_entry(outLfuNode, Node, lfu);
}

/**
 * @description: 将指定node插入到指定频数的节点中，直接暴力遍历找预期频数节点，如果找不到则插入恰好大于预期频数的前一个节点
 *               解释下为什么暴力找，因为能被换出后再次换入的一定是不怎么活跃的，频数肯定不会很高，所以理论不会找很多次
 * @param {Node} *node
 * @param {uint16_t} freq
 * @param {Arch} *arch
 * @return {*}
 * @Date: 2021-09-18 00:02:46
 */
void LFU_InsertFreq(Node *node, uint16_t freq, Arch *arch) {
    bool flg = false;
    LFUHead *test = &arch->lfu;
    LFUHead *pos = list_entry(&arch->lfu.hlist, LFUHead, hlist);
    LFUHead *newHead = NULL;
    // checkVCnt(pos);
    if (freq < LFU_HEAD_HASH && lfuHash[freq] != NULL) {
        flg = true;
        pos = lfuHash[freq];
    } else {
        list_for_each_entry(pos, &arch->lfu.hlist, hlist) {
            if (pos->freq == freq) {
                flg = true;
                break;
            } else if (pos->freq > freq) {
                break;
            }
        }
    }
    
    // checkVCnt(pos);
    if (flg) { /* 找到对应频率的lfuHead */
        list_add(&node->lfu.hnode, &pos->vlist);
        node->lfu.head = pos;
        pos->len++;
        // checkVCnt(pos);
    } else {   /* 新建lfuHead节点 */
        newHead = LFU_GetNewHeadNode(arch);
        struct list_head *newHeadAddr = &newHead->vlist;                                     
        newHead->freq = freq;                                 /* 更新频数 */
        newHead->len = 1;                                     /* 初始化lfu桶的长度 */
        list_add_tail(&newHead->hlist, &pos->hlist);          /* 将当前头结点插入到F的链表中，即插入到 lfuHead 之后 */
        list_add(&node->lfu.hnode, &newHead->vlist);          /* 更新节点的hash指针，即将从当前桶移入后一个桶中 */
        node->lfu.head = newHead; 
        assert(lfuHash[newHead->freq] == NULL);
        lfuHash[newHead->freq] = newHead;
        // checkVCnt(newHead);
        cntTest++;
    }
}

#define TRY_LOCK(tar, have) \
    do { \
        while (!tar.try_lock()) { \
            have.unlock(); \
            std::this_thread::yield(); \
            have.lock(); \
        } \
    } while(0)

Node *HashBucketFind(Arch *arch, uint32_t pno, uint32_t psize, int fd, uint8_t isW)
{
    HashBucket *hashBucket = &arch->bkt;
    Pool *pool = &arch->pool;
    Node *dst = NULL;  
    Node *node = NULL; /* 临时变量 */
    uint8_t sizeType = size2Idx(psize);
    if (isW) {
        hashBucket->writeCnt++;
    } else {
        hashBucket->readCnt++;
    }
    // std::mutex &nodeLock = hashBucket->bkt[pno].bLock;
    Mutex &nodeLock = hashBucket->bkt[pno].bLock;
    nodeLock.lock();
    // runCnt++;
    TRY_LOCK(g_lock, nodeLock); // 11+1+1+1+1+
    if (!hlist_empty(&hashBucket->bkt[pno].hhead)) { /* Find! */
        hashBucket->hit++;
        dst = hlist_entry(hashBucket->bkt[pno].hhead.first, Node, hash);
        if (isW) {
            dst->pageFlg.dirty = 1;
            // hashBucket->hitWrite++;
        }
        /* ---- 更新 hit begin ---- */
        LFU_Hit(dst, arch);
        /* ---- 更新 hit end  ---- */
        g_lock.unlock();
        hashBucket->bkt[pno].freq++; /* 记录历史访问次数，根据历史访问插入 */
        return dst;
    }
    g_lock.unlock();
    /* 执行到这里说明当前页号MISS，手上拿着目标桶的锁 */
    hashBucket->miss++;
    
    g_mlock.lock();
    if (pool->unusedCnt > 0) {
        node = GetNodeFromMemPool(pool);
        g_mlock.unlock();
    } else {
        g_mlock.unlock();
        TRY_LOCK(g_lock, nodeLock);  /* 保证在未拿到全局锁之前不会对nodeLock进行任何修改 */
        /* 清洗淘汰节点 begin ---- */
        node = LFU_Shrink(arch);        /* 仅拿到最老的节点指针，不会对其进行任何修改 */
        // std::mutex &oldNodeLock = hashBucket->bkt[node->pageFlg.pno].bLock;
        Mutex &oldNodeLock = hashBucket->bkt[node->pageFlg.pno].bLock;
        TRY_LOCK(oldNodeLock, nodeLock); // +1
        // struct hlist_head *curBucketHead = &hashBucket->bkt[node->pageFlg.pno].hhead;
        list_del(&node->lfu.hnode);
        node->lfu.head->len--;
        hlist_del(&node->hash);
        node->lfu.head = NULL;
        g_lock.unlock();
        /* node 已经完全脱离 LFU 和 hashbucket */
        if (node->pageFlg.dirty) {
            // hashBucket->fetched++;
            node->pageFlg.dirty = 0;
            WriteBack(node, (uint32_t)pagePart[E_PAGE_SIZE_][node->pageFlg.sizeType + 1], fd);
        }
        oldNodeLock.unlock();
        /* 清洗淘汰节点 end ---- */
    }
    /* node 不属于LRU，也不属于hashbucket */
    InitNodePara(node, sizeType - 1, pool->poolType, pno);
    
    if (isW) {
        node->pageFlg.dirty = 1;
    } else {
        Fetch(node, psize, GetPageOffset(pno, sizeType), fd); // +1+1
    }

    TRY_LOCK(g_lock, nodeLock); // +1
    hashBucket->bkt[pno].freq++; /* 记录历史访问次数，根据历史访问插入 */
    /* 将新node插入 req begin ---- */
    // list_add(&node->lru, &arch->rep.head);
    LFU_InsertFreq(node, hashBucket->bkt[pno].freq, arch);
    /* 将新node插入 req end ---- */
    g_lock.unlock();

    hlist_add_head(&node->hash, &hashBucket->bkt[pno].hhead); /* 加入到对应的 hash 桶中 */
    /* MISS End */
    return node;
}

