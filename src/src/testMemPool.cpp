/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-14 17:29:28
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-16 19:26:43
 * @FilePath: /src/src/testMemPool.cpp
 */

#include <iostream>
#include <mutex>
#include <atomic>
#include <thread>
#include "MemoryPool.h"

using namespace std;

struct Data {
    uint8_t d[2048];
};

class SpinLock
{
  std::atomic_bool flag;
public:
  inline void lock(void) {
    bool e = false;
    while (!flag.compare_exchange_weak(e, true, std::memory_order_acquire)) { 
      std::this_thread::yield(); // do not spin
      e = false; 
    }
  }
  inline void unlock(void) {
    flag.store(false, std::memory_order_release);
  }
};


int main() {
    mutex lock1;
    std::recursive_mutex lock2;
    std::atomic_bool lock3;
    SpinLock lock4;
    printf("lock1.size() = %d\n", sizeof(lock1));
    printf("lock2.size() = %d\n", sizeof(lock2));
    printf("lock3.size() = %d\n", sizeof(lock3));
    printf("lock4.size() = %d\n", sizeof(lock4));


    MemoryPool<Data> pool;
    Data *ptr[20];
    for (int i = 0; i < 10; ++i) {
        ptr[i] = pool.allocate();
    }
    for (int i = 0; i < 10; ++i)
        pool.deallocate(ptr[i]);
    return 0;
}