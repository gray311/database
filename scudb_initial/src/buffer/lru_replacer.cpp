/**
 * LRU implementation
 */
#include "buffer/lru_replacer.h"
#include "page/page.h"

namespace scudb {

template <typename T> LRUReplacer<T>::LRUReplacer() {}

template <typename T> LRUReplacer<T>::~LRUReplacer() {}

/*
 * Insert value into LRU
 */
template <typename T> void LRUReplacer<T>::Insert(const T &value) {
  auto iter = Map.find(value);
  List.push_front(value);
  if(iter != Map.end()) List.erase(iter->second);
  Map[value] = List.begin();
}

/* If LRU is non-empty, pop the head member from LRU to argument "value", and
 * return true. If LRU is empty, return false
 */
template <typename T> bool LRUReplacer<T>::Victim(T &value) {
  if (List.empty()) return false;
  auto iter = List.end();
  value = *(--iter);
  List.pop_back();
  Map.erase(value);
  return true;
}

/*
 * Remove value from LRU. If removal is successful, return true, otherwise
 * return false
 */
template <typename T> bool LRUReplacer<T>::Erase(const T &value) {
  auto iter = Map.find(value);
  if (iter == Map.end()) return false; 
  Map.erase(value);
  List.erase(iter->second);
  return true;
}

template <typename T> size_t LRUReplacer<T>::Size() { return List.size(); }

template class LRUReplacer<Page *>;
// test only
template class LRUReplacer<int>;

} // namespace scudb
