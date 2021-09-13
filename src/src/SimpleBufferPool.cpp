/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-12 21:00:37
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-13 21:53:48
 * @FilePath: /src/src/SimpleBufferPool.cpp
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <csignal>
#include <vector>
#include <fcntl.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <log4cxx/propertyconfigurator.h>
#include <cassert>
#include <unordered_map>
#include <map>
#include <list>
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>

#include "const.h"
#include "errcode.h"
#include "memPool.h"
#include "hash_bucket.h"
#include "replacement.h"
#include "SimpleBufferPool.h"

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("SimpleBufferPool");
extern uint64_t pagePart[PART_CNT][PS_CNT];
// extern std::unordered_map<uint32_t, uint8_t> size2Idx;
Arch smallArch, largeArch;

uint32_t SimpleBufferPool::getStartPage(uint8_t sizeType, uint32_t pno) {
    sizeType++;
    return pno - ((pno - pagePart[PAGE_PREFIX_SUM][sizeType]) % pagePart[BLCOK_PAGE_COUNT][sizeType]);
}

extern mutex g_lruMutex;


void SimpleBufferPool::InitConstPara() {
        uint8_t idx = 1;
        uint64_t sumNo = 0, sumCacheIdx = 0, sumBytes = 0;
        memset(pagePart, 0, sizeof pagePart);
        for (auto it : page_no_info) {
            uint32_t blockSize = (it.first == PS_2M) ? POOL_LARGE_BLOCK : POOL_SMALL_BLOCK;
            sumNo += it.second;
            sumBytes += it.first * 1ll * it.second;
            sumCacheIdx += it.second / (blockSize / it.first);
            if (it.second % (blockSize / it.first) != 0) {
                sumCacheIdx++;
            }
            pagePart[CACHE_IDX][idx]        = sumCacheIdx;
            pagePart[PAGE_PREFIX_SUM][idx]  = sumNo;
            pagePart[BLCOK_PAGE_COUNT][idx] = blockSize / it.first;
            pagePart[E_PAGE_SIZE_][idx]        = it.first;                        /* 1:8K, 2:16K, 3:32K, 4:2M */
            // size2Idx[it.first]              = idx;                             /* 8K:1, 16K:2, 32K:3, 2M:4 */
            pagePart[SUM_BYTES][idx]        = sumBytes;
            pagePart[LAST_NUM][idx]         = it.second % pagePart[BLCOK_PAGE_COUNT][idx];
            idx++;
        }
    }

    void SimpleBufferPool::InitLRU(LRU *lru) {
        lru->lruSize = 0;
        INIT_LIST_HEAD(&lru->head);
    }

    SimpleBufferPool::SimpleBufferPool(const string &file_name,
                    const map<size_t, size_t> &page_no_info)
        : BufferPool(file_name, page_no_info), page_no_info(page_no_info) {
        LOG4CXX_INFO(logger, "BufferPool using file " << file_name);
        
        for (int &fd : fds) {
            fd = open(file_name.c_str(), O_RDWR);
            if (fd <= 0) {
                throw runtime_error("Can not open the source file");
            }
        }
        /* Init const parameter */
        InitConstPara();
        /* Init memory pool, small and chunk */
        assert(!InitPool(&smallArch.pool, 0,                   (MEM_SIZE >> 2) * 4 / POOL_SMALL_BLOCK));
        assert(!InitPool(&largeArch.pool, SET_LEFT_BIT(32, 4), (MEM_SIZE >> 2) * 0 / POOL_LARGE_BLOCK));
        LOG4CXX_DEBUG(logger, "memory pool 1 size: " << smallArch.pool.size);
        LOG4CXX_DEBUG(logger, "memory pool 2 size: " << largeArch.pool.size);
        LOG4CXX_DEBUG(logger, "memory pool 1 capacity: " << smallArch.pool.capacity);
        LOG4CXX_DEBUG(logger, "memory pool 2 capacity: " << largeArch.pool.capacity);

        /* Init Hash bucket */
        assert(!InitHashBucket(&smallArch.bkt, HASH_BUCKET_SMALL_SIZE));
        assert(!InitHashBucket(&largeArch.bkt, HASH_BUCKET_CHUNK_SIZE));
        LOG4CXX_DEBUG(logger, "hash bucket small size: " << smallArch.bkt.size);
        LOG4CXX_DEBUG(logger, "hash bucket small capSize: " << smallArch.bkt.capSize);
        LOG4CXX_DEBUG(logger, "hash bucket chunk size: " << largeArch.bkt.size);
        LOG4CXX_DEBUG(logger, "hash bucket chunk capSize: " << largeArch.bkt.capSize);

        /* Init LRU */
        InitLRU(&smallArch.rep);
        InitLRU(&largeArch.rep);
        LOG4CXX_DEBUG(logger, "arc small RSize: " << smallArch.rep.lruSize);
        LOG4CXX_DEBUG(logger, "arc chunk RSize: " << largeArch.rep.lruSize);
    }

    size_t SimpleBufferPool::page_start_offset(pageno no) {
        size_t boundary = 0;
        for (auto &range : page_no_info) {
        if (no >= range.second) {
            boundary += range.first * range.second;
            no -= range.second;
        } else {
            return boundary + (no * range.first);
        }
        }
        return -1;
    }

    void SimpleBufferPool::read_page(pageno no, unsigned int page_size, void *buf, int t_idx)  {
        int fd = fds[t_idx];
        g_lruMutex.lock();
        Arch *arch = (page_size == PS_2M) ? &largeArch : &smallArch;
        Node *dst = HashBucketFind(arch, no, page_size, fd, 0);
        uint8_t *ptr = (uint8_t *)((uint8_t *)dst->blk + (no - dst->page_start) * page_size);
        memcpy(buf, ptr, page_size);
        g_lruMutex.unlock();
    }

    void SimpleBufferPool::write_page(pageno no, unsigned int page_size, void *buf, int t_idx)  {
        int fd = fds[t_idx];
        g_lruMutex.lock(); 
        Arch *arch = (page_size == PS_2M) ? &largeArch : &smallArch;
        Node *dst = HashBucketFind(arch, no, page_size, fd, 1);
        uint8_t *ptr = (uint8_t *)dst->blk + (no - dst->page_start) * page_size;
        memcpy(ptr, buf, page_size);
        // printf("After pno = %X, fd = %d, Off = %d \n", no, fd, (no - dst->page_start) * page_size);
        // uint8_t *pp = (uint8_t*)buf;
        // printf("src: ");
        // for (int i = 0; i < 5; ++i) {
        //     printf("[%08X] ", *(uint8_t*)(pp + i));
        // }puts("");
        // pp = (uint8_t *)dst->blk;
        // printf("dst: ");
        // for (int i = 0; i < 5; ++i) {
        //     printf("[%08X] ", *(uint8_t*)(pp + i));
        // }puts("");
        g_lruMutex.unlock();
    }

    SimpleBufferPool::~SimpleBufferPool()  {
        LOG4CXX_INFO(logger, "Recycling...");
        assert(!DeInitPool(&smallArch.pool, fds[0]));
        assert(!DeInitPool(&largeArch.pool, fds[0]));
        for (int &fd : fds) {
            close(fd);
        }
        LOG4CXX_INFO(logger, "small Hit rate: " << ((smallArch.bkt.hit) * 1.0 / 100000000));
    }
