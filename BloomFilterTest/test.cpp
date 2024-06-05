#include <algorithm>
#include <valarray>
#include <bitset>
#include <map>
#include <fcntl.h>
#include <cstddef>
#include <memory>
#include <random>
#include <string_view>
#include <cassert>
#include <thread>
#include <mutex>
#include <iostream>
#include <cstring>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <sys/resource.h>
#include <atomic>
#include <fstream>
#include <malloc.h>
#include <unistd.h>

using namespace std;
#include "bloom_filter.hpp"
extern "C"{
#include "bloom.h"

}
uint64_t max_value;


void getTargetFalseRate(double targetRate){

    double hash_num[] = { 4 , 5,6,7, 8, 9, 10,11, 12, 13, 14, 15, 16};
    double counts[] = {2,3,4,5,6,7,8,9,10};

    int num1 = sizeof (counts) / sizeof (double);
    int num2 = sizeof (hash_num) / sizeof (double );
    for (int i = 0; i < num1; ++i) {
        double table_size = 8 * 8;
        while (true){
            int mini_hash = 0;
            double mini_p = 1.0;
            for (int k = 0; k < num2; ++k) {
                double hash_n = hash_num[k];
                double p = pow( 1- pow(1-1.0/table_size, hash_n * counts[i]) , hash_n);
                if (p < mini_p){
                    mini_hash = hash_n;
                    mini_p = p;
                }

            }
            if (mini_p <= targetRate){
                printf("0 %d %d %d %.8f %.8f\n",
                       (int)mini_hash, (int)counts[i], (int)table_size / 8, mini_p, targetRate);
                break;
            }
            table_size += 8*8;
        }
    }

    for (int i = 0; i < num1; ++i) {
        double table_size = 8 * 8;
        while (true){
            int mini_hash = 0;
            double mini_p = 1.0;
            for (int k = 0; k < num2; ++k) {
                double hash_n = hash_num[k];
                double p = pow( (2 * counts[i] * (1-1.0/(pow(2, hash_n))) + hash_n - 1) / (1.0*table_size) , hash_n) + 2*(hash_n - 1) / (1.0 * max_value);

                if (p < mini_p){
                    mini_hash = hash_n;
                    mini_p = p;
                }

            }
            if (mini_p <= targetRate){
                printf("1 %d %d %d %.8f %.8f\n",
                       (int)mini_hash, (int)counts[i], (int)table_size / 8, mini_p, targetRate);
                break;
            }
            table_size += 8*8;
        }
    }
}

double testBloom3(int insert_num, int hash_num, int table_size){
    Bloom_table_size = table_size;
    Bloom_number_of_hashes = hash_num;
    initBloom();

    unsigned char bloom[Bloom_table_size];
    memset(bloom, 0, sizeof (bloom));
    int insert_key[insert_num];
    int r = (int)((double)rand() / ((double)RAND_MAX) * (max_value));
    for (int i = 0; i < insert_num; ++i) {
        insert_key[i] = r +i;
        BloomInsertKey((char*)&insert_key[i], sizeof (r), reinterpret_cast<char *>(&bloom));
    }
    for (int i = 0; i < insert_num; ++i) {
        if(!BloomContainKey((char*)&insert_key[i], sizeof (insert_key[i]), reinterpret_cast<char *>(&bloom))){
            printf("not found: %d\n", insert_key[i]);
        }
    }

    size_t total = 100000000L, fp = 0, neg = 0;
    for (size_t i = 0; i < total; ++i) {
        int r = (int)((double)rand() / ((double)RAND_MAX) * (max_value));
        bool  found = false;
        for (int j = 0; j < insert_num; ++j) {
            if(insert_key[j] == r){
                found = true;
                break;
            }
        }
        if(!found){
            neg ++;
        }
        if(!found && BloomContainKey((char*)&r, sizeof (r), reinterpret_cast<char *>(&bloom))){
            fp ++;
        }
    }

    return fp * 1.0/ neg;
}

double testRangeBloom3(int insert_num, int hash_num, int table_size){
    Bloom_table_size = table_size;
    Bloom_number_of_hashes = hash_num;
    initBloom();

    unsigned char bloom[Bloom_table_size];
    memset(bloom, 0, sizeof (bloom));
    int insert_key[insert_num];
    int r = (int)((double)rand() / ((double)RAND_MAX) * (max_value));
    for (int i = 0; i < insert_num; ++i) {
        insert_key[i] = r +i;
        RangeBloomInsertKey((char*)&insert_key[i], sizeof (r), reinterpret_cast<char *>(&bloom));
    }
    for (int i = 0; i < insert_num; ++i) {
        if(!RangeBloomContainKey((char*)&insert_key[i], sizeof (insert_key[i]), reinterpret_cast<char *>(&bloom))){
            printf("not found: %d\n", insert_key[i]);
        }
    }
retry:
    size_t total = 300000000L, fp = 0, neg = 0;
    for (size_t i = 0; i < total; ++i) {
        int r = (int)((double)rand() / ((double)RAND_MAX) * (max_value));
        bool  found = false;
        for (int j = 0; j < insert_num; ++j) {
            if(insert_key[j] == r){
                found = true;
                break;
            }
        }
        if(!found){
            neg ++;
        }
        if(!found && RangeBloomContainKey((char*)&r, sizeof (r), reinterpret_cast<char *>(&bloom))){
            fp ++;
        }
    }
    if (neg == total){
        goto retry;
    }
    return  fp * 1.0/ neg;
}

void evaluateFR(){

    double hash_num[] = { 4 , 5,6,7, 8, 9, 10,11, 12, 13, 14, 15, 16};
    double counts[] = {1,2,3,4,5,6,7,8,9,10};
    double table_size[]={8};
    int num1 = sizeof (counts) / sizeof (double);
    int num2 = sizeof (hash_num) / sizeof (double );
    int num3 = sizeof (table_size) / sizeof (double);

//    printf("evaluate blooms:\n");

//
    for (int i = 0; i < num1; ++i) {
        for (int j = 0; j < num3; ++j) {
            int mini_hash = 0;
            double mini_p = 1.0;
            for (int k = 0; k < num2; ++k) {
                double total_p = 0;
                double count = 0;
                double max_p = 0, min_p = 1;
                for (int l = 0; l < 10; ++l) {
                    double p = testBloom3(counts[i], hash_num[k], table_size[j]);
                    if(p >= 1e-10){
                        total_p += p;
                        count ++;
                        if (p > max_p){
                            max_p = p;
                        }
                        if (p < min_p){
                            min_p = p;
                        }
                    }
                }
                double p = total_p / count;
                if (p < mini_p){
                    mini_hash = hash_num[k];
                    mini_p = p;
                }

            }
            printf("0 %d %d %d %.8f\n",
                   (int)mini_hash, (int)counts[i], (int)table_size[j], mini_p);
            fflush(stdout);
        }
    }
//    printf("evaluate range blooms:\n");
    for (int i = 0; i < num1; ++i) {
        for (int j = 0; j < num3; ++j) {
            int mini_hash = 0;
            double mini_p = 1.0;
            for (int k = num2-1; k < num2; ++k) {
                double total_p = 0;
                double count = 0;
                for (int l = 0; l < 10; ++l) {
                    double p = testRangeBloom3(counts[i], hash_num[k], table_size[j]);
                    if(p >= 1e-10){
                        total_p += p;
                        count ++;
                    }
                }
                double p = total_p / count;
                if (p < mini_p){
                    mini_hash = hash_num[k];
                    mini_p = p;
                }

            }
            printf("1 %d %d %d %.8f\n",
                   (int)mini_hash, (int)counts[i], (int)table_size[j], mini_p);
            fflush(stdout);

        }
    }
}


int main() {
    max_value = INT32_MAX;
    max_value = 10000000;
    getTargetFalseRate(1e-4);
    getTargetFalseRate(1e-5);
    evaluateFR();

    return 0;

}
