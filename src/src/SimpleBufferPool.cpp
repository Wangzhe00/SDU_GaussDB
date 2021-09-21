/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-12 21:00:37
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-20 23:46:51
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
Arch smallArch;
// Arch largeArch;
std::mutex g_lruMutex;
uint64_t realMemStat;
uint64_t usefulMemStat;
int cntTest;
// std::atomic_uint_fast32_t runCnt;

uint32_t SimpleBufferPool::getStartPage(uint8_t sizeType, uint32_t pno) {
    return pno;
}

void SimpleBufferPool::InitConstPara() {
        uint8_t idx = 1;
        uint64_t sumNo = 0, sumBytes = 0;
        memset(pagePart, 0, sizeof pagePart);
        for (auto it : page_no_info) {
            sumNo += it.second;
            sumBytes += it.first * 1ll * it.second;
            pagePart[E_PAGE_SIZE_][idx]     = it.first; /* 1:8K, 2:16K, 3:32K, 4:2M */
            pagePart[PAGE_PREFIX_SUM][idx]  = sumNo;
            pagePart[SUM_BYTES][idx]        = sumBytes;
            idx++;
        }
    }

void SimpleBufferPool::InitLRU(LRU *lru) {
    lru->lruSize = 0;
    INIT_LIST_HEAD(&lru->head);
}

void SimpleBufferPool::InitLfuHeadPool(Arch *arch, uint32_t size) {
    LFUHead *lfuHead = NULL;
    arch->lfu.freq = _LIST_HEAD;
    arch->lfu.len = 0;
    INIT_LIST_HEAD(&arch->lfu.hlist);
    INIT_LIST_HEAD(&arch->lfu.vlist);
    INIT_LIST_HEAD(&arch->lfu.pool);
    INIT_LIST_HEAD(&arch->lfuHeadUsedPool);
    INIT_LIST_HEAD(&arch->lfuHeadUnusedPool);
    for (int i = 0; i < size; ++i) {
        lfuHead = (LFUHead *)malloc(sizeof(LFUHead));
        lfuHead->freq = 0;
        lfuHead->len = 0;
        INIT_LIST_HEAD(&lfuHead->hlist);
        INIT_LIST_HEAD(&lfuHead->vlist);
        INIT_LIST_HEAD(&lfuHead->pool);
        list_add(&lfuHead->pool, &arch->lfuHeadUnusedPool);
    }
    realMemStat += sizeof(LFUHead) * size;
}

SimpleBufferPool::SimpleBufferPool(const string &file_name,
                const map<size_t, size_t> &page_no_info)
    : BufferPool(file_name, page_no_info), page_no_info(page_no_info) {
    LOG4CXX_INFO(logger, "BufferPool using file " << file_name);
    // runCnt.store(0);
    for (int &fd : fds) {
        fd = open(file_name.c_str(), O_RDWR);
        if (fd <= 0) {
            throw runtime_error("Can not open the source file");
        }
    }
    /* Init const parameter */
    InitConstPara();
    /* Init memory pool, hash bucket, LRU */
    assert(!InitPool(&smallArch.pool, 0, MEM_SIZE / POOL_SMALL_BLOCK));
    /* 2000的由来, 2000头结点节点，可以最多容纳 4000000个page，按照最小的page_size=8K, 也大于30G数据库大小 */
    InitLfuHeadPool(&smallArch, 2000);
    assert(!InitHashBucket(&smallArch, (uint32_t)(pagePart[PAGE_PREFIX_SUM][4] + 1)));
    // InitLRU(&smallArch.rep);

    LOG4CXX_INFO(logger, "memory pool 1 size: " << smallArch.pool.size);
    LOG4CXX_INFO(logger, "memory pool 1 capacity: " << smallArch.pool.capacity);
    LOG4CXX_INFO(logger, "hash bucket small size: " << smallArch.bkt.size);
    LOG4CXX_INFO(logger, "hash bucket small capSize: " << smallArch.bkt.capSize);
    LOG4CXX_INFO(logger, "arc small RSize: " << smallArch.rep.lruSize);
    LOG4CXX_INFO(logger, "realMemStat: " << realMemStat);
    LOG4CXX_INFO(logger, "usefulMemStat: " << usefulMemStat);
    LOG4CXX_INFO(logger, "unusefulMemStat: " << realMemStat - usefulMemStat);
}

void SimpleBufferPool::read_page(pageno no, unsigned int page_size, void *buf, int t_idx)  {
    int fd = fds[t_idx];
    // g_lruMutex.lock();
    Node *dst = HashBucketFind(&smallArch, no, page_size, fd, 0);
    memcpy(buf, dst->blk, page_size);
    smallArch.bkt.bkt[no].bLock.unlock();
    // runCnt--;
    // g_lruMutex.unlock();
}

void SimpleBufferPool::write_page(pageno no, unsigned int page_size, void *buf, int t_idx)  {
    int fd = fds[t_idx];
    // g_lruMutex.lock(); 
    Node *dst = HashBucketFind(&smallArch, no, page_size, fd, 1);
    memcpy(dst->blk, buf, page_size);
    // g_lruMutex.unlock();
    smallArch.bkt.bkt[no].bLock.unlock();
    // runCnt--;
}

SimpleBufferPool::~SimpleBufferPool()  {
    char buf[64];
    HashBucket *bkt = &smallArch.bkt;
    LOG4CXX_INFO(logger, "Recycling...");
    sprintf(buf, "pool, usedCnt = %d, ununsedCnt = %d", smallArch.pool.usedCnt, smallArch.pool.unusedCnt);
    LOG4CXX_INFO(logger, buf);
    assert(!DeInitPool(&smallArch.pool, fds[0]));
    for (int &fd : fds) {
        close(fd);
    }
    uint32_t hit = bkt->hit.load();
    uint32_t miss = bkt->miss.load();
    uint32_t fetched = bkt->fetched.load();
    uint32_t hitWrite = bkt->hitWrite.load();
    uint32_t readCnt = bkt->readCnt.load();
    uint32_t writeCnt = bkt->writeCnt.load();
    sprintf(buf, "small Hit:[%d], Miss:[%d], Rate[%.2f%], Fetched:[%d], HitWrite:[%d]", hit, miss, (hit * 100.0) / (hit + miss), fetched, hitWrite);
    LOG4CXX_INFO(logger, buf);
    sprintf(buf, "small ReadCnt:[%d](%.2f%), WriteCnt:[%d](%.2f%)", readCnt, readCnt * 100.0 / (readCnt + writeCnt), writeCnt, writeCnt * 100.0 / (readCnt + writeCnt));
    LOG4CXX_INFO(logger, buf);
    printf("cntTest = %d\n", cntTest);
}
