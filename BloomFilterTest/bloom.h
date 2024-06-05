//
// Created by jyh_2 on 10/14/2023.
//

#ifndef CTEST_BLOOM_H
#define CTEST_BLOOM_H

#include <glob.h>
#ifdef __cplusplus
extern "C"
{
#endif


extern size_t Bloom_number_of_hashes;
extern size_t Bloom_table_size;
extern unsigned long long int     Bloom_random_seed_;

#define bits_per_char 0x08    // 8 bits in 1 char(unsigned)
typedef unsigned int bloom_type;
extern const unsigned char bit_mask[bits_per_char];

extern bloom_type salt_[40];
void calcPara(int projected_element_count, double false_positive_probability);
void generate_unique_salt();

void initBloom();

void compute_indices(bloom_type hash, size_t* bit_index, size_t* bit);

bloom_type hash_ap(const unsigned char* begin, size_t remaining_length, bloom_type hash);

void BloomInsertKey( char* src, size_t src_len, char* dest);
void RangeBloomInsertKey( char* src, size_t src_len, char* dest);

int BloomContainKey(char* data, size_t src_len, char* bloom);
int RangeBloomContainKey(char* data, size_t src_len, char* bloom);

#ifdef __cplusplus
}
#endif
#endif //CTEST_BLOOM_H
