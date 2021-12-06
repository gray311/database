/**
 * b_plus_tree.cpp
 */
#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <bits/stdc++.h>
#include "common/exception.h"
#include "common/logger.h"
#include "common/rid.h"
#include "index/b_plus_tree.h"
#include "page/header_page.h"

namespace scudb {

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(const std::string &name,
                          BufferPoolManager *buffer_pool_manager,
                          const KeyComparator &comparator,
                          page_id_t root_page_id)
        : index_name_(name), root_page_id_(root_page_id),
          buffer_pool_manager_(buffer_pool_manager), comparator_(comparator) {}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::IsEmpty() const { return root_page_id_ == INVALID_PAGE_ID; }

template <typename KeyType, typename ValueType, typename KeyComparator>
thread_local int BPlusTree<KeyType, ValueType, KeyComparator>::rootLockedCnt = 0;
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::GetValue(const KeyType &key,
                              std::vector<ValueType> &result,
                              Transaction *transaction) {
  B_PLUS_TREE_LEAF_PAGE_TYPE *tar = FindLeafPage(key,false,OpType::READ,transaction);
  if (tar == nullptr)
    return false;
  result.resize(1);
  auto ret = tar->Lookup(key,result[0],comparator_);
  FreePagesInTransaction(false,transaction,tar->GetPageId());
  return ret;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value std::std::pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value,
                            Transaction *transaction) {
  LockRootPageId(true);
  if (IsEmpty()) {
    StartNewTree(key,value);
    TryUnlockRootPageId(true);
    return true;
  }
  TryUnlockRootPageId(true);
  bool res = InsertIntoLeaf(key,value,transaction);
  
  return res;
}
/*
 * Insert constant key & value std::std::pair into an empty tree
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then update b+
 * tree's root page id and insert entry directly into leaf page.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::StartNewTree(const KeyType &key, const ValueType &value) {
  
  page_id_t newPageId;
  Page *rootPage = buffer_pool_manager_->NewPage(newPageId);
  assert(rootPage != nullptr);
  // 叶结点
  B_PLUS_TREE_LEAF_PAGE_TYPE *root = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(rootPage->GetData());
  // 要更新根节点页面id
  root->Init(newPageId,INVALID_PAGE_ID);
  root_page_id_ = newPageId;
  UpdateRootPageId(true);
  
  root->Insert(key,value,comparator_);

  buffer_pool_manager_->UnpinPage(newPageId,true);
}

/*
 * Insert constant key & value std::pair into leaf page
 * User needs to first find the right leaf page as insertion target, then look
 * through leaf page to see whether insert key exist or not. If exist, return
 * immdiately, otherwise insert entry. Remember to deal with split if necessary.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::InsertIntoLeaf(const KeyType &key, const ValueType &value,
                                    Transaction *transaction) {
  B_PLUS_TREE_LEAF_PAGE_TYPE *leafPage = FindLeafPage(key,false,OpType::INSERT,transaction);
  ValueType v;
  bool exist = leafPage->Lookup(key,v,comparator_);
  // 如果树中已经有值了，就返回false
  if (exist) {
    FreePagesInTransaction(true,transaction);
    return false;
  }
  // 不需要分裂就直接插入
  leafPage->Insert(key,value,comparator_);

  if (leafPage->GetSize() > leafPage->GetMaxSize()) {
    B_PLUS_TREE_LEAF_PAGE_TYPE *newLeafPage = Split(leafPage,transaction);
    // 将分裂的节点插入到父节点
    InsertIntoParent(leafPage,newLeafPage->KeyAt(0),newLeafPage,transaction);
  }
  
  FreePagesInTransaction(true,transaction);
  return true;
}

/*
 * Split input page and return newly created page.
 * Using template N to represent either internal page or leaf page.
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then move half
 * of key & value std::pairs from input page to newly created page
 */
 /*
root node也满了,需要insert parnet,但是root没有parent,那需要新建一个root,之前的root变成了非root的结点(此时需要更新root_page_id).我们还知道递归一定要有一个递归终止点,那这种case往往是最终的终止点,因为不可能从root再往上递归了。
不是root node,并且其父结点有足够的空间被insert。
不是root node,其父结点没有空间提供insert,此时需要split当前结点的父结点。难点在于此处:为什么我们不可以直接split该父结点,而是先生成一个该父结点的copy,与该父结点不同之处在于copy不含old_node，而是含有new_node.然后再split该copy。将split后的再copy到父结点中
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N> N *BPLUSTREE_TYPE::Split(N *node, Transaction *transaction) {
  
  page_id_t newPageId;
  Page* const newPage = buffer_pool_manager_->NewPage(newPageId);
  assert(newPage != nullptr);
  newPage->WLatch();
  transaction->AddIntoPageSet(newPage);
  
  N *newNode = reinterpret_cast<N *>(newPage->GetData());
  newNode->Init(newPageId, node->GetParentPageId());
  node->MoveHalfTo(newNode, buffer_pool_manager_);
  
  return newNode;
}

/*
 * Insert key & value std::pair into internal page after split
 * @param   old_node      input page from split() method
 * @param   key
 * @param   new_node      returned page from split() method
 * User needs to first find the parent page of old_node, parent node must be
 * adjusted to take info of new_node into account. Remember to deal with split
 * recursively if necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *old_node,
                                      const KeyType &key,
                                      BPlusTreePage *new_node,
                                      Transaction *transaction) {
  // 如果old_node是根节点，则需要新生成一个根页面
  if (old_node->IsRootPage()) {
    Page* const newPage = buffer_pool_manager_->NewPage(root_page_id_);
    assert(newPage != nullptr);
    assert(newPage->GetPinCount() == 1);
    B_PLUS_TREE_INTERNAL_PAGE *newRoot = reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE *>(newPage->GetData());
    newRoot->Init(root_page_id_);
    newRoot->PopulateNewRoot(old_node->GetPageId(),key,new_node->GetPageId());
    old_node->SetParentPageId(root_page_id_);
    new_node->SetParentPageId(root_page_id_);
    UpdateRootPageId();
    
    
    buffer_pool_manager_->UnpinPage(newRoot->GetPageId(),true);
    return;
  }
  page_id_t parentId = old_node->GetParentPageId();
  auto *page = FetchPage(parentId);
  assert(page != nullptr);
  B_PLUS_TREE_INTERNAL_PAGE *parent = reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE *>(page);
  new_node->SetParentPageId(parentId);
  
  
  parent->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
  // 如果父节点还有空间
  if (parent->GetSize() > parent->GetMaxSize()) {
    
    B_PLUS_TREE_INTERNAL_PAGE *newLeafPage = Split(parent,transaction);
    InsertIntoParent(parent,newLeafPage->KeyAt(0),newLeafPage,transaction);
  }
  buffer_pool_manager_->UnpinPage(parentId,true);
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value std::pair associated with input key
 * If current tree is empty, return immdiately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {
  if (IsEmpty()) return;
  B_PLUS_TREE_LEAF_PAGE_TYPE *delTar = FindLeafPage(key,false,OpType::DELETE,transaction);
  int curSize = delTar->RemoveAndDeleteRecord(key,comparator_);
  if (curSize < delTar->GetMinSize()) {
    CoalesceOrRedistribute(delTar,transaction);
  }
  FreePagesInTransaction(true,transaction);
  
}

/*
 * User needs to first find the sibling of input page. If sibling's size + input
 * page's size > page's max size, then redistribute. Otherwise, merge.
 * Using template N to represent either internal page or leaf page.
 * @return: true means target leaf page should be deleted, false means no
 * deletion happens
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::CoalesceOrRedistribute(N *node, Transaction *transaction) {
  
  if (node->IsRootPage()) {
    bool delOldRoot = AdjustRoot(node);
    if (delOldRoot) {transaction->AddIntoDeletedPageSet(node->GetPageId());}
    return delOldRoot;
  }
  
  N *node2;
  bool isRightSib = FindLeftSibling(node,node2,transaction);
  BPlusTreePage *parent = FetchPage(node->GetParentPageId());
  B_PLUS_TREE_INTERNAL_PAGE *parentPage = static_cast<B_PLUS_TREE_INTERNAL_PAGE *>(parent);
  
  if (node->GetSize() + node2->GetSize() <= node->GetMaxSize()) {
    if (isRightSib) {std::swap(node,node2);} 
    int removeIndex = parentPage->ValueIndex(node->GetPageId());
    Coalesce(node2,node,parentPage,removeIndex,transaction);
    buffer_pool_manager_->UnpinPage(parentPage->GetPageId(), true);
    return true;
  }
  /* Redistribution: borrow an entry from N2 */
  int nodeInParentIndex = parentPage->ValueIndex(node->GetPageId());
  Redistribute(node2,node,nodeInParentIndex);
  buffer_pool_manager_->UnpinPage(parentPage->GetPageId(), false);
  return false;
}

INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::FindLeftSibling(N *node, N * &sibling, Transaction *transaction) {
  auto page = FetchPage(node->GetParentPageId());
  B_PLUS_TREE_INTERNAL_PAGE *parent = reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE *>(page);
  int index = parent->ValueIndex(node->GetPageId());
  int siblingIndex = index - 1;
  if (index == 0) { 
    siblingIndex = index + 1;
  }
  sibling = reinterpret_cast<N *>(CrabingProtocalFetchPage(
          parent->ValueAt(siblingIndex),OpType::DELETE,-1,transaction));
  buffer_pool_manager_->UnpinPage(parent->GetPageId(), false);
  return index == 0;
}

/*
 * Move all the key & value std::pairs from one page to its sibling page, and notify
 * buffer pool manager to delete this page. Parent page must be adjusted to
 * take info of deletion into account. Remember to deal with coalesce or
 * redistribute recursively if necessary.
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 * @param   parent             parent page of input "node"
 * @return  true means parent node should be deleted, false means no deletion
 * happend
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::Coalesce(
        N *&neighbor_node, N *&node,
        BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *&parent,
        int index, Transaction *transaction) {
  
  assert(node->GetSize() + neighbor_node->GetSize() <= node->GetMaxSize());
  
  node->MoveAllTo(neighbor_node,index,buffer_pool_manager_);
  transaction->AddIntoDeletedPageSet(node->GetPageId());
  parent->Remove(index);
  if (parent->GetSize() <= parent->GetMinSize()) {
    return CoalesceOrRedistribute(parent,transaction);
  }
  return false;
}

/*
 * Redistribute key & value std::pairs from one page to its sibling page. If index ==
 * 0, move sibling page's first key & value std::pair into end of input "node",
 * otherwise move sibling page's last key & value std::pair into head of input
 * "node".
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
void BPLUSTREE_TYPE::Redistribute(N *neighbor_node, N *node, int index) {
  if (index == 0) {
    neighbor_node->MoveFirstToEndOf(node,buffer_pool_manager_);
  } else {
    neighbor_node->MoveLastToFrontOf(node, index, buffer_pool_manager_);
  }
}
/*
 * Update root page if necessary
 * NOTE: size of root page can be less than min size and this method is only
 * called within coalesceOrRedistribute() method
 * case 1: when you delete the last element in root page, but root page still
 * has one last child
 * case 2: when you delete the last element in whole b+ tree
 * @return : true means root page should be deleted, false means no deletion
 * happend
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::AdjustRoot(BPlusTreePage *old_root_node) {
  
  // 如果root是叶结点并且其中还有KV,就不需要调整。
  // 若没有KV了,就把该page设置为INVALID,即没有这棵B+Tree了
  
  if (old_root_node->IsLeafPage()){
    if (old_root_node->GetSize() == 0){
      root_page_id_ = INVALID_PAGE_ID;
      UpdateRootPageId(false);
      return true;
    }
    return false;
  }

  // root是内部结点并且还有最后一个节点,就将最后剩下的结点设置为新的root结点
  if (old_root_node->GetSize() == 1){
    auto root =
        reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t,
                                               KeyComparator> *>(old_root_node);
    root_page_id_ = root->ValueAt(0);
    // 更新root_page_id到disk中去
    UpdateRootPageId(false); 
    // 拿到对应的叶结点,即new_root
    auto *page = buffer_pool_manager_->FetchPage(root_page_id_);
    if (page == nullptr){
      throw Exception(EXCEPTION_TYPE_INDEX, "all page are pinned while AdjustRoot");
    }
    auto new_root = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t,KeyComparator> *>(page->GetData());
    new_root->SetParentPageId(INVALID_PAGE_ID);
    buffer_pool_manager_->UnpinPage(root_page_id_, true);
    return true;
  }
  return false;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leaftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin() {
  KeyType useless;
  auto start_leaf = FindLeafPage(useless, true);
  TryUnlockRootPageId(false);
  return INDEXITERATOR_TYPE(start_leaf, 0, buffer_pool_manager_);
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin(const KeyType &key) {
  auto start_leaf = FindLeafPage(key);
  TryUnlockRootPageId(false);
  if (start_leaf == nullptr) {
    return INDEXITERATOR_TYPE(start_leaf, 0, buffer_pool_manager_);
  }
  int idx = start_leaf->KeyIndex(key,comparator_);
  return INDEXITERATOR_TYPE(start_leaf, idx, buffer_pool_manager_);
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Find leaf page containing particular key, if leftMost flag == true, find
 * the left most leaf page
 */
INDEX_TEMPLATE_ARGUMENTS
B_PLUS_TREE_LEAF_PAGE_TYPE *BPLUSTREE_TYPE::FindLeafPage(const KeyType &key,
                                                         bool leftMost,OpType op,
                                                         Transaction *transaction) {
  // 如果操作不是只读的，就要锁根节点
  bool exclusive = (op != OpType::READ);
  LockRootPageId(exclusive);
  if (IsEmpty()) {
    TryUnlockRootPageId(exclusive);
    return nullptr;
  }
  
   //拿到root结点并加锁
  auto pointer = CrabingProtocalFetchPage(root_page_id_,op,-1,transaction);
  page_id_t next;

  //迭代遍历
  for (page_id_t cur = root_page_id_; !pointer->IsLeafPage(); pointer =
          CrabingProtocalFetchPage(next,op,cur,transaction),cur = next) {
    B_PLUS_TREE_INTERNAL_PAGE *internalPage = static_cast<B_PLUS_TREE_INTERNAL_PAGE *>(pointer);
    if (leftMost) {
      next = internalPage->ValueAt(0);
    }else {
      next = internalPage->Lookup(key,comparator_);
    }
  }
  return static_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(pointer);
}
INDEX_TEMPLATE_ARGUMENTS
BPlusTreePage *BPLUSTREE_TYPE::FetchPage(page_id_t page_id) {
  auto page = buffer_pool_manager_->FetchPage(page_id);
  return reinterpret_cast<BPlusTreePage *>(page->GetData());
}
INDEX_TEMPLATE_ARGUMENTS
BPlusTreePage *BPLUSTREE_TYPE::CrabingProtocalFetchPage(page_id_t page_id,OpType op,page_id_t previous, Transaction *transaction) {
  bool exclusive = op != OpType::READ;
  auto page = buffer_pool_manager_->FetchPage(page_id);
  Lock(exclusive,page);
  auto treePage = reinterpret_cast<BPlusTreePage *>(page->GetData());
  if (previous > 0 && (!exclusive || treePage->IsSafe(op))) {
    FreePagesInTransaction(exclusive,transaction,previous);
  }
  if (transaction != nullptr)
    transaction->AddIntoPageSet(page);
  return treePage;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::FreePagesInTransaction(bool exclusive, Transaction *transaction, page_id_t cur) {
  TryUnlockRootPageId(exclusive);
  if (transaction == nullptr) {
    assert(!exclusive && cur >= 0);
    Unlock(false,cur);
    buffer_pool_manager_->UnpinPage(cur,false);
    return;
  }
  for (Page *page : *transaction->GetPageSet()) {
    int curPid = page->GetPageId();
    Unlock(exclusive,page);
    buffer_pool_manager_->UnpinPage(curPid,exclusive);
    if (transaction->GetDeletedPageSet()->find(curPid) != transaction->GetDeletedPageSet()->end()) {
      buffer_pool_manager_->DeletePage(curPid);
      transaction->GetDeletedPageSet()->erase(curPid);
    }
  }
  assert(transaction->GetDeletedPageSet()->empty());
  transaction->GetPageSet()->clear();
}

/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      defualt value is false. When set to true,
 * insert a record <index_name, root_page_id> into header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
  HeaderPage *header_page = static_cast<HeaderPage *>(
          buffer_pool_manager_->FetchPage(HEADER_PAGE_ID));
  // header_page相当于一个目录,保存了所有的index及其对应的root_page_id

  // 判断我们是新建了一个index,还是只是更新之前的root_page_id
  if (insert_record){
     // create a new record<index_name + root_page_id> in header_page
    header_page->InsertRecord(index_name_, root_page_id_);
  }
  else{
    // update root_page_id in header_page
    header_page->UpdateRecord(index_name_, root_page_id_);
  }
  buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID, true);
}

/*
 * This method is used for debug only
 * print out whole b+tree sturcture, rank by rank
 */
INDEX_TEMPLATE_ARGUMENTS
std::string BPLUSTREE_TYPE::ToString(bool verbose) {
  if (IsEmpty()) {
    return "Empty tree";
  }
  std::queue<BPlusTreePage *> todo, tmp;
  std::stringstream tree;
  auto node = reinterpret_cast<BPlusTreePage *>(
          buffer_pool_manager_->FetchPage(root_page_id_));
  if (node == nullptr) {
    throw Exception(EXCEPTION_TYPE_INDEX,
                    "all page are pinned while printing");
  }
  todo.push(node);
  bool first = true;
  while (!todo.empty()) {
    node = todo.front();
    if (first) {
      first = false;
      tree << "| ";
    }
    
    if (node->IsLeafPage()) {
      auto page = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(node);
      tree << page->ToString(verbose) <<"("<<node->GetPageId()<< ")| ";
    } else {
      auto page = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(node);
      tree << page->ToString(verbose) <<"("<<node->GetPageId()<< ")| ";
      page->QueueUpChildren(&tmp, buffer_pool_manager_);
    }
    todo.pop();
    if (todo.empty() && !tmp.empty()) {
      todo.swap(tmp);
      tree << '\n';
      first = true;
    }
    
    buffer_pool_manager_->UnpinPage(node->GetPageId(), false);
  }
  return tree.str();
}

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name,
                                    Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;

    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, transaction);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name,
                                    Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, transaction);
  }
}


/***************************************************************************
 *  Check integrity of B+ tree data structure.
 ***************************************************************************/

INDEX_TEMPLATE_ARGUMENTS
int BPLUSTREE_TYPE::isBalanced(page_id_t pid) {
  if (IsEmpty()) return true;
  auto node = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(pid));
  if (node == nullptr) {
    throw Exception(EXCEPTION_TYPE_INDEX,"all page are pinned while isBalanced");
  }
  int ret = 0;
  if (!node->IsLeafPage())  {
    auto page = reinterpret_cast<B_PLUS_TREE_INTERNAL_PAGE *>(node);
    int last = -2;
    for (int i = 0; i < page->GetSize(); i++) {
      int cur = isBalanced(page->ValueAt(i));
      if (cur >= 0 && last == -2) {
        last = cur;
        ret = last + 1;
      }else if (last != cur) {
        ret = -1;
        break;
      }
    }
  }
  buffer_pool_manager_->UnpinPage(pid,false);
  return ret;
}

INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::isPageCorr(page_id_t pid,std::pair<KeyType,KeyType> &out) {
  if (IsEmpty()) return true;
  auto node = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager_->FetchPage(pid));
  if (node == nullptr) {
    throw Exception(EXCEPTION_TYPE_INDEX,"all page are pinned while isPageCorr");
  }
  bool ret = true;
  if (node->IsLeafPage())  {
    auto page = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(node);
    int size = page->GetSize();
    ret = ret && (size >= node->GetMinSize() && size <= node->GetMaxSize());
    for (int i = 1; i < size; i++) {
      if (comparator_(page->KeyAt(i-1), page->KeyAt(i)) > 0) {
        ret = false;
        break;
      }
    }
    out = std::pair<KeyType,KeyType>{page->KeyAt(0),page->KeyAt(size-1)};
  } else {
    auto page = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(node);
    int size = page->GetSize();
    ret = ret && (size >= node->GetMinSize() && size <= node->GetMaxSize());
    std::pair<KeyType,KeyType> left,right;
    for (int i = 1; i < size; i++) {
      if (i == 1) {
        ret = ret && isPageCorr(page->ValueAt(0),left);
      }
      ret = ret && isPageCorr(page->ValueAt(i),right);
      ret = ret && (comparator_(page->KeyAt(i) ,left.second)>0 && comparator_(page->KeyAt(i), right.first)<=0);
      ret = ret && (i == 1 || comparator_(page->KeyAt(i-1) , page->KeyAt(i)) < 0);
      if (!ret) break;
      left = right;
    }
    out = std::pair<KeyType,KeyType>{page->KeyAt(0),page->KeyAt(size-1)};
  }
  buffer_pool_manager_->UnpinPage(pid,false);
  return ret;
}

INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::Check(bool forceCheck) {
  if (!forceCheck && !openCheck) {
    return true;
  }
  std::pair<KeyType,KeyType> in;
  bool isPageInOrderAndSizeCorr = isPageCorr(root_page_id_, in);
  bool isBal = (isBalanced(root_page_id_) >= 0);
  bool isAllUnpin = buffer_pool_manager_->CheckAllUnpined();
  if (!isPageInOrderAndSizeCorr) printf("problem in page order or page size\n");
  if (!isBal) printf("problem in balance\n");
  if (!isAllUnpin) printf("problem in page unpin\n");
  return isPageInOrderAndSizeCorr && isBal && isAllUnpin;
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

} 
