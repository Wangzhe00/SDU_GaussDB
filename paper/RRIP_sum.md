



# High Performance Cache Replacement Using Re-Reference Interval Prediction (ISCA 2010)

### 1. Motivation

论文指出，在以下两种情境下，LRU不能高效地利用缓存：

- 热工作集大于可用缓存容量
- 对非时间局部性的数据的引用突发，从缓存中丢弃热数据集合

为了理解LRU能够具备良好收益的情况，我们举几个情景：

- 最近访问友好: （1，2，3，4，……，N，N，N-1，……，2，1），这种情境下，LRU很高，其它任何的替换策略都可能会降低这些访问模式的性能。
- 抖动访问: （1，2，3，……，K）重复。当K <= 缓存块数， LRU表现良好；否则，比较一般。
- 流访问模式：替换策略无关近要，没有存在的意义。
- 混合访问模式：重新引用间隔不固定，有近的有远的，（1-k，k-1）缓存A次 * （1-k，k-m）缓存N次，当m+k小于工作缓存时，LRU工作良好，否则工作较差。也就是说，当出现scan时，LRU表现总是不佳的。

NRU策略在一定程度上可以缓解这个问题：
```shell
NRU(Not Recent Used) 是LRU的一个近似策略，被广泛应用于现代高性能处理器中。应用NRU策略的cache，需要在每个cache block中增加一位标记，该标记（NRU bit）“0”表示最近可能被访问到的，“1”表示最近不能访问到的。

每当一个cache hit，该cache block的NRU bit被设置为“0”表示在最近的将来，该cache block很有可能再被访问到；每当一个cache miss，替换算法会从左至右扫描NRU bit为“1”的block，如果找到则替换出该cache block，并将新插入的cache block 的NRU bit置为“0”，如果没有找到，那么将所有cache block的NRU bit置为“1”，重新从左至右扫描。NRU在一定程度上，可以理解成向左查找表。
```

### 2. RRIP

```
```

啊实打实



