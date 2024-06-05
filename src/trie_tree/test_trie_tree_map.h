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
#include "Tool/bloom.h"

extern "C" {
#include "../Tool/ArrayList.h"
#include "../Tool/Tools.h"
}

#define BITE_ONE 0x01
#define EACH_NODE_CHILD_NODE_SIZE  256
//#define TRIE_MASK_OPT
std::atomic<int> trie_min_ones_count_{Bloom_table_size * bits_per_char};

/**
 * @brief 前缀树节点
 * 
 */
class TrieNode {
public:
    unsigned char key_; // 当前节点的key 无符号cha类型
    uint64_t path_=0;      // 该字符有多少次路线经过，用以统计以该字符作为前缀的路径个数
    char end_;
#ifdef TRIE_MASK_OPT
    char mask_[Bloom_table_size];
#endif
    std::vector<QueryMetaTrieTree*> ptr_list_{};    // 叶节点使用，存储外部数据指针|标识 用于删除|失效
    
    std::unordered_map<unsigned char, TrieNode*> maps_;

    pthread_rwlock_t lock_;

    TrieNode() {
#ifdef TRIE_MASK_OPT
        memset(mask_,0, sizeof(mask_));
#endif
        path_ = 0;
        end_ = '0';
        pthread_rwlock_init(&lock_, NULL);
    }
    explicit TrieNode(unsigned char ch) {
        path_ = 0;
    }

    void InsertIntoMask(QueryMetaTrieTree* meta) {
#ifdef TRIE_MASK_OPT
        for (int i = 0; i < Bloom_table_size; ++i) {
            this->mask_[i] |= meta->data[i];
        }
#endif
    }
    bool checkMask(QueryMetaTrieTree* meta){
#ifdef TRIE_MASK_OPT
#ifdef INVERSE_INDEX

#if Bloom_table_size == 8
        return __builtin_popcountll(*((uint64_t*)this->mask_) & *((uint64_t*)meta->data)) >= trie_min_ones_count_;
#else
        int ones_count = 0;
        uint64_t * my_mask = (uint64_t*)this->mask_;
        uint64_t * target_mask = (uint64_t*)meta->data;
        for (int i = 0; i < Bloom_table_size / 8; ++i) {
            ones_count += __builtin_popcountll(my_mask[i] & target_mask[i]);
        }
        if (ones_count < trie_min_ones_count_){
            return false;
        }
#endif
#else
        for (int i = 0; i < Bloom_table_size; ++i) {
            if((this->mask_[i] & meta->data[i]) != meta->data[i]) {

                return false;
            }
        }
#endif
        return true;
#else
        return true;
#endif
    }
    void resetMask(){
#ifdef TRIE_MASK_OPT
        memset(mask_,0, sizeof(mask_));
        for (auto ptr:ptr_list_) {
            InsertIntoMask(ptr);
        }
        for (auto it:maps_) {
            pthread_rwlock_rdlock(&it.second->lock_);
            for (int i = 0; i < Bloom_table_size; ++i) {
                this->mask_[i] |= it.second->mask_[i];
            }
            pthread_rwlock_unlock(&it.second->lock_);
        }
#endif
    }
    ~TrieNode() = default;
    bool Search(QueryMetaTrieTree* meta, Arraylist* removedQuery, int idx, int thread_id){
        bool res = false;
        if(!checkMask(meta)){
            return false;
        }
        auto ch = meta->data[idx];
        std::vector<TrieNode*> tmp_continue_node;
        if(ch == 0) {
#ifndef INVERSE_INDEX
            pthread_rwlock_rdlock(&this->lock_);
            for(auto da : this->maps_) {
                tmp_continue_node.push_back(da.second);
            }
            pthread_rwlock_unlock(&this->lock_);
#endif
        } else {
            pthread_rwlock_rdlock(&this->lock_);
            for(auto da : this->maps_) {
#ifdef INVERSE_INDEX
                if(((ch) & (da.first)) == ((da.first))) {
                    tmp_continue_node.push_back(da.second);
                }
#else
                if((ch & da.first) == ch) {
                    tmp_continue_node.push_back(da.second);
                }
#endif
            }
            pthread_rwlock_unlock(&this->lock_);
        }
        // 最后一层叶子结点 | 即最后一次循环时 获取数据
        if(idx == Bloom_table_size - 1) {
            for(auto no : tmp_continue_node) {
#ifdef INVERSE_INDEX
                if(RecursiveGetData(no, removedQuery, meta)){
#ifdef INVERSE_END_EARLY
                    return true;
#else
                    res = true;
#endif
                }
#else
                RecursiveGetData(no, removedQuery, meta);
#endif
            }
        } else{
            for (auto node: tmp_continue_node) {
#ifdef INVERSE_INDEX
                if(node->Search(meta, removedQuery, idx + 1, thread_id)){
#ifdef INVERSE_END_EARLY
                    return true;
#else
                    res = true;
#endif
                }
#else
                node->Search(meta, removedQuery, idx + 1, thread_id);
#endif

            }
        }
#ifndef INVERSE_INDEX
    if(pthread_rwlock_trywrlock(&this->lock_) ==0) {
        resetMask();
        pthread_rwlock_unlock(&this->lock_);
    }
#endif
        return res;
    }
    bool RecursiveGetData(TrieNode* node, Arraylist* removeQuery, QueryMetaTrieTree* meta) {
        if(node == nullptr) {
            return false;
        }
#ifdef INVERSE_INDEX
        for (auto query : node->ptr_list_) {
            if (query->timestamp > meta->timestamp){
                return true;
            }
        }
        return false;
#endif
        // 从叶子节点中移除
        pthread_rwlock_wrlock(&node->lock_);
        std::vector<QueryMetaTrieTree*> ll = node->ptr_list_;
        for(auto it = node->ptr_list_.begin();it != node->ptr_list_.end();) {
            bool sym = true;
            for(int i=0;sym && i<Bloom_table_size;i++) {
                if( ((*it)->data[i] & meta->data[i]) != meta->data[i] ) {
                    sym = false;
                    break;
                }
            }
            if(sym) {
                ArraylistAdd(removeQuery, *it);
                // result.push_back(*it);
                it = node->ptr_list_.erase(it);
            } else {
                it++;
            }
        }
        node->resetMask();
        pthread_rwlock_unlock(&node->lock_);
        return false;
    }
};

/**
 * @brief 前缀树
 * 
 */
class TrieTree {
private:
    TrieNode* root_;    // 根节点

public:
    TrieTree();
    explicit TrieTree(TrieNode *root) : root_(root) {   }
    ~TrieTree();

    // 插入数据  固定位数的定长字符
    // int Insert2(std::vector<unsigned char> input, AA aa, int thread_id);
    int Insert2(QueryMetaTrieTree* meta, int thread_id);

    // 查询整个字符序列是否出现过
    bool Search(QueryMetaTrieTree* meta, Arraylist* removedQuery, int thread_id){
        return StartsWithReg(meta, removedQuery, thread_id);
    }

    bool RecursiveGetData(TrieNode* node, Arraylist* removedQuery, QueryMetaTrieTree* meta){return false;}

    bool StartsWithReg(QueryMetaTrieTree* meta, Arraylist* removedQuery, int thread_id);

    std::vector<QueryMetaTrieTree*> Delete(QueryMetaTrieTree* meta, int thread_id);

    void Print(){}

    // 打印树中的所有节点
    // void PrintAll();
    // // 打印以**为前缀的序列
    // void PrintPre(std::string str);
    // 输出以root为根的所有数据
    // void PrintDef(TrieNode* node);
    void Destroy(TrieNode* root);
    static const indexType type = Trie_tree;
};
TrieTree::TrieTree() {
    root_ = new TrieNode();
    // root_ = (TrieNode *)malloc(sizeof(TrieNode));
}
TrieTree::~TrieTree() { Destroy(root_); }

//#define TRIE_INSERT_OPT
/**
 * @brief 插入 并写入标志 指针
 * 
 * @param input 
 * @param aa 
 */
inline int TrieTree::Insert2(QueryMetaTrieTree* meta, int thread_id) {
#ifdef INVERSE_INDEX
#if Bloom_table_size == 8
    int ones_count = __builtin_popcountll(*((uint64_t*)meta->data));
    if (ones_count < trie_min_ones_count_) {
        trie_min_ones_count_ = ones_count;
    }
#endif
#endif
    TrieNode* node = root_;
    unsigned char ch;
    for(size_t i=0;i<Bloom_table_size;i++) {
        ch = meta->data[i];
        TrieNode* tmpNode = node;
#ifdef TRIE_INSERT_OPT
        pthread_rwlock_wrlock(&node->lock_);
        node->InsertIntoMask(meta);

        auto it = node->maps_.find(ch);
        if(it != node->maps_.end()) {
            node = it->second;
            pthread_rwlock_unlock(&tmpNode->lock_);
        } else {
            it = node->maps_.find(ch);
            if(it != node->maps_.end()) {
                node = it->second;
            }else{
                // 上层根节点 新创建时新增 -> to old_node 中 而不是新建的节点中
                TrieNode* ptr = new TrieNode();
                node->maps_.insert({ch, ptr});
                node = ptr;
            }
            pthread_rwlock_unlock(&tmpNode->lock_);
        }
#else
        pthread_rwlock_wrlock(&node->lock_);
        auto it = node->maps_.find(ch);
        if(it != node->maps_.end()) {
            node = it->second;
        } else {
            // 上层根节点 新创建时新增 -> to old_node 中 而不是新建的节点中
            TrieNode* ptr = new TrieNode();
            node->maps_.insert({ch, ptr});
            node = ptr;
        }
        pthread_rwlock_unlock(&tmpNode->lock_);
#endif
    }

    pthread_rwlock_wrlock(&node->lock_);
    // node->path_ = 99;
    node->end_ = '1';
    node->ptr_list_.push_back((meta));
    pthread_rwlock_unlock(&node->lock_);
    return 1;
}


// 该函数定义的前缀查找 -> 不是所谓正常的前缀查找，是实现全局查找的功能；
// 即先根据前缀查询，如果前缀符合，路由到该节点下的所有子节点；如果前缀不符合，继续到下一个节点；
inline bool TrieTree::StartsWithReg(QueryMetaTrieTree* meta, Arraylist* removedQuery, int thread_id) {

    return root_->Search(meta,removedQuery,0, thread_id);

    // // 递归获取数据
    // for(auto re_no : recur_node) {
    //     RecursiveGetData(re_no, removedQuery, meta);
    // }
}


/**/
inline std::vector<QueryMetaTrieTree*> TrieTree::Delete(QueryMetaTrieTree* meta, int thread_id) {
    std::unordered_set<TrieNode *> recur_node;
    std::unordered_set<TrieNode *> continue_node;
    std::unordered_set<TrieNode *> tmp_continue_node;
    std::vector<QueryMetaTrieTree*> result = {};
    for(int i=0;i<Bloom_table_size;i++) {
        if(meta->data[i] == 0) {
            for(auto con : continue_node) {
                pthread_rwlock_rdlock(&con->lock_);
                for(const auto& da : con->maps_) {
                    tmp_continue_node.insert(da.second);
                }
                pthread_rwlock_unlock(&con->lock_);
            }
        } else {
            for(auto con : continue_node) {
                pthread_rwlock_rdlock(&con->lock_);
                for(const auto& da : con->maps_) {           
                    if(
                        (da.first & meta->data[i]) == meta->data[i] 
                    ) {
                        tmp_continue_node.insert(da.second);
                    }
                }
                pthread_rwlock_unlock(&con->lock_);
            }
        }
       
        // 最后一层叶子结点 | 即最后一次循环时 获取数据
        if(i == Bloom_table_size - 1) {
            for(auto no : tmp_continue_node) {
                RecursiveGetData(no, nullptr, meta);
            }
        }

        // 清理上层节点的 continue_node
        continue_node.clear();
        continue_node = tmp_continue_node;
        tmp_continue_node.clear();
    }
    return result;
    // 子节点删除 需要往父结点更新 可能只有一个 删除后为空 ??
}



/*
inline void TrieTree::PrintDef(TrieNode* node) {
    if(node == nullptr) {
        return;
    }
    if( node->path_ > 0) {
        if(node->ptr_list_.empty()) {
            // std::cout << static_cast<int>(node->key_) << "|<->|" << node->path_ << '\n';
        } else {
            std::vector<QueryMetaTrieTree*> ll = node->ptr_list_;
            std::cout << static_cast<int>(node->key_)  << "|<->|" << node->path_  << ";ll.size:" << ll.size() << '\n';
        }
    }
    for(int i=0;i<EACH_NODE_CHILD_NODE_SIZE;i++) {
        if(node->nextns_[i] != nullptr) {
            PrintDef(node->nextns_[i]);
        }
    }
}
inline void TrieTree::PrintAll() {
    std::cout << "PrintAllData--->>>" << '\n';
    PrintDef(root_);
}
*/


/**
 * @brief 销毁树结构
 * 
 * @param root 
 */
inline void TrieTree::Destroy(TrieNode* root) {
    if(root == nullptr) {
        return;
    }
    delete root;
    // 树的根节点重新赋值 | 避免下次使用或调用 避免堆栈异常
    root = nullptr;
}
