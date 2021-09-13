/*
 * @Description: header of mempool
 * @Author: Wangzhe
 * @Date: 2021-09-05 15:05:28
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-09-13 11:01:11
 * @FilePath: \src\inc\memPool.h
 */

#ifndef MEMPOOL_H
#define MEMPOOL_H
#include "baseStruct.h"

uint8_t InitPool(Pool *pool, uint32_t pageFlg, uint32_t size);
uint8_t DeInitPool(Pool *pool, int fd);

#endif /* MEMPOOL_H */