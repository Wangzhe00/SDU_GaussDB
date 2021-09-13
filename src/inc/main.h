/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-12 20:20:43
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-12 20:30:07
 * @FilePath: \sftp\src\inc\main.h
 */
#ifndef MAIN_H
#define MAIN_H

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

  BufferPool(string file_name, const map<size_t, size_t> &page_no_info)
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

    void InitLRU(LRU *lru) ;

    SimpleBufferPool(const string &file_name,
                    const map<size_t, size_t> &page_no_info);

    size_t page_start_offset(pageno no);

    uint8_t BufferPoolFindBlock(pageno no, unsigned int page_size, Node *dst, int fd);

    uint8_t BufferPoolWritePage(pageno no, unsigned int page_size, Node *dst, int fd, void *buf);

    void read_page(pageno no, unsigned int page_size, void *buf, int t_idx) override ;

    void write_page(pageno no, unsigned int page_size, void *buf, int t_idx) override ;

    ~SimpleBufferPool() override ;
};


#endif /* MAIN_H */