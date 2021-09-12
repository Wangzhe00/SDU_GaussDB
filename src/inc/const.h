/*
 * @Author: your name
 * @Date: 2021-09-05 14:36:28
 * @LastEditTime: 2021-09-12 14:35:08
 * @LastEditors: Wangzhe
 * @Description: In User Settings Edit
 * @FilePath: \sftp\src\inc\const.h
 */
#ifndef CONST_H
#define CONST_H

#define PAGE_CHUNK_SIZE (2 * 1024 * 1024)
#define MEM_SIZE (2ll * 1024 * 1024 * 1024)
#define MX_DB_SIZE (30ll * 1024 * 1024 * 1024)
#define POOL_SMALL_BLOCK (256 * 1024)
#define POOL_LARGE_BLOCK (8 * 1024 * 1024)
#define CHUNK_CACHE_SIZE (POOL_LARGE_BLOCK / PAGE_CHUNK_SIZE)


#define ARC_LRU_RATIO 0.7

#define HASH_BUCKET_CLOSE_SMALL_SIZE (MX_DB_SIZE / POOL_SMALL_BLOCK)
#define HASH_BUCKET_SMALL_SIZE 120011

#define HASH_BUCKET_CLOSE_CHUNK_SIZE (MX_DB_SIZE / POOL_LARGE_BLOCK)
#define HASH_BUCKET_CHUNK_SIZE 3847


#define SET_LEFT_BIT(len, idx) (1 << (len - idx - 1))
#define GET_LEFT_BIT(len, idx, val) ((1 << (len - idx - 1)) & val)

#define BUCKET_MAX_IDX 0xffffff
#define _LIST_HEAD 0xffff

typedef enum {
    PS_8K  =    8 * 1024,
    PS_16K =   16 * 1024,
    PS_32K =   32 * 1024,
    PS_2M  = 2048 * 1024,
    PS_CNT = 4,
} PS;

enum {
    CACHE_IDX = 0,      /* cache line 前缀和 */
    PAGE_PREFIX_SUM,    /* 页号前缀和 */
    BLCOK_PAGE_COUNT,   /* 每个 cache line 包含的 page 个数 */
    PAGE_SIZE,          /* page大小 */
    SUM_BYTES,          /* 总字节数，在计算offset使用 */
    LAST_NUM,           /* 最后一个 cache line 剩余的 page 个数，防止最后一个块读取失败*/
    PART_CNT,
};

#endif /* CONST_H */