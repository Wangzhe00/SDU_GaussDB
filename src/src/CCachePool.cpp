
#include "CCachePool.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
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
#include <list>


#include <map>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <random>

#include "CommonDecl.h"
#include "main.h"
// #include "CDLL.h"
// #include "LRUBufferPool.h"

/*
#define FD_TYPE FILE*
//#define ISFDINVALID(x) (x == NULL)
#define FOPEN fopen
#define FSEEK fseek
#define FTELL ftell
#define FCLOSE fclose
#define FREAD(fd, buf, size) fread(buf, 1, size, fd)
#define FWRITE(fd, buf, size) fwrite(buf, 1, size, fd)
#undef O_RDWR
#define O_RDWR "rb+"
*/




// using namespace std;

static bool g_program_shutdown = false;
static int server_socket = -1;
static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("EXAMPLE");
/*
#define LOG_ERROR(logger, message, ...) do { \
   vsprint\
  } while (0);
*/




enum MSG_TYPE
{
  GET = 0,
  SET,
  INVALID_TYPE
};

/*
class HashBin
{
  CDLL *bin[];
};
*/



// 测试使用 #####################################################################################################################################################################
/*
#define PAGE_NUM_8K  (131072 * 2) //  1G * 2
#define PAGE_NUM_16K (65536 * 2)  //  1G * 2
#define PAGE_NUM_32K (32768 * 2)  //  1G * 2
#define PAGE_NUM_2M  (512 * 2)    //  1G * 2
*/

// 162.9M/s

#define PAGE_NUM_8K  (0 * 2) //  1G * 2
#define PAGE_NUM_16K (65536 * 10)  //  1G * 2 1441792
#define PAGE_NUM_32K (0 * 2)  //  1G * 2
#define PAGE_NUM_2M  (0 * 2)    //  1G * 2

const uint32_t workNodeCount = 1'000'000; // 331197; // 8 * 240 * 1024; // 测试点数量 240 * 1024 = 60s
/*
#define PAGE_NUM_8K  32
#define PAGE_NUM_16K 16
#define PAGE_NUM_32K 8
#define PAGE_NUM_2M  2
*/
map<size_t, size_t> page_no_info{
  {PS_8K,  PAGE_NUM_8K},
  {PS_16K, PAGE_NUM_16K},
  {PS_32K, PAGE_NUM_32K},
  {PS_2M,  PAGE_NUM_2M},
};
struct __attribute__((packed)) WorkLoadNode
{
  uint32_t rw : 4; // R0 W1
  uint32_t size : 4;
  uint32_t no : 24;
};
const char *fileName = "/home/wangzhe/guo/in/test.dat";
uint8_t buf[PS_2M] = { 0 }; // 测试中使用的Buffer 只开一个最大号
size_t DB_FILE_SIZE; // 数据库文件预期大小
pageno PAGE_COUNT; // 总页数
uint32_t *pageIds; // 缓存bench中的页特征值
std::mutex benchLock;
const uint32_t PAGEIDENT = 0x80000000u;// 0xDEADBEEF;
std::default_random_engine randEngine(1354);
std::uniform_int_distribution<unsigned> randPage8(0, PAGE_NUM_8K - 1);
std::uniform_int_distribution<unsigned> randPage16(0, PAGE_NUM_16K - 1);
std::uniform_int_distribution<unsigned> randPage32(0, PAGE_NUM_32K - 1);
std::uniform_int_distribution<unsigned> randPage2m(0, PAGE_NUM_2M - 1);
std::uniform_int_distribution<unsigned> randWRb(0, 1);
std::uniform_int_distribution<unsigned> randPageType(0, page_no_info.size() - 1);
std::uniform_int_distribution<unsigned> RANDOM_DISTRIB_TAB[] = { randPage8 , randPage16, randPage32, randPage2m };
const pageno PAGE_NUM_TAB[] = { PAGE_NUM_8K , PAGE_NUM_16K, PAGE_NUM_32K, PAGE_NUM_2M };
const uint32_t PAGE_SIZE_TAB[] = { PS_8K , PS_16K, PS_32K, PS_2M };
const size_t pageCountPrefix[] = { 0, PAGE_NUM_8K, PAGE_NUM_8K + PAGE_NUM_16K, PAGE_NUM_8K + PAGE_NUM_16K + PAGE_NUM_32K };

inline void setBenchValue(const uint32_t &pageNo, const uint32_t &val) {
  std::lock_guard<std::mutex> with(benchLock);
  pageIds[pageNo] = val;
}
inline uint32_t getBenchValue(const uint32_t pageNo) {
  std::lock_guard<std::mutex> with(benchLock);
  return pageIds[pageNo];
}

void init_testbench(const string &file_name,
  const map<size_t, size_t> &page_no_info) {
  // 初始化数据库文件 
  FD_TYPE c_fd = FOPEN(file_name.c_str(), O_APPEND);
  if (c_fd <= 0) {
    throw std::runtime_error("Can not open the source file");
  }
  FCLOSE(c_fd);
  c_fd = FOPEN(file_name.c_str(), O_RDWR);
  if (c_fd <= 0) {
    throw std::runtime_error("Can not open the source file");
  }
  FSEEK(c_fd, 0L, SEEK_END); 
  struct stat st;
  fstat(c_fd, &st);
  size_t c_fsize = st.st_size;
  {
    // InitDbFile
    size_t targetSize = 0;
    PAGE_COUNT = 0;
    for (auto &range : page_no_info) {
      targetSize += range.first * (range.second);
      PAGE_COUNT += range.second;
    }
    DB_FILE_SIZE = targetSize;
    // 扩容数据库到指定大小
    if (targetSize > c_fsize) {
      ptrdiff_t diff = targetSize - c_fsize;
      FSEEK(c_fd, c_fsize, SEEK_SET);
      while (diff > 0) {
        size_t batch = std::min((ptrdiff_t)PS_2M, diff);
        FWRITE(c_fd, buf, batch);
        c_fsize += batch;
        diff -= batch;
        printf("Expanding %llu -> %llu\r", c_fsize, targetSize);
      }
      printf("Expanding %llu -> %llu\n", c_fsize, targetSize);
    }
  }

  // 随机填充标记page与数据库
  printf("Wartermarking file\n");
  pageIds = new uint32_t[PAGE_COUNT];

  pageno ckptcnt = PAGE_COUNT / 100;
  pageno nextCkpt = ckptcnt;
  size_t ptr = 0;
  size_t page_cnt = 0;
  for (auto &range : page_no_info) {
    for (size_t i = 0; i < range.second; ++i, ++page_cnt) {
      FSEEK(c_fd, ptr, SEEK_SET);
      *(uint32_t *)buf = (uint32_t)(page_cnt + PAGEIDENT);
      pageIds[page_cnt] = (uint32_t)(page_cnt + PAGEIDENT);
      FWRITE(c_fd, buf, 4);
      ptr += range.first;
      if (page_cnt > nextCkpt) {
        printf("%d/%d\n", page_cnt, PAGE_COUNT);
        nextCkpt += ckptcnt;
      }
    }
  }

  FCLOSE(c_fd);
  printf("DB Init Done Size:%lld Pages:%d\n", DB_FILE_SIZE, PAGE_COUNT);
}

// 清理测试环境
void deinit_testbench(WorkLoadNode *pLoad) {
  delete[]pageIds;
  delete[]pLoad;
}

// 加载测试序列
// 当前实现未从序列文件加载数据，为随机访问随机读写
// 待加载wrokload文件
WorkLoadNode *load_workload(uint32_t nodes) {
  WorkLoadNode *ret = new WorkLoadNode[nodes];
  for (uint32_t i = 0; i < nodes; ++i) {
    uint32_t pageType = 1; // randPageType(randEngine);
    while (pageType >= PAGE_NUM_TAB[pageType]) {
      if (++pageType > 3) { pageType = 0; }
    }
    WorkLoadNode &node = ret[i];
    node.no = pageCountPrefix[pageType] + RANDOM_DISTRIB_TAB[pageType](randEngine);
    node.rw = randWRb(randEngine);
    node.size = pageType;
  }
  return ret;
}

// 冷校验数据库
// 检查缓冲池持久化是否正确
bool checkDbIntegrity(const string &file_name) {
  bool pass = true;
  size_t ptr = 0;
  size_t pageCnt = 0;
  FD_TYPE c_fd = FOPEN(file_name.c_str(), O_RDWR);
  for (auto &range : page_no_info) {
    for (size_t i = 0; i < range.second; ++i, ++pageCnt) {
      FSEEK(c_fd, ptr, SEEK_SET);
      FREAD(c_fd, buf, 4);
      if (pageIds[pageCnt] != *(uint32_t *)buf) {
        pass = false;
        printf("CheckFail @Page.%08X Off.%08X Read:[%08X] != [%08X]\n", pageCnt, ptr, *(uint32_t *)buf, pageIds[pageCnt]);
        return false;
      }
      ptr += range.first;
    }
  }
  FCLOSE(c_fd);
  return pass;
}


bool chkU32(void *pa, uint32_t v) {
  return *(uint32_t *)pa == v;
}

// 使用给定测试序列测试
void bench_workload(const string &file_name,
  const map<size_t, size_t> &page_no_info,
  const WorkLoadNode *pLoad, uint32_t nodes) {

  auto start = chrono::high_resolution_clock::now();
  init_testbench(fileName, page_no_info);
  auto end = chrono::high_resolution_clock::now();
  double elapsedTime = chrono::duration_cast<chrono::nanoseconds>(end - start).count() * 1e-9;
  printf("BenchInitTime:%fs\n", elapsedTime);
  printf("DBFile:%s TestPoints:%d\n", file_name.c_str(), nodes);
  start = chrono::high_resolution_clock::now();
  SimpleBufferPool *bp = new SimpleBufferPool(fileName, page_no_info);
  end = chrono::high_resolution_clock::now();
  elapsedTime = chrono::duration_cast<chrono::nanoseconds>(end - start).count() * 1e-9;
  printf("InitTime:%fs\nTestBegin...\n", elapsedTime);

   
  uint32_t ckptcnt = nodes / 100;
  uint32_t nextCkpt = ckptcnt;
  start = chrono::high_resolution_clock::now();
  for (uint32_t i = 0; i < nodes; ++i) {
    const WorkLoadNode &node = pLoad[i];
    if (node.rw == 0) { // read
      //printf("TEST No.%-5X R %u %08X\n", i, node.size, node.no);
      uint8_t *rbuf = nullptr;
      bp->read_page(node.no, PAGE_SIZE_TAB[node.size], &rbuf, i%32);
      uint32_t val = getBenchValue(node.no);
      if (!chkU32(rbuf, val)) {
        printf("TEST Failed when processing work node %d <RW:%c S:%u %08X> [%08X] != [%08X]\n", i, (node.rw ? 'W' : 'R'), node.size, node.no, (*(uint32_t *)rbuf), val);
        throw std::exception();
      }
    } else { // write
      //printf("TEST No.%-5X W %u %08X %08X\n", i, node.size, node.no, i);
      setBenchValue(node.no, i);
      *(uint32_t *)buf = i;
      bp->write_page(node.no, PAGE_SIZE_TAB[node.size], buf, 1);
    }
    if (i == nextCkpt) {
      printf("%u / %u done\n", i, nodes);
      nextCkpt += ckptcnt;
    }
  }
  end = chrono::high_resolution_clock::now();
  elapsedTime = chrono::duration_cast<chrono::nanoseconds>(end - start).count() * 1e-9;
  printf("TestDone\nRunTime:%fs\n", elapsedTime);
  bp->show_hit_rate();
  start = chrono::high_resolution_clock::now();
  delete bp;
  end = chrono::high_resolution_clock::now();
  elapsedTime = chrono::duration_cast<chrono::nanoseconds>(end - start).count() * 1e-9;
  printf("RecyclingTime:%fs\n", elapsedTime);
  printf("Checking DB file integrity... ");
  // 离线检查数据库完整性
  if (checkDbIntegrity(file_name)) {
    printf("Success\n");
  } else {
    printf("DB Check Fail\n");
    throw std::exception();
    return;
  }
}

// 本地测试入口
// 当前实现为单线程测试
int main(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);
  log4cxx::PropertyConfigurator::configureAndWatch("./log4cxx.properties");
  printf("Loading bench workload...\n");
  WorkLoadNode *workLoads = load_workload(workNodeCount);
  printf("Wordload Loaded\n");
  auto start = chrono::high_resolution_clock::now();
  bench_workload(fileName, page_no_info, workLoads, workNodeCount);
  auto end = chrono::high_resolution_clock::now();
  double elapsedTime = chrono::duration_cast<chrono::nanoseconds>(end - start).count() * 1e-9;
  printf("TestBenchRunTime:%fs\n", elapsedTime);

  deinit_testbench(workLoads);
  printf("Press <Enter> to exit.\n");
  //while (getchar() != '\n') {
  //}
  return 0;
}





