//
// Created by jyh_2 on 4/24/2024.
//

#ifndef BLINK_BFTREE_H
#define BLINK_BFTREE_H

#include "Tool/ArrayList.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "Tool/bloom.h"

    void *CreateIndex();
    int IndexInsert(void * index, QueryMetaTrieTree* meta);
    int   IndexSearch(void * index, QueryMetaTrieTree* meta, Arraylist* removedQuery);
    int GetNodeNum();
    int GetIndexSize(void * index);
#ifdef __cplusplus
}
#endif
#endif //BLINK_BFTREE_H
