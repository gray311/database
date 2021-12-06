/**
 * lru_replacer.h
 *
 * Functionality: The buffer pool manager must maintain a LRU list to collect
 * all the pages that are unpinned and ready to be swapped. The simplest way to
 * implement LRU is a FIFO queue, but remember to dequeue or enqueue pages when
 * a page changes from unpinned to pinned, or vice-versa.
 */

#pragma once
#include <list>
#include <unordered_map>
#include "buffer/replacer.h"
#include "hash/extendible_hash.h"

namespace scudb {

template <typename T> class LRUReplacer : public Replacer<T> {
public:

  
  // do not change public interface
  LRUReplacer();

  ~LRUReplacer();

  void Insert(const T &value);

  bool Victim(T &value);

  bool Erase(const T &value);

  size_t Size();

private:
  typedef typename std::list<T>::iterator Iterator;
  std::list<T>List; //用于存放lru
  std::unordered_map<T,Iterator> Map; //用于管理lru的位置，类似于链表，但速度更快
  // add your member variables here
};

} // namespace scudb
