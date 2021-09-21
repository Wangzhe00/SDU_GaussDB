/*
 * @Author: your name
 * @Date: 2021-09-05 14:36:28
 * @LastEditTime: 2021-09-20 23:49:45
 * @LastEditors: Wangzhe
 * @Description: In User Settings Edit
 * @FilePath: /src/inc/const.h
 */
#ifndef CONST_H
#define CONST_H
#include <stdexcept>

#define PAGE_CHUNK_SIZE (2 * 1024 * 1024)
#define MEM_SIZE (6464 * 1ll * 1024 * 1024)
#define MX_DB_SIZE (30ll * 1024 * 1024 * 1024)
#define LFU_HEAD_HASH (100)


#define POOL_SMALLEST_BLOCK (8 * 1024)
#define POOL_SMALL_BLOCK (16 * 1024)
#define POOL_LARGE_BLOCK (8 * 1024 * 1024)
#define CHUNK_CACHE_SIZE (POOL_LARGE_BLOCK / PAGE_CHUNK_SIZE)

#define ARC_LRU_RATIO 0.7

#define HASH_BUCKET_CLOSE_SMALL_SIZE (MX_DB_SIZE / POOL_SMALLEST_BLOCK)
#define HASH_BUCKET_SMALL_SIZE 3932167

#define HASH_BUCKET_CLOSE_CHUNK_SIZE (MX_DB_SIZE / POOL_LARGE_BLOCK)
#define HASH_BUCKET_CHUNK_SIZE 3847


#define SET_LEFT_BIT(len, idx) (1 << (len - idx - 1))
#define GET_LEFT_BIT(len, idx, val) ((1 << (len - idx - 1)) & val)

#define BUCKET_MAX_IDX 0xffffff
#define _LIST_HEAD 0xffff

enum PS {
    PS_8K  =    8 * 1024,
    PS_16K =   16 * 1024,
    PS_32K =   32 * 1024,
    PS_2M  = 2048 * 1024,
    PS_CNT = 5,
} ;

enum {
    E_PAGE_SIZE_ = 0,       /* page大小 */
    PAGE_PREFIX_SUM = 1,    /* 页号前缀和 */
    SUM_BYTES = 2,          /* 总字节数，在计算offset使用 */
    PART_CNT,
};


inline uint8_t size2Idx(const uint32_t &page_size) {
    switch (page_size) {
        case PS_8K: return 1;
        case PS_16K: return 2;
        case PS_32K: return 3;
        case PS_2M: return 4;
        default:throw std::runtime_error("PageSizeNotSupported");
    }
    return 0;
}

#endif /* CONST_H */