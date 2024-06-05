//
// Created by jyh_2 on 4/8/2024.
//

#ifndef QTREE_TEST_INVERTED_TREE_H
#define QTREE_TEST_INVERTED_TREE_H

#endif //QTREE_TEST_INVERTED_TREE_H
#include <cstddef>
#include <cstdio>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <cstring>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <vector>
#include <list>
#include <map>
#include "Tool/bloom.h"

extern "C" {
#include "../Tool/ArrayList.h"
#include "../Tool/Tools.h"
}
#if defined(OPT_FAT)
#define BITE_ONE 0x01
//#ifndef INVERSE_INDEX
//#define MASK_OPT
//#endif
#define COMPRESS_OPT
#define MAX_LINKED_POSITION (Bloom_table_size * bits_per_char / Bloom_number_of_hashes + 1)
#if Bloom_table_size == 8
using PositionType=int8_t;
#else
using PositionType=int16_t;

#endif
#define PosNan (-1)
//#define PRINT
//#define PRINT_NODE
#ifndef MAX_FOUT
#define MAX_FOUT ((Bloom_table_size * 4))
#endif
#ifndef PER_NODE_NUM
#define PER_NODE_NUM 32
#else
#if PER_NODE_NUM < 0
#undef PER_NODE_NUM
#define PER_NODE_NUM 0
#endif
#endif
thread_local uint64_t search_node_count = 0;
thread_local uint64_t search_level_count = 0;

thread_local uint64_t search_op_count = 0;
thread_local uint64_t last_removed_list_size = 0;

#define INVERSE_ZERO
#define REMOVED_LIST
#define MINI_LOCK
typedef volatile char my_mutex_t;

inline void my_mutex_unlock(my_mutex_t *_l) __attribute__((always_inline));
inline void my_mutex_lock(my_mutex_t *_l) __attribute__((always_inline));
inline int tas(my_mutex_t *lock) __attribute__((always_inline));

/*
 * Non-recursive spinlock. Using `xchg` and `ldstub` as in PostgresSQL.
 */
/* Call blocks and retunrs only when it has the lock. */
inline void my_mutex_lock(my_mutex_t *_l) {
    while(tas(_l)) {
#if defined(__i386__) || defined(__x86_64__)
        __asm__ __volatile__ ("pause\n");
#endif
    }
}

/** Unlocks the lock object. */
inline void my_mutex_unlock(my_mutex_t *_l) {
    *_l = 0;
}

inline int tas(my_mutex_t *lock) {
    register char res = 1;
#if defined(__i386__) || defined(__x86_64__)
    __asm__ __volatile__ (
            "lock xchgb %0, %1\n"
            : "+q"(res), "+m"(*lock)
            :
            : "memory", "cc");
#elif defined(__sparc__)
    __asm__ __volatile__ (
						  "ldstub [%2], %0"
						  : "=r"(res), "+m"(*lock)
						  : "r"(lock)
						  : "memory");
#else
#error TAS not defined for this architecture.
#endif
    return res;
}

#ifdef MINI_LOCK
#define mutex_lock(l) my_mutex_lock(&(l))
#define mutex_unlock(l) my_mutex_unlock(&(l))
#else
#define mutex_lock(l) l.lock()
#define mutex_unlock(l) l.unlock()
#endif

#define GET_UINT64_(ptr) (*((uint64_t*)ptr))

#define GET_MASK(pos) (longlong_mask[63-pos])
inline int getPosition(QueryMetaTrieTree* meta, PositionType* position){
#if Bloom_table_size == 8
    uint64_t  mask = *(uint64_t*) meta->data;
    int num = 0;
    for (int i = 0; i < 64; ++i) {
#ifdef INVERSE_ZERO
        if((mask & GET_MASK(i)) == 0) {
#else
        if(mask & GET_MASK(i)) {
#endif
            position[num ++] = (i);
        }
    }
#else
    uint64_t*  mask = (uint64_t*) meta->data;
    int num = 0;
    for (int i = 0; i < Bloom_table_size / 8; ++i) {
        for (int j = 0; j < 64; ++j) {
#ifdef INVERSE_ZERO
            if((mask[i] & GET_MASK(j)) == 0) {
#else
            if(mask[i] & GET_MASK(j)) {
#endif
                position[num ++] = (i * 64) + j;
            }
        }
    }
#endif
    return num;

}

std::atomic<uint32_t> BFTrieNodeId(0);
class BFTrie;
class BFTrieNode {
public:

    std::vector<QueryMetaTrieTree*> ptr_list_[MAX_FOUT];    // 叶节点使用，存储外部数据指针|标识 用于删除|失效
#ifdef INVERSE_INDEX
    std::vector<QueryMetaTrieTree*> end_node[MAX_FOUT];
#endif
#ifdef REMOVED_LIST
    std::vector<uint16_t> removed_ptr_index[MAX_FOUT];
#endif
#ifdef MINI_LOCK
    my_mutex_t removed_lock_[MAX_FOUT];
#else
    std::mutex removed_lock_[MAX_FOUT];
#endif
    BFTrieNode* children[MAX_FOUT]={NULL};
#ifdef MINI_LOCK
    my_mutex_t lock_ = 0;
#else
    std::mutex  lock_;
#endif
    PositionType pos;
    uint8_t level;
//#ifdef PRINT
//    uint32_t id;
//#endif

//    uint64_t mask_;
    BFTrieNode(){
#ifdef PRINT
//        id = BFTrieNodeId.fetch_add(1);
        BFTrieNodeId.fetch_add(1);
#endif
#ifdef MINI_LOCK
        for (int i = 0; i < MAX_FOUT; ++i) {
            removed_lock_[i]  = 0;
        }
#endif
    }
    BFTrieNode(BFTrieNode* p, PositionType pos) {
        if(p){
            this->level = p->level + 1;
        } else{
            this->level = 0;
        }
#ifdef PRINT
//        id = BFTrieNodeId.fetch_add(1);
        BFTrieNodeId.fetch_add(1);
#endif

#ifdef MASK_OPT
        mask_ = 0;
#endif
        this->pos = pos;
#ifdef MINI_LOCK
        memset((void *) removed_lock_, 0, sizeof (my_mutex_t) * MAX_FOUT);
#endif
    }
#ifdef REMOVED_LIST
    inline void AddRemoveAtomic(PositionType child, std::vector<uint16_t> &removed){
        mutex_lock(removed_lock_[child]);
        for (auto idx:removed) {
            removed_ptr_index[child].push_back(idx);
        }
        mutex_unlock(removed_lock_[child]);
    }
    inline void AddRemoveAtomic(int child, int idx, int &sym){
        mutex_lock(removed_lock_[child]);
        if (__glibc_likely((sym & (DEL_FLAG | INVALID_FLAG))
                    && (~(sym & REMOVED_FLAG)))){
            removed_ptr_index[child].push_back(idx);
            sym|= REMOVED_FLAG;
        }
        mutex_unlock(removed_lock_[child]);
    }
#endif
#ifdef INVERSE_INDEX
    inline bool SearchPtrList(QueryMetaTrieTree *meta, const PositionType* position, const int pos_num, const int idx, Arraylist* removedQuery){
//        rwlock_rdlock(&this->lock_);
        bool res = false;
        for (int i = idx; i < pos_num; ++i) {
            PositionType pos_idx = (position[i] - this->pos)< MAX_FOUT?
                                    (position[i] - this->pos - 1): (MAX_FOUT -1);
            assert(pos_idx < MAX_FOUT);
            auto& list = ptr_list_[pos_idx];
            mutex_lock(removed_lock_[pos_idx]);
            auto count = list.size();
            auto end_list = end_node[pos_idx];
            mutex_unlock(removed_lock_[pos_idx]);
            for (auto query : end_list) {
#if Bloom_table_size == 8
                if (__glibc_likely(query != NULL) && __glibc_unlikely((GET_UINT64_(meta->data) & GET_UINT64_(query->data)) == GET_UINT64_(query->data))
                     && __glibc_likely(meta->timestamp <= query->timestamp)){
                    if (__glibc_unlikely(removedQuery!=NULL)){
                        ArraylistAdd(removedQuery, query);
                    }
#ifdef INVERSE_END_EARLY
                    return true;
#else
                    res = true;
#endif
                }
#else
                if(__glibc_likely(meta->timestamp < query->timestamp)){
                        bool find = true;
                        uint64_t * mask = (uint64_t*) meta->data;
                        for (int k = 0; k < Bloom_table_size /8; ++k) {
                            if(__glibc_likely((mask[k] & GET_UINT64_(query->data)) != GET_UINT64_(query->data))){
                                find = false;
                                break;
                            }
                        }
                        if(__glibc_unlikely(find)){
                            return true;
                        }
                    }
#endif
            }
            for (size_t j = 0; j < count; j ++) {
                auto query = list[j];
                if(__glibc_likely(query != NULL)){
#if Bloom_table_size == 8
                    if ( __glibc_unlikely((GET_UINT64_(meta->data) & GET_UINT64_(query->data)) == GET_UINT64_(query->data))
                        && __glibc_likely(meta->timestamp < query->timestamp)){
                        if (__glibc_unlikely(removedQuery!=NULL)){
                            ArraylistAdd(removedQuery, query);
                        }
//                      rwlock_unlock(&this->lock_);
#ifdef INVERSE_END_EARLY
                        return true;
#else
                        res = true;
#endif
                    }
#else
                    if(__glibc_likely(meta->timestamp < query->timestamp)){
                        bool find = true;
                        uint64_t * mask = (uint64_t*) meta->data;
                        for (int k = 0; k < Bloom_table_size /8; ++k) {
                            if(__glibc_likely((mask[k] & GET_UINT64_(query->data)) != GET_UINT64_(query->data))){
                                find = false;
                                break;
                            }
                        }
                        if(__glibc_unlikely(find)){
                            return true;
                        }
                    }
#endif
                }
            }
            if(__glibc_unlikely(pos_idx == MAX_FOUT -1)){
                break;
            }
        }
        return res;
//        rwlock_unlock(&this->lock_);
#elif defined(INVERSE_ZERO)
    inline bool SearchPtrList(QueryMetaTrieTree *meta, const PositionType* position, const int pos_num, const int idx, Arraylist* removedQuery){
//        rwlock_rdlock(&this->lock_);
        bool res = false;
        for (int i = idx; i < pos_num; ++i) {
            PositionType pos_idx = (position[i] - this->pos)< MAX_FOUT?
                                   (position[i] - this->pos - 1): (MAX_FOUT -1);
            assert(pos_idx < MAX_FOUT);
            auto& list = ptr_list_[pos_idx];
            mutex_lock(removed_lock_[pos_idx]);
            auto count = list.size();
            mutex_unlock(removed_lock_[pos_idx]);

            for (size_t j = 0; j < count; j ++) {
                auto query = list[j];
#if Bloom_table_size == 8
#ifdef REMOVED_LIST
                if (__glibc_unlikely((GET_UINT64_(meta->data) & GET_UINT64_(query->data)) == GET_UINT64_(meta->data)) && __glibc_likely(query->sym == 0)){
                    query->sym |= INVALID_FLAG;
                    AddRemoveAtomic(pos_idx, j, query->sym);
                    ArraylistAdd(removedQuery, query);
                }else if(__glibc_unlikely(query->sym == DEL_FLAG)){
                    AddRemoveAtomic(pos_idx,j, query->sym);
                }
#else
                if (__glibc_unlikely((GET_UINT64_(meta->data) & GET_UINT64_(query->data)) == GET_UINT64_(meta->data)) && __glibc_likely(query->sym == 0)){
                        query->sym |= REMOVED_FLAG;
                        ArraylistAdd(removedQuery, query);
                    }
#endif
#else
                if(__glibc_likely(query->sym == 0)){
                        bool find = true;
                        uint64_t * mask = (uint64_t*) meta->data;
                        for (int k = 0; k < Bloom_table_size /8; ++k) {
                            uint64_t meta_mask = mask[k];
                            if(__glibc_likely((meta_mask & GET_UINT64_(query->data)) != meta_mask)){
                                find = false;
                                break;
                            }
                        }
#ifdef RREMOVED_LIST
                        if(__glibc_unlikely(find)){
                            query->sym |= INVALID_FLAG;
                            AddRemoveAtomic(pos_idx, j, query->sym);
                            ArraylistAdd(removedQuery, query);
                        }
#else
                        if(__glibc_unlikely(find)){
                            query->sym |= REMOVED_FLAG;
                            ArraylistAdd(removedQuery, query);
                        }
#endif
                }
#ifdef RREMOVED_LIST
                else if(__glibc_unlikely(query->sym == DEL_FLAG)){
                    AddRemoveAtomic(pos_idx,j, query->sym);
                }
#endif
#endif
            }
            if(__glibc_unlikely(pos_idx == MAX_FOUT -1)){
                break;
            }
        }
        return res;
#else
    inline void SearchPtrList(QueryMetaTrieTree *meta, const int next_pos, Arraylist* removedQuery){
//        rwlock_rdlock(&this->lock_);

        for (int i = 0; i <= next_pos; ++i) {
            mutex_lock(removed_lock_[i]);
            auto &list = ptr_list_[i];
            auto count = list.size();
            mutex_unlock(removed_lock_[i]);
            assert(count <= list.size());
            for (size_t j = 0; j < count; j ++) {
                auto query = list[j];
                if(__glibc_likely(query != NULL)){
#if Bloom_table_size == 8
#ifdef REMOVED_LIST
                    if (__glibc_unlikely((GET_UINT64_(meta->data) & GET_UINT64_(query->data)) == GET_UINT64_(meta->data))
                        && __glibc_likely(query->sym == 0) && __glibc_likely(query->timestamp <= meta->timestamp)){
                        query->sym |= INVALID_FLAG;
                        AddRemoveAtomic(i, j, query->sym);
                        ArraylistAdd(removedQuery, query);
                    }else if(__glibc_unlikely(query->sym == DEL_FLAG)){
                        AddRemoveAtomic(i,j, query->sym);
                    }
#else
                    if (__glibc_unlikely((GET_UINT64_(meta->data) & GET_UINT64_(query->data)) == GET_UINT64_(meta->data)) && __glibc_likely(query->sym == 0)){
                        query->sym |= REMOVED_FLAG;
                        ArraylistAdd(removedQuery, query);
                    }
#endif
#else
                    if(__glibc_likely(query->sym == 0)){
                        bool find = true;
                        uint64_t * mask = (uint64_t*) meta->data;
                        for (int k = 0; k < Bloom_table_size /8; ++k) {
                            uint64_t meta_mask = mask[k];
                            if(__glibc_likely((meta_mask & GET_UINT64_(query->data)) != meta_mask)){
                                find = false;
                                break;
                            }
                        }
#ifdef RREMOVED_LIST
                        if(__glibc_unlikely(find)){
                            query->sym |= INVALID_FLAG;
                            AddRemoveAtomic(i, j, query->sym);
                            ArraylistAdd(removedQuery, query);
                        }
#else
                        if(__glibc_unlikely(find)){
                            query->sym |= REMOVED_FLAG;
                            ArraylistAdd(removedQuery, query);
                        }
#endif
                    }
#ifdef RREMOVED_LIST
                    else if(__glibc_unlikely(query->sym == DEL_FLAG)){
                        AddRemoveAtomic(i,j, query->sym);
                    }
#endif
#endif
                }
            }
        }
//        rwlock_unlock(&this->lock_);
#endif
    }
    inline bool Search(QueryMetaTrieTree *meta, const PositionType* position, const int pos_num, const int idx, Arraylist* removedQuery);
    inline void InsertIntoMask(QueryMetaTrieTree* meta) {
#ifdef MASK_OPT
        this->mask_ |= GET_UINT64_(meta->data);
#endif
    }

    inline bool Insert(QueryMetaTrieTree* meta, PositionType* position, int pos_num, int pos_idx){
        auto idx =  (position[pos_idx] - this->pos - 1) < MAX_FOUT ? (position[pos_idx] - this->pos - 1) : MAX_FOUT - 1;
#ifdef REMOVED_LIST
        if(removed_ptr_index[idx].size() > 0){
            mutex_lock(removed_lock_[idx]);
            if(removed_ptr_index[idx].size() > 0){
                this->ptr_list_[idx][removed_ptr_index[idx].back()] = meta;
                removed_ptr_index[idx].pop_back();
                mutex_unlock(removed_lock_[idx]);
                return true;
            }else{
                mutex_unlock(removed_lock_[idx]);
                return Insert(meta, position, pos_num, pos_idx);
            }
        }
#endif
#ifdef INVERSE_INDEX
        if (pos_idx == pos_num -1){
            mutex_lock(removed_lock_[idx]);
            auto cur_meta = GET_UINT64_(meta);
            for (auto query : end_node[idx]) {
                if (__glibc_likely(GET_UINT64_(query->data) == cur_meta)
                && __glibc_likely(query->queryId == meta->queryId)){
                    if( __glibc_likely(meta->timestamp > query->timestamp)){
                        query->timestamp  = meta->timestamp;
                    }
                    mutex_unlock(removed_lock_[idx]);
                    return true;
                }
            }
            end_node[idx].push_back(meta);
            mutex_unlock(removed_lock_[idx]);
            return true;
        }
#endif
        if(this->ptr_list_[idx].size() < PER_NODE_NUM || pos_idx == pos_num-1){
            mutex_lock(removed_lock_[idx]);
            this->ptr_list_[idx].push_back(meta);
            mutex_unlock(removed_lock_[idx]);
            return true;
        }
        if (__glibc_unlikely(!children[idx])) {
            mutex_lock(this->lock_);
            children[idx] = new BFTrieNode(this, this->pos + idx + 1);
//            ptr->Insert(meta, position, pos_num);
            mutex_unlock(this->lock_);
//            return true;
        }
        return false;
    }

    ~BFTrieNode() = default;
};

/**
 * @brief 倒排树
 *
 */
class BFTrie {
private:
    BFTrieNode* root_;    // 根节点
public:

    BFTrie();
    explicit BFTrie(BFTrieNode *root) : root_(root) {
    }
    ~BFTrie();

    // 插入数据  固定位数的定长字符
    int Insert2(QueryMetaTrieTree* meta, int thread_id) {
        BFTrieNode* node = root_;
        PositionType position[Bloom_size];
        int pos_num = getPosition(meta, position);
        for (int pos_i = 0; pos_i < pos_num;) {
            if (node->Insert(meta, position, pos_num, pos_i)){
                return 1;
            }
            node = node->children[position[pos_i] - node->pos - 1< MAX_FOUT? (position[pos_i] - node->pos - 1) : (MAX_FOUT - 1)];
#if defined(INVERSE_INDEX) || defined(INVERSE_ZERO)
            ++ pos_i;
#else
            if (position[pos_i] == node->pos){
                ++ pos_i;
            }
#endif
        }

        return 1;
    }

    bool Search(QueryMetaTrieTree* meta, Arraylist* removedQuery, int thread_id);
    int GetTotalSize(){
        uint64_t total_size = 0;
        std::vector<BFTrieNode*> continue_node;
        std::vector<BFTrieNode*> tmp_continue_node;
        continue_node.push_back(root_);

        while (!continue_node.empty()){
            for (auto node:continue_node) {

                for (int i = 0; i < MAX_FOUT; ++i) {
                    total_size += (node->ptr_list_[i].size());
#ifdef REMOVED_LIST
                    total_size -= (node->removed_ptr_index[i].size());
#endif
#ifdef INVERSE_INDEX
                    total_size += (node->end_node[i].size());
#endif
                }
                for (int i = 0; i < MAX_FOUT; ++i) {
                    if(node->children[i]){
                        tmp_continue_node.push_back(node->children[i]);
                    }
                }
            }

            continue_node = tmp_continue_node;
            tmp_continue_node.clear();
        }
        return total_size;
    }


    void Print(){
#ifdef PRINT

//        std::cout << "BFTrie node num:" << BFTrieNodeId << ", total_size:" << GetTotalSize()<< std::endl;

#endif
#ifdef PRINT_NODE
        std::vector<BFTrieNode*> continue_node;
        std::vector<BFTrieNode*> tmp_continue_node;
        continue_node.push_back(root_);

        while (!continue_node.empty()){
            for (auto node:continue_node) {
                std::cout << node->id << "(" << (int) node->level << ","
                          << (int) node->pos << ")";
                std::cout << "(";
                for (int i = 0; i < MAX_FOUT; ++i) {
                    std::cout << node->ptr_list_[i].size() << ",";

                }
                std::cout << "):";
                for (int i = 0; i < MAX_FOUT; ++i) {
                    if(node->children[i]){
                        std::cout << "" << i << ":" << node->children[i]->id << ",";
                        tmp_continue_node.push_back(node->children[i]);
                    }
                }
                std::cout << std::endl;
            }

            continue_node = tmp_continue_node;
            tmp_continue_node.clear();
        }
#endif
    }

    // 打印树中的所有节点
    // void PrintAll();
    // // 打印以**为前缀的序列
    // void PrintPre(std::string str);
    // 输出以root为根的所有数据
    // void PrintDef(BFTrieNode* node);
    void Destroy(BFTrieNode* root);
    static const indexType type = BF_Trie;
};
BFTrie::BFTrie() {
    root_ = new BFTrieNode((BFTrieNode*)NULL, PosNan);
    // root_ = (BFTrieNode *)malloc(sizeof(BFTrieNode));
}
BFTrie::~BFTrie() { Destroy(root_); }



inline bool BFTrieNode::Search(QueryMetaTrieTree *meta, const PositionType* position, const int pos_num, const int idx, Arraylist* removedQuery) {
#if defined(INVERSE_INDEX) || defined(INVERSE_ZERO)
    bool res = false;
    if (__glibc_unlikely(idx >= pos_num)){
        return false;
    }
#ifdef PRINT
    search_node_count ++;
    search_level_count += this->level + 1;
#endif
    if(SearchPtrList(meta, position, pos_num, idx, removedQuery)){
#if defined(INVERSE_END_EARLY) && !defined(INVERSE_ZERO)
        return true;
#else
        res = true;
#endif
    }
    int i = __glibc_unlikely(position[idx] == this->pos)?(idx + 1) : idx;
    for (; i < pos_num && (position[i] - this->pos - 1)< MAX_FOUT; ++i) {
        int pos_idx = (position[i] - this->pos - 1);
        if (children[pos_idx]){
            if(children[pos_idx]->Search(meta, position, pos_num, i + 1, removedQuery)){
#if defined(INVERSE_END_EARLY) && !defined(INVERSE_ZERO)
                return true;
#else
                res = true;
#endif
            }
        }
    }
    if (__glibc_unlikely(i < pos_num) && children[MAX_FOUT-1]){
        return children[MAX_FOUT-1]->Search(meta, position, pos_num, i, removedQuery) || res;
    }else{
        return res;
    }

}
#else
#ifdef PRINT
    search_node_count ++;
    search_level_count += this->level + 1;
#endif
    if (__glibc_unlikely(idx >= pos_num)){
        SearchPtrList(meta, MAX_FOUT - 1, removedQuery);
        for (int i =0; i < MAX_FOUT; ++i) {
            if (__glibc_unlikely(children[i] != NULL)){
                children[i]->Search(meta, position, pos_num, idx, removedQuery);
            }
        }
//        std::cout <<idx << ", " << (int)this->level << std::endl;
        return false;
    }
    auto next_pos = (position[idx] - this->pos) < MAX_FOUT? (position[idx] - this->pos -1) : (MAX_FOUT - 1);

    SearchPtrList(meta, next_pos, removedQuery);
    if (__glibc_likely(children[next_pos] != NULL)){
        children[next_pos]->Search(meta, position, pos_num, (position[idx] == this->pos + 1 + next_pos)? (idx + 1) : idx, removedQuery);
    }
    for (int i = 0; i < next_pos; ++i) {
        if(children[i]){
            children[i]->Search(meta, position, pos_num, idx, removedQuery);
        }
    }
    return false;
}
#endif
inline bool BFTrie::Search(QueryMetaTrieTree* meta, Arraylist* removedQuery, int thread_id) {
    PositionType position[Bloom_size];
    int pos_num = getPosition(meta, position);
#ifdef INVERSE_INDEX
    return root_->Search(meta, position, pos_num, 0, removedQuery);
#else
    return root_->Search(meta, position, pos_num, 0, removedQuery);
#endif
}


/**
 * @brief 销毁树结构
 *
 * @param root
 */
inline void BFTrie::Destroy(BFTrieNode* root) {
    if(root == nullptr) {
        return;
    }
    delete root;
    // 树的根节点重新赋值 | 避免下次使用或调用 避免堆栈异常
    root = nullptr;
}
#endif
