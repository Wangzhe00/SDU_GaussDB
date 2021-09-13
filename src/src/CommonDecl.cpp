/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-12 20:16:54
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-12 20:57:30
 * @FilePath: \sftp\src\src\CommonDecl.cpp
 */

#include "CommonDecl.h"
#include "main.h"


const size_t PAGE_NODE_SIZE_MAP[5] = { POOL_CACHENODE_S_SIZE, POOL_CACHENODE_S_SIZE, POOL_CACHENODE_S_SIZE, POOL_CACHENODE_L_SIZE, 0 };
const size_t PAGE_SIZE_MAP[5] = { PS_8K, PS_16K, PS_32K, PS_2M, 0 };
const uint16_t PAGE_NUM_MAP[5] = { POOL_NODE_S_8K_NUM, POOL_NODE_S_16K_NUM, POOL_NODE_S_32K_NUM, POOL_NODE_L_2M_NUM, 0 };

// MemoryPool<SBuf> poolS;

