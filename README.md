[TOC]



## 比赛注意事项

### 1. 比赛周期

- 初赛/复赛：2021年9月30日
- 总决赛：2021年10月下旬

### 2.作品提交

- 因[“互联网+大赛官网”](https://cy.ncss.cn/mtcontest/detail?id=8a80808d7ad10375017ae26b186e00db)对作品格式限制要求，选手们需要先在[“互联网+大赛官网”](https://cy.ncss.cn/mtcontest/detail?id=8a80808d7ad10375017ae26b186e00db)侧提交word或PPT作品描述文档即可（描述清楚答题思路即可），然后将代码文件提交到当前的华为云大赛平台
- GaussDB赛事组会根据[互联网+大赛官网](https://cy.ncss.cn/mtcontest/detail?id=8a80808d7ad10375017ae26b186e00db)提交的word文档和当前平台的代码得分情况综合评判优秀作品！
- 本平台代码作品命名规则“赛道名称-团队名-联系方式”，比如“`GaussDB赛道–xxxxx队名-电话号码`”﻿。
- **每人每天最多提交5次代码作品**

## 赛题内容

一般数据库系统都会有一个缓冲池，`GaussDB`数据库也不例外。`GaussDB(for MySQL)`数据库的`bufferpool`（缓冲池）主要用于将一些频繁访问的热点数据缓存在`bufferpool`中，避免对慢速磁盘设备的频繁访问，从而加快数据的访问速度，提升数据库的性能。

本赛题要求结合`GaussDB(for MySQL)`产品特性设计思路，设计合理的缓冲池方案，实现一个高性能、高扩展性的`bufferpool`（缓冲池）。

## 答题要求

- 实现一个基本功能的`bufferpool`，能够缓存固定大小`（page size 16KB）`一些热点数据。

- 使用`LRU`、`LFU`和`LRU-K`等淘汰算法及其变种，提高`bufferpool`的命中率，从而提升热点数据的访问速度。

- 在云化场景下，为了提高资源利用率，会多种数据库共用同一存储资源池，每种数据库有不同的`page size`（页面大小）, `bufferpool`需要有高扩展性，能够支持同时缓存各种`page size`（`page size `固定为`8KB`、`16KB`、`32KB`和`2MB`）的数据。

注：同时缓存是指不同大小的page会同时存在于同一个`bufferpool`中