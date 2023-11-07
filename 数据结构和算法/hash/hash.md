hash算法及应用
1、 hash函数及冲突；
2、 散列表（哈希表）；
3、 布隆过滤器；
4、 hyperloglog
5、 分布式一致性hash；

选择hash算法两点注意：
1、 计算速度快；
2、 强随机性

主要的hash算法：
murmurhash ： 
cityhash ： 
siphash ： 字符串接近时有强随机性

--------------------------------------------------------------------------------------------

1、 散列表（哈希表）
1.1冲突解决：
  1.1.1、 拉链法
  1.1.2、 开放寻址法

1.2应用：
1、 C++ STL unordered_map、unordered_set、unordered_multimap、unordered_multiset
    （map、set、multimap、multiset是红黑树实现）
    为了实现迭代器，STL实现中讲数据结点串成单链表；


2、 布隆过滤器

