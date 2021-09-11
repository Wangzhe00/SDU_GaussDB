/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-08 12:57:05
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-11 17:31:03
 * @FilePath: \src\inc\replacement.h
 */
#ifndef REPLACEMENT_H
#define REPLACEMENT_H

class ARC {
public:
    uint32_t RRealLen;
    uint32_t RExpLen;
    uint32_t gRRealLen;
    uint32_t gRExpLen;
    uint32_t FRealLen;
    uint32_t FExpLen;
    uint32_t gFRealLen;
    uint32_t gFExpLen;
    uint32_t lfuUsedPoolSize;
    uint32_t lfuUnusedPoolSize;
    struct list_head R;
    struct list_head gR;
    struct list_head gF;
    LFUHead F;
    struct list_head lfuHeadUsedPool;
    struct list_head lfuHeadUnusedPool;
public:
    ARC(uint32_t size, uint8_t flg);

    void InitRep(uint32_t size, uint8_t flg);
    void LFUHeaderClear(LFUHead *lfuHead)
    void LFU_GetFreq2();
    void ReturnMemPool(Node *node, Pool *pool);
    void LFU_Shrink(HashBucket *bkt);
    void LFU_GhostShrink();
    void ARC_RHit(Node *node);
    void ARC_gRHit(Node *node);
    LFUHead *LFU_GetNewHeadNode();
    void ARC_FHit(Node *node);
    void LRU_GhostShrink();
    void LRU_Shrink(HashBucket *bkt);

    uint8_t hit(Node *node);
};

class Rep : public ARC {
public:
    Rep(uint32_t size);
    ~Rep();

    /* hit(Node *node) */
};




#endif /* REPLACEMENT_H */