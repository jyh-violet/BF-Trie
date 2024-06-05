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
#include <set>

#include <map>
#include <atomic>
#include "Tool/bloom.h"

extern "C" {
#include "../Tool/ArrayList.h"
#include "../Tool/Tools.h"
}
#ifndef OPT_FAT
//#define PRINT
using PositionType=unsigned char;
#endif
std::atomic<uint32_t> nodeId(0);
/**
 * @brief 倒排树节点
 *
 */
class ANode {
public:
    std::vector<QueryMetaTrieTree*> ptr_list_{};    // 叶节点使用，存储外部数据指针|标识 用于删除|失效

    std::map<uint32_t, ANode*> parents{};
    std::vector<ANode*> children{};
    std::set<PositionType> positions{};
    unsigned char version;
    pthread_rwlock_t lock_;
    uint32_t id;
    ANode() {
        version = 0;
        id = nodeId.fetch_add(1);
//        if(id % 1000 == 0){
//            std::cout << id << std::endl;
//        }
        pthread_rwlock_init(&lock_, NULL);
    }
    void Adjust(){
        for (auto node:children) {
            pthread_rwlock_rdlock(&node->lock_);
            auto child_parents = node->parents;
            pthread_rwlock_unlock(&node->lock_);
            for (auto p: child_parents) {
                if(this->Satisfy(p.second->positions)){
                    pthread_rwlock_wrlock(&p.second->lock_);
                    p.second->children.push_back(this);
                    this->parents.insert({p.second->id, p.second});
                    pthread_rwlock_unlock(&p.second->lock_);
                }else if(p.second->Satisfy(this->positions)){
                    pthread_rwlock_wrlock(&p.second->lock_);
                    this->children.push_back(p.second);
                    p.second->parents.insert({this->id, this});
                    pthread_rwlock_unlock(&p.second->lock_);
                }
            }
        }
    }
    bool Search(std::vector<PositionType> &position, int idx, Arraylist* removedQuery){return false;}
    bool Satisfy(std::set<PositionType> &position){
        if(this->positions.size() > position.size()){
            return false;
        }
        for (auto pos: this->positions) {
            if(position.find(pos) == position.end()){
                return false;
            }
        }
        return true;
    }
    explicit ANode(unsigned char ch) {
    }
    ~ANode() = default;
};

/**
 * @brief A-Tree: A Dynamic Data Structure for Efficiently Indexing
 *                Arbitrary Boolean Expressions
 *
 */
class ATree {
private:
    ANode* root_;    // 根节点
    pthread_rwlock_t lock_[Bloom_table_size * bits_per_char];
public:
    std::vector<std::vector<ANode*>> links{Bloom_table_size * bits_per_char, std::vector<ANode*>() };
    ATree();
    explicit ATree(ANode *root) : root_(root) {
        for(auto &l : lock_){
            pthread_rwlock_init(&l, NULL);
        }

    }
    ~ATree();

    // 插入数据  固定位数的定长字符
    // int Insert2(std::vector<unsigned char> input, AA aa, int thread_id);
    int Insert2(QueryMetaTrieTree* meta, int thread_id);


    void RecursiveGetData(ANode* node, Arraylist* removedQuery, QueryMetaTrieTree* meta);

    bool Search(QueryMetaTrieTree* meta, Arraylist* removedQuery, int thread_id);

    std::vector<QueryMetaTrieTree*> Delete(QueryMetaTrieTree* meta, int thread_id);

    void getPosition(QueryMetaTrieTree* meta, std::vector<PositionType> &position);

    void Print(){
//        std::set<ANode*> continue_node;
//        std::set<ANode*> tmp_continue_node;
//        continue_node.insert(root_);
//        while (!continue_node.empty()){
//            for (auto node:continue_node) {
//                std::cout << node->id << "(";
//                for (auto p : node->positions) {
//                    std::cout << (int)p<< ",";
//                }
//                std::cout << "):";
//
//                for (auto p : node->parents) {
//                    tmp_continue_node.insert(p.second);
//                    std::cout << p.second->id << ",";
//                }
//                std::cout << " | ";
//            }
//            continue_node = tmp_continue_node;
//            tmp_continue_node.clear();
//            std::cout << std::endl;
//        }
#ifdef PRINT
        std::cout << nodeId << std::endl;
#endif
    }
    // 打印树中的所有节点
    // void PrintAll();
    // // 打印以**为前缀的序列
    // void PrintPre(std::string str);
    // 输出以root为根的所有数据
    // void PrintDef(ANode* node);
    void Destroy(ANode* root);

    static const indexType type = A_tree;
};
ATree::ATree() {
    root_ = new ANode();
    // root_ = (ANode *)malloc(sizeof(ANode));
}
ATree::~ATree() { Destroy(root_); }

inline void ATree::getPosition(QueryMetaTrieTree* meta, std::vector<PositionType> &position){
    for (int i = 0; i < Bloom_table_size; ++i) {
        unsigned char ch = meta->data[i];
        for (int j = bits_per_char - 1; j >= 0; --j) {
//            std::cout <<(int) (ch & bit_mask[j])<< std::endl;
#ifdef INVERSE_INDEX
            if ((ch & bit_mask[j])){
                position.push_back((bits_per_char-1-j) + i * bits_per_char);
            }
#else
            if (!(ch & bit_mask[j])){
                position.push_back((bits_per_char-1-j) + i * bits_per_char);
            }
#endif
        }
    }
//    if(position.size() < Bloom_number_of_hashes -3){
//        std::cout << meta->queryId << ": ";
//        for (auto &pos:position) {
//            std::cout << ((int)pos) << ", ";
//        }
//        std::cout << std::endl;
//    }
}
#define INVERTED_INSERT_OPT
/**
 * @brief 插入 并写入标志 指针
 *
 * @param input
 * @param aa
 */
inline int ATree::Insert2(QueryMetaTrieTree* meta, int thread_id) {
    std::vector<PositionType> positionv{};
    ATree::getPosition(meta, positionv);
    std::set<PositionType> position{positionv.begin(), positionv.end()};
    std::vector<ANode*> children{};
    int new_node = 0, old_node = 0;
    pthread_rwlock_wrlock(&root_->lock_);
    for (auto pos:position) {
        auto it = root_->parents.find(pos);
        if(it == root_->parents.end()){
            new_node ++;
            ANode* new_ptr = new ANode();
            new_ptr->positions.insert(pos);
            new_ptr->children.push_back(root_);
            root_->parents.insert({pos, new_ptr});
            children.push_back(new_ptr);
        } else{
            ++ old_node;
            children.push_back(it->second);
        }
    }
    int loop = 0;
    pthread_rwlock_unlock(&root_->lock_);
    while (children.size() > 1){
        loop ++;
        std::set<ANode*> candidates{};
        bool remove_children = false;
        for(auto it = children.begin(); it != children.end();){
            auto node = *it;
            bool inserted = false;
            int maxMatch =0;
            ANode* matchNode = NULL;
            pthread_rwlock_rdlock(&node->lock_);
            for (auto p : node->parents) {
                if((int)p.second->positions.size() > maxMatch && p.second->Satisfy(position)){
                    inserted = true;
                    maxMatch = p.second->positions.size();
                    matchNode = p.second;
                }
            }
            pthread_rwlock_unlock(&node->lock_);
            if (inserted){
                remove_children = true;
                candidates.insert(matchNode);
                it = children.erase(it);
            } else{
                ++ it;
            }
        }
        for ( auto node:candidates) {
            bool  included = false;
            for (auto it = children.begin(); it != children.end();) {
                if((*it)->Satisfy(node->positions)){
                    it = children.erase(it);
                    remove_children = true;
                } else if(node->Satisfy((*it)->positions)){
                    included = true;
                    ++it;
                } else{
                    ++it;
                }
            }
            if(!included){
                ++ old_node;
                children.push_back(node);
            }
        }
        if(!remove_children && children.size() > 1){
            new_node ++;
            ANode* new_ptr = new ANode();
            ANode* child0 = children.back();
            children.pop_back();
            ANode* child1 = children.back();
            children.pop_back();
            new_ptr->children.push_back(child0);
            new_ptr->children.push_back(child1);
            new_ptr->positions.insert(child0->positions.begin(), child0->positions.end());
            new_ptr->positions.insert(child1->positions.begin(), child1->positions.end());
            new_ptr->Adjust();
            pthread_rwlock_wrlock(&child0->lock_);
            child0->parents.insert({new_ptr->id, new_ptr});
            pthread_rwlock_unlock(&child0->lock_);
            pthread_rwlock_wrlock(&child1->lock_);
            child0->parents.insert({new_ptr->id, new_ptr});
            pthread_rwlock_unlock(&child1->lock_);
            children.push_back(new_ptr);
        }
    }

    if(!children.empty()){
        auto node = children[0];
        pthread_rwlock_wrlock(&node->lock_);
        node->ptr_list_.push_back((meta));
        pthread_rwlock_unlock(&node->lock_);
    } else{
        auto node = root_;
        pthread_rwlock_wrlock(&node->lock_);
        node->ptr_list_.push_back((meta));
        pthread_rwlock_unlock(&node->lock_);
    }
//    std::cout << new_node << "," << old_node << "," << loop << "," << position.size() << std::endl;
    return 1;
}


inline bool ATree::Search(QueryMetaTrieTree* meta, Arraylist* removedQuery, int thread_id) {
    bool res = false;
    std::vector<PositionType> positionv{};
    ATree::getPosition(meta, positionv);
    std::set<PositionType> position{positionv.begin(), positionv.end()};
    std::set<ANode*> continue_node{root_};
    std::set<ANode*> tmp_continue_node{};
    int node_count =0;
    while (!continue_node.empty()){
        for(auto node: continue_node){
            node_count ++;
            pthread_rwlock_rdlock(&node->lock_);
#ifdef INVERSE_INDEX
            if(!node->ptr_list_.empty()){
                for (auto query : node->ptr_list_) {
                    if (query->timestamp > meta->timestamp){
                        pthread_rwlock_unlock(&node->lock_);
                        return true;
                    }
                }
            }
#endif
            for(auto it: node->parents){
                if(it.second->Satisfy(position)){
                    tmp_continue_node.insert(it.second);
                }
            }
            pthread_rwlock_unlock(&node->lock_);
#ifndef INVERSE_INDEX
            pthread_rwlock_wrlock(&node->lock_);
            for(auto ptr:node->ptr_list_){
                ArraylistAdd(removedQuery, ptr);
            }
            node->ptr_list_.clear();
            pthread_rwlock_unlock(&node->lock_);
#endif
        }
        continue_node = tmp_continue_node;
        tmp_continue_node.clear();
    }
//    std::cout << node_count << std::endl;
    return res;
}


/**
 * @brief 销毁树结构
 *
 * @param root
 */
inline void ATree::Destroy(ANode* root) {
    if(root == nullptr) {
        return;
    }
    delete root;
    // 树的根节点重新赋值 | 避免下次使用或调用 避免堆栈异常
    root = nullptr;
}
