/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-08 12:57:05
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-10 22:40:49
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
    struct list_head lfuHeadUsedPool;
    struct list_head lfuHeadUnusedPool;
    LFUHead F;
    LFUHead gF;
public:
    ARC(uint32_t size, uint8_t flg);

    void ARC::InitRep(uint32_t size, uint8_t flg);
    
    
    /**
     * @description: 清洗 LFU 的头结点，放入 unused 池子中
     * @param {LFUHead} *lfuHead
     * @return {*}
     * @Date: 2021-09-08 21:44:17
     */
    uint8_t LFU_GetFreq2();
    void LFU_GetFreq2();

    void ARC_RHit(Node *node);
    LFUHead *LFU_GetNewHeadNode();
    /**
     * @description: 命中 ARC中 LFU 链表实现
     * @param {Node} *node Cache Line Node
     * @return {*}
     * @Date: 2021-09-08 21:40:28
     */
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