//
// Created by jyh_2 on 4/24/2024.
//
#include <stdint.h>
#include "bftree.h"
#include <cstddef>
#include <mutex>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <atomic>
#include "test_trie_tree_map.h"
#include "test_bf_trie.h"
#include "test_a_tree.h"
void *CreateIndex(){
    srand((unsigned)time(NULL));
    initBloom();
    return (void *) new BFTrie();
}
int IndexInsert(void * index, QueryMetaTrieTree* meta){
    ((BFTrie*)index)->Insert2(meta, threadID);
    return 1;
}
int   IndexSearch(void * index, QueryMetaTrieTree* meta, Arraylist* removedQuery){
    if (((BFTrie*)index)->Search(meta, removedQuery, threadID)){
        return 1;
    } else{
        return 0;
    }
}
int GetNodeNum(){
    return BFTrieNodeId.load();
}

int GetIndexSize(void * index){
    return ((BFTrie*)index)->GetTotalSize();
}