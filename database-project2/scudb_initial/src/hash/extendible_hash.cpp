#include <list>

#include "hash/extendible_hash.h"
#include "page/page.h"

namespace scudb {

/*
 * constructor
 * array_size: fixed array size for each bucket
 */
template <typename K, typename V>
ExtendibleHash<K, V>::ExtendibleHash(size_t size): size(size){
    global_depth = 0;
    buckets.emplace_back(); //类似于push_back，但接受多参数传递且效率更高
    directory.emplace_back(& buckets.front()); //默认一个桶
}

/*
 * helper function to calculate the hashing address of input key
 */
template <typename K, typename V>
size_t ExtendibleHash<K, V>::HashKey(const K &key) {
    size_t hash = Hash(key);
    return hash & ((1u << (unsigned)global_depth) - 1); //保留后global_depth位，作为索引
}

/*
 * helper function to return global depth of hash table
 * NOTE: you must implement this function in order to pass test
 */
template <typename K, typename V>
int ExtendibleHash<K, V>::GetGlobalDepth() const {
    return global_depth; //直接返回索引表长度
}

/*
 * helper function to return local depth of one specific bucket
 * NOTE: you must implement this function in order to pass test
 */
template <typename K, typename V>
int ExtendibleHash<K, V>::GetLocalDepth(int bucket_id) const {
    Bucket * bucket = directory[bucket_id];
    return bucket->local_depth;
}

/*
 * helper function to return current number of bucket in hash table
 */
template <typename K, typename V>
int ExtendibleHash<K, V>::GetNumBuckets() const {
    return int(buckets.size());
}

/*
 * lookup function to find value associate with input key
 */
template <typename K, typename V>
bool ExtendibleHash<K, V>::Find(const K &key, V &value) {

    std::lock_guard<std::mutex> lock(mutex); //进程锁，只准一个进程访问

    const size_t bucket_id = HashKey(key);
    const Bucket * bucket = directory[bucket_id];
    const std::list<Instance> & instances = bucket->instances;
    for (auto iterator = instances.begin(); iterator != instances.end(); iterator++) { //枚举迭代器，找到目标key
        if (iterator -> key == key) {
            value = iterator -> value;
            return true;
        }
    }
    return false;
}

/*
 * delete <key,value> entry in hash table
 * Shrink & Combination is not required for this project
 */
template <typename K, typename V>
bool ExtendibleHash<K, V>::Remove(const K &key) {

    std::lock_guard<std::mutex> lock(mutex); //进程锁，只能有一个进程正在删除

    const size_t bucket_id = HashKey(key);
    Bucket * bucket = directory[bucket_id];
    std::list<ExtendibleHash::Instance> & instances = bucket->instances;
    for (auto iterator = instances.begin(); iterator != instances.end(); iterator++) {
        if (iterator -> key == key) {
            instances.erase(iterator);
            return true;
        }
    }
    return false;
}

template <typename K, typename V>
void ExtendibleHash<K, V>::Handle_OverflowBucket(scudb::ExtendibleHash<K, V>::Bucket *bucket) {
    std::list<Instance> & instances = bucket->instances;
    if (instances.size() <= size) return;//桶未满，则返回

    // local depth == global depth则溢出
    if (bucket->local_depth == global_depth) {
        global_depth += 1;
        const size_t directory_size = directory.size();
        directory.resize(directory_size*2);

        for (size_t i = 0; i < directory_size; i++) {
            directory[i + directory_size] = directory[i];
        }
    }

    bucket->local_depth += 1;
    buckets.emplace_back();
    Bucket & companion_bucket = buckets.back(); //把同伴桶赋值为空桶
    companion_bucket.local_depth = bucket->local_depth;

    for (size_t i = directory.size()/2; i < directory.size(); i++) {
        if (directory[i] == bucket) {
            directory[i] = & companion_bucket;
        }
    }

    // 将bucket中的溢出示例部分插入同伴桶中
    for (auto iterator = instances.begin(); iterator != instances.end();) {
        size_t bucket_id = HashKey(iterator->key);
        Bucket* target_bucket = directory[bucket_id];
        if (target_bucket == bucket) {
            iterator++;
        } else {
            auto prev_iterator = iterator;
            iterator++;
            companion_bucket.instances.emplace_back(*prev_iterator);
            instances.erase(prev_iterator); //在原bucket中删除instance
        }
    }
    Handle_OverflowBucket(bucket);
    Handle_OverflowBucket(&companion_bucket);
}


/*
 * insert <key,value> entry in hash table
 * Split & Redistribute bucket when there is overflow and if necessary increase
 * global depth
 */
template <typename K, typename V>
void ExtendibleHash<K, V>::Insert(const K &key, const V &value) {

    std::lock_guard<std::mutex> lock(mutex); //进程锁，只能有一个进程正在插入

    const size_t bucket_id = HashKey(key);
    Bucket * bucket = directory[bucket_id];
    std::list<Instance> & instances = bucket->instances;
    for (auto iterator = instances.begin(); iterator != instances.end(); iterator++) { //检查hash值是否入桶
        if (iterator -> key == key) {
            iterator->value = value;
            return;
        }
    }
    instances.emplace_back(key, value);
    Handle_OverflowBucket(bucket);
}

template class ExtendibleHash<page_id_t, Page *>;
template class ExtendibleHash<Page *, std::list<Page *>::iterator>;
// test purpose
template class ExtendibleHash<int, std::string>;
template class ExtendibleHash<int, std::list<int>::iterator>;
template class ExtendibleHash<int, int>;

} // namespace scudb