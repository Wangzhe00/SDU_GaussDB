<!--
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-05 16:11:59
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-05 16:15:48
 * @FilePath: \gauss\readme.md
-->

## 注意事项

1. 内存池中的节点需要提前分配，由于默认的采用`Copy-on-write`会导致只有使用时才会分配对应内存(页表映射等)，需要使用mlock锁定物理内存，注意要用**root**权限