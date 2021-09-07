# PostgreSQL的buffer分析

[源代码位置](https://github.com/postgres/postgres/tree/master/src/backend/storage/buffer)

## API

**源代码中注释十分清晰，了解细节请查看源文件，此处仅列出简要的概括**

- [buf_init.c](https://github.com/postgres/postgres/blob/master/src/backend/storage/buffer/buf_init.c)
  - `void InitBufferPool(void)`初始化shared buffer pool
  - `Size BufferShmemSize(void)`计算buffer pool的大小
- [buf_table.c](https://github.com/postgres/postgres/blob/master/src/backend/storage/buffer/buf_table.c)
  - `Size BufTableShmemSize(int size)`估计一个mapping  hashtable需要的空间
  - `void InitBufTable(int size)`初始化hash table，大小由上面的那个函数计算
  - `uint32 BufTableHashCode(BufferTag *tagPtr)`计算这个table的hash
  - `int BufTableLookup(BufferTag *tagPtr, uint32 hashcode)`给一个BufferTag，返回一个buffer ID
  - `int BufTableInsert(BufferTag *tagPtr, uint32 hashcode, int buf_id)`向hashTable插入一个条目
  - `void BufTableDelete(BufferTag *tagPtr, uint32 hashcode)`删掉bufferTable中的一个条目
- [bufmgr.c](https://github.com/postgres/postgres/blob/master/src/backend/storage/buffer/bufmgr.c)
- [freelist.c](https://github.com/postgres/postgres/blob/master/src/backend/storage/buffer/freelist.c)
- [localbuf.c](https://github.com/postgres/postgres/blob/master/src/backend/storage/buffer/localbuf.c)

## 设计思路解读

[README](https://github.com/postgres/postgres/blob/master/src/backend/storage/buffer/README)阅读中

