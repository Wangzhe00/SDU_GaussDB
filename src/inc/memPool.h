/*
 * @Description: header of mempool
 * @Author: Wangzhe
 * @Date: 2021-09-05 15:05:28
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-11 13:44:35
 * @FilePath: \src\inc\memPool.h
 */

#ifndef MEMPOOL_H
#define MEMPOOL_H
#include "baseStruct.h"

uint8_t InitPool(Pool *pool, uint32_t pageFlg, uint32_t size);
uint8_t DeInitPool(Pool *pool, uint8_t ps, uint32_t size);

#endif /* MEMPOOL_H */