//
// Created by jyh_2 on 10/14/2023.
//

#ifndef CTEST_BLOOM_H
#define CTEST_BLOOM_H

#include <glob.h>
#include "Tools.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef NO_RANGE_BLOOM
#define RANGE_FLAG 0
#else
#define Range_Bloom
#define RANGE_FLAG 1
#endif

#ifndef Bloom_number_of_hashes
#define Bloom_number_of_hashes 16
#endif
#ifndef Bloom_table_size
#define Bloom_table_size 8
#endif
#define OPT_FAT
//#define NDEBUG
#define Bloom_size (Bloom_table_size*8)
extern unsigned long long int     Bloom_random_seed_;
#define bits_per_char 0x08    // 8 bits in 1 char(unsigned)
extern const unsigned char bit_mask[bits_per_char];
extern const uint64_t longlong_mask[64];
//#define COMMON_TREE
#define TEST_RANGE
//#define INVERSE_INDEX
#define INVERSE_END_EARLY
typedef enum indexType{
    Trie_tree,
    Inverted_tree,
    A_tree,
    BF_Trie,
    List_index
}indexType;

typedef struct QueryMetaTrieTree {
//    char    queryId[queryIdLen];
    uint64_t queryId;
    uint64_t timestamp;
    unsigned char data[Bloom_table_size];
    int sym;
}QueryMetaTrieTree;
#define DEL_FLAG 1
#define INVALID_FLAG 2
#define REMOVED_FLAG 4

typedef unsigned int bloom_type;

extern bloom_type salt_[40];
void calcPara(int projected_element_count, double false_positive_probability);
void generate_unique_salt();

void initBloom();

void compute_indices(bloom_type hash, size_t* bit_index, size_t* bit);

bloom_type hash_ap(const unsigned char* begin, size_t remaining_length, bloom_type hash);

void BloomInsertKey( char* src, size_t src_len, char* dest);
int BloomContainKey(char* data, size_t src_len, char* bloom);
uint64_t BloomGenerate(char* bloom, DataRegionType dataRegion);

typedef uint64_t my_rwlock_t;
extern __thread int threadID;

void my_rwlock_init(my_rwlock_t* lock, void * ptr);
void my_rwlock_rdlock(my_rwlock_t* lock);
void my_rwlock_wrlock(my_rwlock_t* lock);
void my_rwlock_unlock(my_rwlock_t* lock);
int my_rwlock_trywrlock(my_rwlock_t* lock);


#ifdef __cplusplus
}
#endif
#endif //CTEST_BLOOM_H
