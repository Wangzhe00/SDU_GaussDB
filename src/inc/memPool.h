/*
 * @Description: header of mempool
 * @Author: Wangzhe
 * @Date: 2021-09-05 15:05:28
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-07 17:09:17
 * @FilePath: \gauss\inc\memPool.h
 */

#ifndef MEMPOOL_H
#define MEMPOOL_H
#include <stdint.h>
#include "list.h"

/* size per blk = size * 8K */
typedef struct {
    /**
     * [pageFlg] descrition
     *       +-----------+-------------------------------+
     *       | 0123 4567 |       first page num          |
     * bit:  | xxxx_xxxx | xxxx_xxxx xxxx_xxxx xxxx_xxxx |
     *       +-----------+-------------------------------+
     * 
     * [0:1] : page tag, 0: 8K, 1:16K, 2:32K, 3:2MB
     * [2:2] : 0:LRU, 1:LFU
     * [3:3] : dirty flag, 1:dirty
     * [4:4] : size type, 0:small, 1:chunk
     * [5:5] : used, 1:used
     * [6:7] : multiple of bucket size, deepest bucket is 4 layers
     * 
     **/
    struct {
        uint32_t sizeType : 2;
        uint32_t rep : 1;
        uint32_t dirty : 1;
        uint32_t poolType : 1;
        uint32_t used : 1;
        uint32_t layer : 2;
    } pageFlg;
    uint32_t page_start;     /* first page num */
    
    void *blk;               /* first address */
    struct list_head memP;   /* list for mempool */
    struct hlist_node hash;  /* list for hash bucket */

    union {
        struct list_head lru;
        struct list_head lfu;
    }arc;

} Node;



typedef struct {
    uint16_t size;
    uint32_t capacity;
    Node used;
    Node unused;
}Pool;


uint8_t InitPool(Pool *pool, uint8_t ps, uint32_t size);
uint8_t DeInitPool(Pool *pool, uint8_t ps, uint32_t size);

#endif /* MEMPOOL_H */