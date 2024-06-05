//
// Created by jyh_2 on 10/14/2023.
//

//
// Created by jyh_2 on 10/14/2023.
//

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdint-gcc.h>
#include <string.h>
#include "bloom.h"

static const int minimum_number_of_hashes = 1;    // 8 bits in 1 char(unsigned)
static const int maximum_number_of_hashes = 8;    // 8 bits in 1 char(unsigned)
static const int minimum_size = 8;    // 8 bits in 1 char(unsigned)
static const int maximum_size = 2048;    // 8 bits in 1 char(unsigned)
unsigned long long int     Bloom_random_seed_ = 0;

size_t Bloom_number_of_hashes=4;
size_t Bloom_table_size=64;

const unsigned char bit_mask[bits_per_char] = {
        0x01,  //00000001
        0x02,  //00000010
        0x04,  //00000100
        0x08,  //00001000
        0x10,  //00010000
        0x20,  //00100000
        0x40,  //01000000
        0x80   //10000000
};

bloom_type salt_[] =
        {
                0xAAAAAAAA, 0x55555555, 0x33333333, 0xCCCCCCCC,
                0x66666666, 0x99999999, 0xB5B5B5B5, 0x4B4B4B4B,
                0xAA55AA55, 0x55335533, 0x33CC33CC, 0xCC66CC66,
                0x66996699, 0x99B599B5, 0xB54BB54B, 0x4BAA4BAA,
                0xAA33AA33, 0x55CC55CC, 0x33663366, 0xCC99CC99,
                0x66B566B5, 0x994B994B, 0xB5AAB5AA, 0xAAAAAA33,
                0x555555CC, 0x33333366, 0xCCCCCC99, 0x666666B5,
                0x9999994B, 0xB5B5B5AA, 0xFFFFFFFF, 0xFFFF0000,
                0xB823D5EB, 0xC1191CDF, 0xF623AEB3, 0xDB58499F,
                0xC8D42E70, 0xB173F616, 0xA91A5967, 0xDA427D63
        };
void calcPara(int projected_element_count, double false_positive_probability){

    double min_m  = 1e7;
    double min_k  = 0.0;
    double k      = 1.0;

    while (k < 1000.0){
        const double numerator   = (- k * projected_element_count);
        const double denominator = log(1.0 - pow(false_positive_probability, 1.0 / k));

        const double curr_m = numerator / denominator;

        if (curr_m < min_m)
        {
            min_m = curr_m;
            min_k = k;
        }

        k += 1.0;
    }

    int number_of_hashes = (int)(min_k);

    int table_size = (int)(min_m);

    table_size += (((table_size % bits_per_char) != 0) ? (bits_per_char - (table_size % bits_per_char)) : 0);

    if (number_of_hashes < minimum_number_of_hashes)
        number_of_hashes = minimum_number_of_hashes;
    else if (number_of_hashes > maximum_number_of_hashes)
        number_of_hashes = maximum_number_of_hashes;

    if (table_size < minimum_size)
        table_size = minimum_size;
    else if (table_size > maximum_size)
        table_size = maximum_size;
    printf("(%f,%d):%d,%d\n", false_positive_probability, projected_element_count, table_size, number_of_hashes);
}
void generate_unique_salt()
{
    /*
      Note:
      A distinct hash function need not be implementation-wise
      distinct. In the current implementation "seeding" a common
      hash function with different values seems to be adequate.
    */


    for (size_t i = 0; i < Bloom_number_of_hashes; ++i)
    {
        /*
           Note:
           This is done to integrate the user defined random seed,
           so as to allow for the generation of unique bloom filter
           instances.
        */
        salt_[i] = salt_[i] * salt_[(i + 3) % Bloom_number_of_hashes] + (bloom_type)(Bloom_random_seed_);
    }
}

void initBloom(){
    srand((unsigned)time(NULL));
    Bloom_random_seed_ = rand();
    generate_unique_salt();
}


#define	FORCE_INLINE inline static

FORCE_INLINE uint64_t rotl64 ( uint64_t x, int8_t r )
{
    return (x << r) | (x >> (64 - r));
}

#define ROTL64(x,y)	rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

#define getblock(x, i) (x[i])

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

FORCE_INLINE uint64_t fmix64(uint64_t k)
{
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xff51afd7ed558ccd);
    k ^= k >> 33;
    k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
    k ^= k >> 33;

    return k;
}

//-----------------------------------------------------------------------------

void MurmurHash3_x64_128 ( const void * key, const int len,
                           const uint32_t seed, void * out )
{
    const uint8_t * data = (const uint8_t*)key;
    const int nblocks = len / 16;

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
    uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);

    int i;

    //----------
    // body

    const uint64_t * blocks = (const uint64_t *)(data);

    for(i = 0; i < nblocks; i++) {
        uint64_t k1 = getblock(blocks,i*2+0);
        uint64_t k2 = getblock(blocks,i*2+1);

        k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;

        h1 = ROTL64(h1,27); h1 += h2; h1 = h1*5+0x52dce729;

        k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

        h2 = ROTL64(h2,31); h2 += h1; h2 = h2*5+0x38495ab5;
    }

    //----------
    // tail

    const uint8_t * tail = (const uint8_t*)(data + nblocks*16);

    uint64_t k1 = 0;
    uint64_t k2 = 0;

    switch(len & 15) {
        case 15: k2 ^= ((uint64_t)tail[14]) << 48;
        case 14: k2 ^= ((uint64_t)tail[13]) << 40;
        case 13: k2 ^= ((uint64_t)tail[12]) << 32;
        case 12: k2 ^= ((uint64_t)tail[11]) << 24;
        case 11: k2 ^= ((uint64_t)tail[10]) << 16;
        case 10: k2 ^= ((uint64_t)tail[ 9]) << 8;
        case  9: k2 ^= ((uint64_t)tail[ 8]) << 0;
            k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

        case  8: k1 ^= ((uint64_t)tail[ 7]) << 56;
        case  7: k1 ^= ((uint64_t)tail[ 6]) << 48;
        case  6: k1 ^= ((uint64_t)tail[ 5]) << 40;
        case  5: k1 ^= ((uint64_t)tail[ 4]) << 32;
        case  4: k1 ^= ((uint64_t)tail[ 3]) << 24;
        case  3: k1 ^= ((uint64_t)tail[ 2]) << 16;
        case  2: k1 ^= ((uint64_t)tail[ 1]) << 8;
        case  1: k1 ^= ((uint64_t)tail[ 0]) << 0;
            k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;
    }

    //----------
    // finalization

    h1 ^= len; h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    h2 += h1;

    ((uint64_t*)out)[0] = h1;
    ((uint64_t*)out)[1] = h2;
}
#define SALT_CONSTANT 0x97c29b3a

void compute_indices(bloom_type hash, size_t* bit_index, size_t* bit)
{
    *bit_index = hash & ((Bloom_table_size  * 8) - 1);
    *bit       = (*bit_index) &0x7;
}

bloom_type hash_ap(const unsigned char* begin, size_t remaining_length, bloom_type hash)
{
    const unsigned char* itr = begin;
    unsigned int loop        = 0;

    while (remaining_length >= 8)
    {
        const unsigned int i1 = *((unsigned int*)(itr)); itr += sizeof(unsigned int);
        const unsigned int i2 = *((unsigned int*)(itr)); itr += sizeof(unsigned int);

        hash ^= (hash <<  7) ^  i1 * (hash >> 3) ^
                (~((hash << 11) + (i2 ^ (hash >> 5))));

        remaining_length -= 8;
    }

    if (remaining_length)
    {
        if (remaining_length >= 4)
        {
            const unsigned int i = *((unsigned int*)(itr));

            if (loop & 0x01)
                hash ^=    (hash <<  7) ^  i * (hash >> 3);
            else
                hash ^= (~((hash << 11) + (i ^ (hash >> 5))));

            ++loop;

            remaining_length -= 4;

            itr += sizeof(unsigned int);
        }

        if (remaining_length >= 2)
        {
            const unsigned short i = *((unsigned int*)(itr));

            if (loop & 0x01)
                hash ^=    (hash <<  7) ^  i * (hash >> 3);
            else
                hash ^= (~((hash << 11) + (i ^ (hash >> 5))));

            ++loop;

            remaining_length -= 2;

            itr += sizeof(unsigned short);
        }

        if (remaining_length)
        {
            hash += ((*itr) ^ (hash * 0xA5A5A5A5)) + loop;
        }
    }

    return hash;
}

void BloomInsertKey( char* src, size_t src_len, char* dest){
    int i;
    uint32_t checksum[4];

    MurmurHash3_x64_128(src, src_len, SALT_CONSTANT, checksum);
    for (i = 1; i <= Bloom_number_of_hashes; i++) {
        for (int j = 0; j < 4; ++j) {
            checksum[j] = hash_ap((const unsigned char *)&checksum[j], sizeof (checksum[j]), salt_[i]);
        }
        uint32_t  hash = checksum[0] +( i * checksum[1] + i*i * checksum[2] + i *i*i* checksum[3]);
        dest[(hash & ((Bloom_table_size  * 8) - 1)) / bits_per_char] |= bit_mask[hash&0x7];

    }
}


void RangeBloomInsertKey( char* src, size_t src_len, char* dest){
    size_t bit_index = 0;
    size_t bit       = 0;

    int i;
    uint32_t checksum[4];
    uint32_t orig_src = *(uint32_t *) src;
    for (i = 0; i < Bloom_number_of_hashes; i++) {
        uint32_t key = orig_src >> i;
        MurmurHash3_x64_128((char *)&key, sizeof (key), SALT_CONSTANT, checksum);
        for (int j = 0; j < 4; ++j) {
            checksum[j] = hash_ap((const unsigned char *)&checksum[j], sizeof (checksum[j]), salt_[i]);
        }
        uint32_t  hash = checksum[0] +( i * checksum[1] + i*i * checksum[2] + i *i*i* checksum[3]);
        compute_indices(hash, &bit_index, &bit);
        dest[bit_index / bits_per_char] |= bit_mask[bit];
    }
}
int RangeBloomContainKey(char* data, size_t src_len, char* bloom)
{
    size_t bit_index = 0;
    size_t bit       = 0;
    uint32_t orig_src = *(uint32_t *) data;

    for (size_t i = 0; i < Bloom_number_of_hashes; ++i)
    {
        uint32_t checksum[4];
        uint32_t key = orig_src >> i;
        MurmurHash3_x64_128((char *)&key, sizeof (key), SALT_CONSTANT, checksum);
        for (int j = 0; j < 4; ++j) {
            checksum[j] = hash_ap((const unsigned char *)&checksum[j], sizeof (checksum[j]), salt_[i]);
        }
        uint32_t  hash = checksum[0] +( i * checksum[1] + i*i * checksum[2] + i *i*i* checksum[3]);
        compute_indices(hash, &bit_index, &bit);
        if ((bloom[bit_index / bits_per_char] & bit_mask[bit]) != bit_mask[bit])
        {
            return 0;
        }
    }

    return 1;
}
int BloomContainKey(char* data, size_t src_len, char* bloom)
{
    size_t bit_index = 0;
    size_t bit       = 0;

    uint32_t checksum[4];

    MurmurHash3_x64_128(data, src_len, SALT_CONSTANT, checksum);
    for (size_t i = 1; i <= Bloom_number_of_hashes; ++i)
    {
        for (int j = 0; j < 4; ++j) {
            checksum[j] = hash_ap((const unsigned char *)&checksum[j], sizeof (checksum[j]), salt_[i]);
        }
        uint32_t  hash = checksum[0] +( i * checksum[1] + i*i * checksum[2] + i *i*i* checksum[3]);
        compute_indices(hash, &bit_index, &bit);
        if ((bloom[bit_index / bits_per_char] & bit_mask[bit]) != bit_mask[bit])
        {
            return 0;
        }
    }

    return 1;
}
int maxValue ;
int rangeWidth = 100;

