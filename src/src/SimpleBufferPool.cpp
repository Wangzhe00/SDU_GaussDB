/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-12 21:00:37
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-09-13 11:09:23
 * @FilePath: \sftp\src\src\SimpleBufferPool.cpp
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

    uint8_t SimpleBufferPool::BufferPoolFindBlock(pageno no, unsigned int page_size, Node **dst, int fd)
    {
        Arch *arch = (page_size == PS_2M) ? &largeArch : &smallArch;
        assert(!HashBucketFind(arch, dst, no, page_size, fd));
        return ROK;
    }

    uint8_t SimpleBufferPool::BufferPoolWritePage(pageno no, unsigned int page_size, Node **dst, int fd, void *buf) {
        Arch *arch = (page_size == PS_2M) ? &largeArch : &smallArch;
        assert(!HashBucketFind(arch, dst, no, page_size, fd));
        (*dst)->pageFlg.dirty = 1;
        memcpy((uint8_t *)(*dst)->blk + (no - (*dst)->page_start) * page_size, buf, page_size);
        return ROK;
    }

    void SimpleBufferPool::read_page(pageno no, unsigned int page_size, void **buf, int t_idx)  {
        // size_t offset = page_start_offset(no);
        int fd = fds[t_idx];
        // lseek(fd, offset, SEEK_SET);
        // size_t ret = read(fd, buf, page_size);
        // if (ret != page_size) {
        //     LOG4CXX_FATAL(logger,
        //                 "read size error read:" << ret << " page size: " << page_size << " errno: " << errno << " page no: "
        //                                         << no);
        // }
        // assert(ret == page_size);

        Node *dst = NULL;
        assert(!BufferPoolFindBlock(no, page_size, &dst, fd));
        uint8_t *p = (uint8_t *)dst->blk;
        *buf = (void *)((uint8_t *)dst->blk + (no - dst->page_start) * page_size);
    }

    void SimpleBufferPool::write_page(pageno no, unsigned int page_size, void *buf, int t_idx)  {
        // uint64_t offset = page_start_offset(no);
        int fd = fds[t_idx];
        // lseek(fd, offset, SEEK_SET);
        // size_t ret = write(fd, buf, page_size);
        // assert(ret == page_size);
        Node *dst = NULL;
        assert(!BufferPoolWritePage(no, page_size, &dst, fd, buf));
    }

    SimpleBufferPool::~SimpleBufferPool()  {
        assert(!DeInitPool(&smallArch.pool, fds[0]));
        assert(!DeInitPool(&largeArch.pool, fds[0]));
        for (int &fd : fds) {
            close(fd);
        }
    }
