## Daily Report 2021-09-08

### MySQL(InnoDB) buffer pool

#### 配置参数

- `innodb_buffer_pool_size`: buffer pool大小

- `innodb_buffer_pool_instances`: buffer pool实例个数（若bufferpool较大，可划分为多个instances，每个instance通过各自的list独立管理，提高读并发度）

- `innodb_buffer_pool_chunk_size`: 当增加或减少`innodb_buffer_pool_size`时，`innodb_buffer_pool_chunk_size`相应变化

>If the new innodb_buffer_pool_chunk_size value * innodb_buffer_pool_instances is larger than the current buffer pool size when the buffer pool is initialized, innodb_buffer_pool_chunk_size is truncated to innodb_buffer_pool_size / innodb_buffer_pool_instances.

>Buffer pool size must always be equal to or a multiple of innodb_buffer_pool_chunk_size * innodb_buffer_pool_instances. If you alter innodb_buffer_pool_chunk_size, innodb_buffer_pool_size is automatically adjusted to a value that is equal to or a multiple of innodb_buffer_pool_chunk_size * innodb_buffer_pool_instances. The adjustment occurs when the buffer pool is initialized. 

- `innodb_old_blocks_pct`: controls the percentage of “old” blocks in the LRU list（LRU链表中插入点的位置）

#### 替换策略：变种LRU

- 普通LRU会产生的问题：预读失效和缓冲池污染。
  - 预读失效：预先加载的一些page后续没有被访问，反而丢弃了原本LRU链表末尾的一些page。
  - 缓冲池污染：一次性扫描大量数据，buffer pool中所有page被替换出去。

- 解决方案：冷热数据分离
  - 将LRU链表分为两部分，热数据区和冷数据区。
  - 当某一page第一次被加载到buffer pool中，先将其放到冷数据区域的链表头部。
  - 经过`innodb_old_blocks_time`（单位：ms）后，若该page再次被访问，将其移动到热数据区域的链表头部。
  - 若page已经在热数据区，再次被访问，不需要每次都移动到热数据区链表头部，MySQL的优化方案是，热数据区的后3/4部分被访问需要移动到链表头部，前1/4部分不移动。

#### LRU链表

![](https://dev.mysql.com/doc/refman/5.7/en/images/innodb-buffer-pool-list.png)

- 分为两个部分：New Sublist，Old Sublist。
- `innodb_old_blocks_pct`控制插入点Midpoint。
- 全表扫描时，设置`innodb_old_blocks_time`的时间窗口可以有效的保护New Sublist。

#### 预读机制

- Linear read-ahead
- Random read-ahead

#### API

- [buf0buf.h](https://github.com/mysql/mysql-server/blob/8.0/storage/innobase/include/buf0buf.h): The database buffer pool high-level routines
  - `dberr_t buf_pool_init(ulint total_size, ulint n_instances)`: Creates the buffer pool.
  - `void buf_pool_free_all()`: Frees the buffer pool at shutdown.
  - `void buf_resize_thread()`: This is the thread for resizing buffer pool.
  - `void buf_pool_clear_hash_index(void)`: Clears the adaptive hash index on all pages in the buffer pool. 
  - `static inline ulint buf_pool_get_curr_size(void)`: Gets the current size of buffer buf_pool in bytes.
  - `static inline ulint buf_pool_get_n_pages(void)`: Gets the current size of buffer buf_pool in frames.
  - get
    - `bool buf_page_optimistic_get(ulint rw_latch, buf_block_t *block,
                             uint64_t modify_clock, Page_fetch fetch_mode,
                             const char *file, ulint line, mtr_t *mtr)`: Get optimistic access to a database page.
    - `bool buf_page_get_known_nowait(ulint rw_latch, buf_block_t *block,
                               Cache_hint hint, const char *file, ulint line,
                               mtr_t *mtr)`: Get access to a known database page, when no waiting can be done.
    - `const buf_block_t *buf_page_try_get_func(const page_id_t &page_id,
                                         const char *file, ulint line,
                                         mtr_t *mtr)`: Given a tablespace id and page number tries to get that page.
    - `buf_block_t *buf_page_get_gen(const page_id_t &page_id,
                              const page_size_t &page_size, ulint rw_latch,
                              buf_block_t *guess, Page_fetch mode,
                              const char *file, ulint line, mtr_t *mtr,
                              bool dirty_with_no_latch = false)`: Get access to a database page.                                          
    - `buf_block_t *buf_page_create(const page_id_t &page_id,
                             const page_size_t &page_size,
                             rw_lock_type_t rw_latch, mtr_t *mtr)`: Initializes a page to the buffer buf_pool.
  - `void buf_page_make_young(buf_page_t *bpage)`: Moves a page to the start of the buffer pool LRU list. 
  - `void buf_page_make_old(buf_page_t *bpage)`: Moved a page to the end of the buffer pool LRU list.
  - `static inline ibool buf_page_peek(const page_id_t &page_id)`: Returns TRUE if the page can be found in the buffer pool hash table.                     

- [buf0dblwr.h](https://github.com/mysql/mysql-server/blob/8.0/storage/innobase/include/buf0dblwr.h): Doublewrite buffer module

- [buf0rea.h](https://github.com/mysql/mysql-server/blob/8.0/storage/innobase/include/buf0rea.h): The database buffer read

- [buf0dump.h](https://github.com/mysql/mysql-server/blob/8.0/storage/innobase/include/buf0dump.h): Implements a buffer pool dump/load

- [buf0flu.h](https://github.com/mysql/mysql-server/blob/8.0/storage/innobase/include/buf0flu.h): The database buffer pool flush algorithm

- [buf0lru.h](https://github.com/mysql/mysql-server/blob/8.0/storage/innobase/include/buf0lru.h): The database buffer pool LRU replacement algorithm

#### Reference

- [MySQL源码](https://github.com/mysql/mysql-server)
  - [buffer pool API声明](https://github.com/mysql/mysql-server/tree/8.0/storage/innobase/include)
  - [buffer pool API实现](https://github.com/mysql/mysql-server/tree/8.0/storage/innobase/buf)
- [MySQL InnoDB文档](https://dev.mysql.com/doc/refman/5.7/en/innodb-storage-engine.html)

#### 待办&问题

- MySQL预读机制详细了解
- 不太清楚MySQL中buffer pool相关的API需要了解到什么程度，上面列出了一些认为比较重要的接口
- 多page size设计还没有找到相关论文或者资料
