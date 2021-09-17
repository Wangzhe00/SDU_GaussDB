<!--
 * @Author: your name
 * @Date: 2021-09-14 10:39:43
 * @LastEditTime: 2021-09-16 19:12:11
 * @LastEditors: Wangzhe
 * @Description: In User Settings Edit
 * @FilePath: /src/doc/thread.md
-->

总体逻辑：

外部调用`read_page`和`write_page`接口，内部均统一调用 `HashBucketFind`接口用以返回指定`page num`对应的`node`(对应的Cache Line)。
 - read_page，直接将拿到的node拷贝到入参
 - write_page, 直接将入参，拷贝到node中

拷贝操作均在内存，速度极快


目前只有三类锁
 - 全局锁(Lock)， 对整条LRU链表进行锁定，尽可能的缩小临界区，
 - 桶锁(Block)， 针对hash桶中`node`上的数据，比如`blk`，脏位等，只要涉及Fetch和Flush都必须获取对应的桶锁，*保证Fetch和Flush在临界区外*
 - 池子锁(Mlock)，关于内存池的锁



```python
HashBucketFind:
    Block.lock()
    Hit:
        while not Lock.try_lock(): # 防止死锁，如果获取不到，及时松开Block
            Block.unlock()
            yeild()
            Block.lock()
        调整LRU               # Lock  细扣代码，尽可能保证临界区最小
        if isW:               # Lock
            对应的node标脏     # Lock
        lock.unlock()
        return
    Miss:
        Mlock.lock()
        if 内存池存在 node:
            直接从内存池里拿
            Mlock.unlock()
            初始化node
            将node加入hash
            if isW:
                对应的node标脏
            从disk Fetch指定的数据   
            while not Lock.try_lock(): # 防止死锁，如果获取不到，及时松开Block
                Block.unlock()
                yeild()
                Block.lock()
            将 node 插入到 LRU 头部             # Lock
            lock.unlock()
        else : # -> 需要自行提供node
            while not Lock.try_lock():
                Block.unlock()
                yeild()
                Block.lock()
            获取最老的node， N1, 从LRU中摘下来  # Lock
            Lock.unlock()
            while not Block_N1.try_lock():
                Block.unlock()
                yeild()
                Block.lock()
            N1 从hash中删除
            Block_N1.unlock()
            对N1进行清洗
            脏的话，写回  
            将node加入hash
            if isW:
                对应的node标脏
            else
                从disk Fetch指定的数据   #如果是写的话，无需Fetch，反正要覆盖成脏的
            while not Lock.try_lock():
                Block.unlock()
                yeild()
                Block.lock()
            将 N1, 插入LRU中  # Lock
            Lock.unlock()
```
