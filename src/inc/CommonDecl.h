#pragma once


#include <sys/stat.h>
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
#include <unordered_set>
#include <queue>
#include <list>

#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <cstdint>
#include <set>
#include <map>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <random>
#include <tuple>

using std::string;
using std::map;
using std::unordered_map;
namespace chrono = std::chrono;


using pageno = unsigned int;
using nodesize_t = uint32_t;
using pageoffset_t = uint32_t; // node��ҳ��ƫ���� ����̫�� �������nodesize

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)


// #define LRU_NODE_COUNT 2048
#define POOL_CACHENODE_S_SIZE 32 * 1024
// #define POOL_CACHENODE_S_SIZE 2 * 1024 * 1024
#define POOL_CACHENODE_L_SIZE 2 * 1024 * 1024

#define PAGE_PS_8K  (8*1024)       // 8Kҳ���ֽ���
#define PAGE_PS_16K (16*1024)      // 16Kҳ���ֽ���
#define PAGE_PS_32K (32*1024)      // 32Kҳ���ֽ���
#define PAGE_PS_2M  (2*1024*1024)  // 2M ҳ���ֽ���

// enum PAGE_SIZE : size_t
// {
//   PS_8K = PAGE_PS_8K,
//   PS_16K = PAGE_PS_16K,
//   PS_32K = PAGE_PS_32K,
//   PS_2M = PAGE_PS_2M,
// };
enum E_PAGE_SIZE : uint8_t
{
  E_PS_8K = 0,
  E_PS_16K = 1,
  E_PS_32K = 2,
  E_PS_2M = 3,
  E_PS_MAX = 4,
};

#if (POOL_CACHENODE_S_SIZE % PAGE_PS_32K != 0) || (POOL_CACHENODE_L_SIZE % PAGE_PS_2M != 0)
#error "Node size must be multiply of 32K"
#endif
#define POOL_NODE_S_8K_NUM (POOL_CACHENODE_S_SIZE/PAGE_PS_8K)
#define POOL_NODE_S_16K_NUM (POOL_CACHENODE_S_SIZE/PAGE_PS_16K)
#define POOL_NODE_S_32K_NUM (POOL_CACHENODE_S_SIZE/PAGE_PS_32K)
#define POOL_NODE_L_2M_NUM (POOL_CACHENODE_L_SIZE/PAGE_PS_2M)


extern const size_t PAGE_NODE_SIZE_MAP[];  // ���ڸ���Сpageÿ��node���ֽ���
extern const size_t PAGE_SIZE_MAP[];       // 
extern const uint16_t PAGE_NUM_MAP[];      // ÿ��node��ŵĸ���Сpage��Ŀ

#define FD_TYPE int
#define FOPEN open
#define FSEEK lseek
#define FTELL tell
#define FCLOSE close
// #define FFLUSH flush
#define FREAD(fd, buf, size) read(fd, buf, size)
#define FWRITE(fd, buf, size) write(fd, buf, size)


inline void memcopy(uint8_t *__restrict a, const uint8_t *__restrict b, size_t n) {
  std::copy(b, b + n, a);
}


#ifdef USE_MEM_POOL
 #include "MemoryPool/MemoryPool.h"
#endif // USE_MEM_POOL

typedef uint8_t SBuf[POOL_CACHENODE_S_SIZE];
typedef uint8_t LBuf[POOL_CACHENODE_L_SIZE];

// #define USE_MEM_POOL

#ifdef USE_MEM_POOL
static MemoryPool<SBuf> poolS;
static std::mutex memLock;
#endif // USE_MEM_POOL

inline uint8_t *allocate(size_t size) {
#ifdef USE_MEM_POOL
  std::lock_guard<std::mutex> with(memLock);
  return *poolS.allocate();
#else // USE_MEM_POOL
  return new uint8_t[size];
#endif // USE_MEM_POOL
}
inline void deallocate(void *ptr) {
#ifdef USE_MEM_POOL
  std::lock_guard<std::mutex> with(memLock);
  poolS.deallocate((SBuf *)ptr);
#else // USE_MEM_POOL
  delete[](uint8_t *)ptr;
#endif // USE_MEM_POOL
}


// #define CCACHE_POOL_DEBUG_FLAG
// #ifdef CCACHE_POOL_DEBUG_FLAG
static std::mutex stdmutex;
extern std::mutex stdmutex;
// #endif


class SpinLock
{
  std::atomic_bool flag;
public:
  inline void lock(void) {
    bool e = false;
    while (!flag.compare_exchange_weak(e, true, std::memory_order_acquire)) { std::this_thread::yield(); e = false; }
  }
  inline void unlock(void) {
    flag.store(false, std::memory_order_release);
  }
};
