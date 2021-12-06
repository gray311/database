#pragma once

#include <cstdlib>
#include <vector>
#include <string>
#include <mutex>
#include <list>

#include "hash/hash_table.h"

namespace scudb {

template <typename K, typename V>
class ExtendibleHash : public HashTable<K, V> {

    struct Instance {
        K key;
        V value;
        Instance() = default;
        Instance(const K & key, const V & value): key(key), value(value) {}
    };

    struct Bucket {
        int local_depth = 0;
        std::list<Instance> instances;
    };

public:
    // constructor
    ExtendibleHash(size_t size);
    // helper function to generate hash addressing
    size_t HashKey(const K &key);
    // helper function to get global & local depth
    int GetGlobalDepth() const;
    int GetLocalDepth(int bucket_id) const;
    int GetNumBuckets() const;
    // lookup and modifier
    bool Find(const K &key, V &value) override;
    bool Remove(const K &key) override;
    void Insert(const K &key, const V &value) override;
    void Handle_OverflowBucket(Bucket * bucket); // 防止溢出

private:
    // add your own member variables here
    std::mutex mutex; // 进程锁
    const size_t size; //每个桶的默认长度
    int global_depth; // 总长度
    std::list<Bucket> buckets; // 桶列表
    std::vector<Bucket*> directory; // 目录，通过哈希值索引桶
    std::hash<K> Hash; // 哈希转换函数
};
} // namespace scudb