#ifndef REPLACEMENT_H
#define REPLACEMENT_H

#include <stdint.h>
#include "const.h"
#include "list.h"
#include "memPool.h"

enum {
    LRU = 0,
    LFU = 1,
};

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
    struct list_head R;
    struct list_head gR;
    struct list_head F;
    struct list_head gF;
public:
    ARC(uint32_t size) {
        uint32_t lruLen = size * ARC_LRU_RATIO;
        uint32_t lfuSize = size - lruSize;
        INIT_LIST_HEAD(this->R);
        INIT_LIST_HEAD(this->gR);
        INIT_LIST_HEAD(this->F);
        INIT_LIST_HEAD(this->gF);
        this->RRealLen = this->gRRealLen = this->FRealLen = this->gFRealLen = 0;
        
    }
    uint8_t hit(Node *node)
    {
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
    }
};

class Rep : public ARC {
public:
    Rep(uint32_t size): ARC(size);

    /* hit(Node *node) */
};

#endif /* REPLACEMENT_H */