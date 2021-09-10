<!--
 * @Description: 
 * @Author: Wangzhe
 * @Date: 2021-09-10 17:20:34
 * @LastEditors: Wangzhe
 * @LastEditTime: 2021-09-10 20:03:12
 * @FilePath: \src\doc\ARC_hit.md
-->

## ARC 满足以下特性

 - 每条链表长度至少为1
 - 初始化的时候LRU长度略大于LFU，防止顺序访问用不到LFU节点

## BUG预知

- 在多线程下，当某一线程`x` `read_page` 时，目前解决方案为直接返回某个`Node`（记为`N1`）的`blk`对应指针，如果存在另一个线程`y` `read_page`刚好为`N1`，正好当前`Node`需要`flush`，则会出现竞争问题，但概率很小，因为`read_page`会将`N1`更新，即使在`LFU`中也不会立即用。如果到时候遇到这方面的问题，就在出口的地方加上读取锁，牺牲性能，`memcpy`解决

## Full Miss

```cpp
if F.realLen < F.expectLen :
    get new node from unused memory pool as N1
    N1 insert F.head
    return

/* F.realLen == F.expectLen */
if gF.realLen < gF.expectLen:
    move gF.tail_node to gF.head
else :
    gF shrink_1  /* unlink original hashNode */
F shrink_1 , shrink node as N2  /* N2 is F.head */
N2'key replace gF.head
fetch data from disk into N2
R.realLen++;
```



## Hit R

```cpp
N1 = hit node in R
if F.realLen < F.expectLen :
    move N1 to F from R /* F.last_node insert R, N1 insert F.head */
    R.realLen--;
	F.realLen++;
    return
/* F is Full */
if gF.realLen < gF.expectLen:
    move tail node to head
else :
    gF shrink_1
F shrink_1 /* shrink node need return unused memory pool */
move N1 to F from R

R.realLen--;
```


## Hit F

```cpp
update F
```

## Hit gR
```cpp
N1 = hit node in gR
if gF.expectLen == 1:
    exec FULL MISS
    return 
F shrink_1, shrink node as  N2 /* shrink node no need return memory pool, directly N1 using */
N2'key replace N1
N1 insert gF from gR
fetch data from disk into N2.blk
N2 insert R from F
R.expectLen++;
F.expectLen--;
R.realLen++;
F.realLen--;
```

## Hit gF
```cpp
N1 = hit node in gF
if gF.expectLen == 1:
    gF shrink_1
    F shrink_1
    return 
R shrink_1, shrink node as N2 /* shrink node no need return memory pool, directly N1 using */
N2'key replace N1
N1 insert gR from gF
fetch data from disk into N2.blk
N2 insert F from R
F.expectLen++;
R.expectLen--;
F.realLen++;
R.realLen--;
```


