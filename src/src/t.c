/*
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 13:33:28
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-05 17:11:49
 * @FilePath: \gauss\src\t.c
 */
#include <stdio.h>
#include <assert.h>

#include "list.h"
#include "const.h"
#include "memPool.h"


Pool P1;

int main() {
    
    assert(!InitPool(&P1, POOL_SMALL_BLOCK >> 13, (MEM_SIZE >> 2) * 3 / POOL_SMALL_BLOCK));
    assert(!DeInitPool(&P1, POOL_SMALL_BLOCK >> 13, (MEM_SIZE >> 2) * 3 / POOL_SMALL_BLOCK));

    return 0;
}