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

#define GET_MASK(pos) (longlong_mask[63-pos])
#ifndef GET_UINT64_
#define GET_UINT64_(ptr) (*((uint64_t*)ptr))
#endif
/**
 * @brief 倒排树
 *
 */
class ListIndex {
private:
    std::vector<QueryMetaTrieTree*> m_list;
    pthread_rwlock_t m_lock;
public:

    explicit ListIndex() {
        pthread_rwlock_init(&m_lock, NULL);
    }
    ~ListIndex(){};

    // 插入数据  固定位数的定长字符
    int Insert2(QueryMetaTrieTree* meta, int thread_id) {
        pthread_rwlock_wrlock(&m_lock);
        m_list.push_back(meta);
        pthread_rwlock_unlock(&m_lock);
        return 1;
    }

    bool Search(QueryMetaTrieTree* meta, Arraylist* removedQuery, int thread_id){
        bool  res = false;
        pthread_rwlock_rdlock(&m_lock);

        for (auto query : m_list) {
#ifdef INVERSE_INDEX
            if (__glibc_likely(query != NULL) && __glibc_unlikely((GET_UINT64_(meta->data) & GET_UINT64_(query->data)) == GET_UINT64_(query->data))
                     && __glibc_likely(meta->timestamp <= query->timestamp)){
                    res = true;
                    break;
                }
#else
            if (__glibc_unlikely((GET_UINT64_(meta->data) & GET_UINT64_(query->data)) == GET_UINT64_(meta->data))
                && __glibc_likely(query->sym == 0) && __glibc_likely(query->timestamp <= meta->timestamp)){
                query->sym |= INVALID_FLAG;
                ArraylistAdd(removedQuery, query);
            }
#endif
        }
        pthread_rwlock_unlock(&m_lock);
        return res;
    }
    int GetTotalSize(){

        return 0;
    }


    void Print(){

    }

    static const indexType type = List_index;
};
