/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-12 20:20:43
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-17 20:55:50
 * @FilePath: /src/inc/SimpleBufferPool.h
 */
#ifndef SIMPLE_BUFFER_POOL_H 
#define SIMPLE_BUFFER_POOL_H

#include "baseStruct.h"

using namespace std;
using pageno = unsigned int;

/** Buffer Pool申明，请按规则实现以下接口*/
class BufferPool {
 protected:
  /** 原始文件 */
  string file_name;
  /** 请根据页面大小从小到大依次编号，即不同的页拥有不同的大小。
   * {{8k, 1024}, {16k, 2048}} 的正确编号为 8kb [0-1023] 16kb [1024-3071]*/
  map<size_t, size_t> page_no_info;
 public:
  /** buffer pool 的最大大小，超过该大小将导致程序OOM */
  static constexpr size_t max_buffer_pool_size = MEM_SIZE;

  BufferPool(const string &file_name, const map<size_t, size_t> &page_no_info)
      : file_name(std::move(file_name)), page_no_info(page_no_info) {
    /** 初始化你的buffer pool，初始化耗时不得超过3分钟 */
  };

  /** 读取一个指定页号为#no的页面, 将内容填充只buf */
  virtual void read_page(pageno no, unsigned int page_size, void *buf, int t_idx) {};

  /** 将buf内容写入一个指定页号为#no的页面 */
  virtual void write_page(pageno no, unsigned int page_size, void *buf, int t_idx) {};

  virtual void show_hit_rate() {};

  /** 资源清理 */
  virtual ~BufferPool() = default;
};

class SimpleBufferPool : public BufferPool {
 protected:
    map<size_t, size_t> page_no_info;
    int fds[32]{};
 public:
    void InitConstPara();

    void InitLRU(LRU *lru);
    void InitLfuHeadPool(Arch *arch, uint32_t size);

    SimpleBufferPool(const string &file_name, const map<size_t, size_t> &page_no_info);

    uint32_t getStartPage(uint8_t sizeType, uint32_t pno);

    void read_page(pageno no, unsigned int page_size, void *buf, int t_idx) override ;

    void write_page(pageno no, unsigned int page_size, void *buf, int t_idx) override ;

    ~SimpleBufferPool() override ;
};


#endif /* SIMPLE_BUFFER_POOL_H */
