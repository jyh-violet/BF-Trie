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
#include <fcntl.h>
#include <unistd.h>
//#include <i386/endian.h>
// #include "test_trie_tree.h"
#include "test_trie_tree_map.h"
#include "test_bf_trie.h"
#include "test_a_tree.h"
#include "test_list.h"
#include "Tool/Tools.h"
extern "C" {
#include "../Tool/ArrayList.h"
#include "../Tool/bloom.h"
#include "../Tool/Tools.h"
}

using namespace std::literals;  // enables literal suffixes, e.g. 24h, 1ms, 1s ...

//#define USE_ATREE
//#define USE_Trie
//#define USE_LIST
#ifdef USE_Trie
using IndexTree=TrieTree;
#elif defined(USE_ATREE)
using IndexTree=ATree;
#elif defined(USE_LIST)
using IndexTree=ListIndex;

#else
using IndexTree=BFTrie;
#endif

DataRegionType dataRegionType = Random;
int valueSpan = 30; // 2 ^valueSpan
extern uint64_t maxValue;
int Qid = 0;
int TOTAL = (int) 100;
double indexInsertRatio = 0;
double cache_hit = 0.9;
extern double readRatio;
extern double scanRatio;
uint64_t local_timestamp = 0;

double zipfPara = 0.75;

extern int rangeWidth;
extern __thread int threadID;

std::atomic<int> total_element{0};
int threadnum = 4;
typedef struct TaskRes{
    double usedTime;
    size_t size;
    uint64_t handled;
    uint64_t search_nodes;
}TaskRes;


typedef struct ThreadAttributesTrieTree{
    int threadId;
    IndexTree* trie;
    QueryMetaTrieTree* insertQueries ;
    QueryMetaTrieTree* queries ;
    QueryMetaTrieTree* removeQuery;
    double *mixPara;
    int start;
    int end;
    TaskRes  result;
}ThreadAttributesTrieTree;

//#define LATENCY
//#define INSERT_LATENCY
//#define SEARCH_LATENCY
typedef struct {
    unsigned long long  *values;
    unsigned long long  *tmp;
    unsigned long long  total;
    unsigned int        size;
    double              range_min;
    double              range_max;
    double              range_deduct;
    double              range_mult;
    pthread_mutex_t     mutex;
} sb_percentile_t;


sb_percentile_t percentile;
void percentile_update(double value)
{
    unsigned int n;

    if (value < percentile.range_min)
        value= percentile.range_min;
    else if (value > percentile.range_max)
        value= percentile.range_max;

    n = floor((log(value) - percentile.range_deduct) * percentile.range_mult
              + 0.5);

    pthread_mutex_lock(&percentile.mutex);
    percentile.total++;
    percentile.values[n]++;
    pthread_mutex_unlock(&percentile.mutex);
}

static int percentile_init(uint64_t size, double range_min, double range_max){
    percentile.values = (unsigned long long *)
            calloc(size, sizeof(unsigned long long));
    percentile.tmp = (unsigned long long *)
            calloc(size, sizeof(unsigned long long));
    if (percentile.values == NULL || percentile.tmp == NULL)
    {
        //log_text(LOG_FATAL, "Cannot allocate values array, size = %u", size);
        return 1;
    }

    percentile.range_deduct = log(range_min);
    percentile.range_mult = (size - 1) / (log(range_max) -
                                          percentile.range_deduct);
    percentile.range_min = range_min;
    percentile.range_max = range_max;
    percentile.size = size;
    percentile.total = 0;

    pthread_mutex_init(&percentile.mutex, NULL);

    return 0;
}


static double percentile_calculate(double percent)
{
    unsigned long long ncur, nmax;
    unsigned int       i;

    pthread_mutex_lock(&percentile.mutex);

    if (percentile.total == 0)
    {
        pthread_mutex_unlock(&percentile.mutex);
        return 0.0;
    }

    memcpy(percentile.tmp, percentile.values,
           percentile.size * sizeof(unsigned long long));
    nmax = floor(percentile.total * percent / 100 + 0.5);

    pthread_mutex_unlock(&percentile.mutex);

    ncur = percentile.tmp[0];
    for (i = 1; i < percentile.size; i++)
    {
        ncur += percentile.tmp[i];
        if (ncur >= nmax)
            break;
    }

    return exp((i) / percentile.range_mult + percentile.range_deduct);
}

void testInsert(ThreadAttributesTrieTree* attributes){
    threadID = attributes->threadId;
    struct timespec startTmp, endTmp;
    clock_gettime(CLOCK_REALTIME, &startTmp);
    for(int i = attributes->start; i < attributes->end; i ++){
#ifdef INVERSE_INDEX
        attributes->insertQueries[i].timestamp = local_timestamp;
#endif
        attributes->trie->Insert2(attributes->insertQueries + i, attributes->threadId);
        total_element ++;
    }
    clock_gettime(CLOCK_REALTIME, &endTmp);
    attributes->result.usedTime = (endTmp.tv_sec - startTmp.tv_sec) + (endTmp.tv_nsec - startTmp.tv_nsec) * 1e-9;
    attributes->result.size = attributes->end - attributes->start;
}

bool workDone = false;
std::atomic<int> doneThread{0};
 void testMix(ThreadAttributesTrieTree* attributes){
     threadID = attributes->threadId;
     Arraylist* removedQuery = ArraylistCreate(attributes->end - attributes->start);
     struct timespec startTmp, endTmp;
     clock_gettime(CLOCK_REALTIME, &startTmp);
     int i;
     int old_removed = 0;
#ifdef INVERSE_INDEX
     int invalid_count = 0;
#endif
     for ( i = attributes->start; (i <  attributes->end) && !workDone; ++i) {
         if(attributes->mixPara[i] < indexInsertRatio){
#ifdef INSERT_LATENCY
             struct timespec time1, time2;
             clock_gettime(CLOCK_REALTIME, &time1);
#endif
#ifdef INVERSE_INDEX
             attributes->queries[i].timestamp = local_timestamp;
#endif
            attributes->trie->Insert2(attributes->queries + i, attributes->threadId);
#ifdef INSERT_LATENCY
             clock_gettime(CLOCK_REALTIME, &time2);
             uint64_t latency = time2.tv_sec * 1e9 + time2.tv_nsec - time1.tv_sec * 1e9 - time1.tv_nsec;
             percentile_update((double )latency);
#endif
            total_element ++;
         }else{
#ifdef SEARCH_LATENCY
             struct timespec time1, time2;
             clock_gettime(CLOCK_REALTIME, &time1);
#endif
#ifdef INVERSE_INDEX
             attributes->removeQuery[i].timestamp = local_timestamp;
             if(attributes->trie->Search(attributes->removeQuery+i, removedQuery, attributes->threadId)){
                 invalid_count ++;
             }
#else
            attributes->trie->Search(attributes->removeQuery+i, removedQuery, attributes->threadId);
#endif
#ifdef SEARCH_LATENCY
             clock_gettime(CLOCK_REALTIME, &time2);
             uint64_t latency = time2.tv_sec * 1e9 + time2.tv_nsec - time1.tv_sec * 1e9 - time1.tv_nsec;
             percentile_update((double )latency);
#endif

         }
     }
     clock_gettime(CLOCK_REALTIME, &endTmp);
     attributes->result.usedTime = (endTmp.tv_sec - startTmp.tv_sec) + (endTmp.tv_nsec - startTmp.tv_nsec) * 1e-9;
     attributes->result.handled = i - attributes->start;
#ifdef INVERSE_INDEX
     attributes->result.size = invalid_count;
#else
     attributes->result.size += removedQuery->size;
#endif
     ArraylistDeallocate(removedQuery);
     doneThread.fetch_add(1);
 }


int workTime = 30;
int test(IndexTree* trie) {
    double generateT = 0, putT = 0, mixT = 0;
    maxValue = 1L << valueSpan;
    printLog = 1;
    srand((unsigned)time(NULL));
    initZipfParameter(TOTAL, zipfPara);
    double *mixPara = (double *) malloc(sizeof (double ) * TOTAL);
    QueryMetaTrieTree* insertQueries = (QueryMetaTrieTree*)malloc(sizeof(QueryMetaTrieTree) * TOTAL );
    QueryMetaTrieTree* queries = (QueryMetaTrieTree*)malloc(sizeof(QueryMetaTrieTree) * TOTAL );
    QueryMetaTrieTree* removeQuery = (QueryMetaTrieTree*)malloc(sizeof(QueryMetaTrieTree) * TOTAL);
    memset(insertQueries,0,sizeof(QueryMetaTrieTree) * TOTAL);
    memset(queries,0,sizeof(QueryMetaTrieTree) * TOTAL);
    memset(removeQuery,0,sizeof(QueryMetaTrieTree) * TOTAL);
    struct timespec startTmp, endTmp;
    clock_gettime(CLOCK_REALTIME, &startTmp);
    initBloom();
    std::atomic<int> next_thread_id;
    next_thread_id.store(0);
    auto init_func=[&](){
        int thread_id = next_thread_id.fetch_add(1);
        uint64_t start_key = TOTAL / threadnum * (uint64_t)thread_id;
        uint64_t end_key = start_key + TOTAL / threadnum;
        if (thread_id == threadnum - 1){
            end_key = TOTAL;
        }
        for(uint64_t i = start_key; i < end_key ;i ++){
            int randNum = rand();
            mixPara[i] = ((double )randNum) / ((double )RAND_MAX + 1);
#ifdef INVERSE_INDEX
            insertQueries[i].queryId = BloomGenerate((char *) &insertQueries[i].data, (DataRegionType)(dataRegionType+1));
            queries[i].queryId = BloomGenerate((char *) &queries[i].data, (DataRegionType)(dataRegionType+1));
            removeQuery[i].queryId = BloomGenerate((char *) &removeQuery[i].data, (DataRegionType)(dataRegionType));
#else
            BloomGenerate((char *) &insertQueries[i].data, dataRegionType);
            insertQueries[i].queryId = i;
            BloomGenerate((char *) &queries[i].data, dataRegionType);
            queries[i].queryId = i + TOTAL;
            BloomGenerate((char *) &removeQuery[i].data, (DataRegionType)(dataRegionType+1));
#endif
        }
    };
    std::vector<std::thread> thread_group;
    for (int i = 0; i < threadnum; i++)
        thread_group.push_back(std::thread{init_func});

    for (int i = 0; i < threadnum; i++)
        thread_group[i].join();

    clock_gettime(CLOCK_REALTIME, &endTmp);
    generateT = (endTmp.tv_sec - startTmp.tv_sec) + (endTmp.tv_nsec - startTmp.tv_nsec) * 1e-9;

    int perThread = TOTAL / threadnum;
    pthread_t thread[MaxThread];
    ThreadAttributesTrieTree attributes[MaxThread];
    memset(attributes, 0, sizeof (ThreadAttributesTrieTree) * MaxThread);

    for(int i = 0; i < threadnum; ++i) {
        attributes[i].threadId = i;
        attributes[i].trie = trie;
        attributes[i].start = i * perThread;
        attributes[i].end = i == (threadnum - 1)? TOTAL: (i + 1)* perThread;
        attributes[i].insertQueries = insertQueries;
        attributes[i].queries = queries;
        attributes[i].removeQuery = removeQuery;
        attributes[i].mixPara = mixPara;
        pthread_create(&thread[i], 0, (void *(*)(void *)) testInsert, (void *)&attributes[i]);
    }

    for(int i = 0; i < threadnum; ++i) {
        pthread_join(thread[i], NULL);
        putT += attributes[i].result.usedTime;
    }
    putT = putT / threadnum;

    size_t removed = 0;

    workDone = false;
    perThread = TOTAL / threadnum;
    memset(attributes, 0, sizeof (ThreadAttributesTrieTree) * MaxThread);
    for(int i = 0; i < threadnum; ++i) {
        attributes[i].threadId = i;
        attributes[i].trie = trie;
        attributes[i].start = i * perThread;
        attributes[i].end = i == (threadnum - 1)? TOTAL: (i + 1)* perThread;
        attributes[i].insertQueries = insertQueries;
        attributes[i].queries = queries;
        attributes[i].removeQuery = removeQuery;
        attributes[i].mixPara = mixPara;
        pthread_create(&thread[i], 0, (void *(*)(void *)) testMix, (void *)&attributes[i]);
    }

    for (int i = 0; i < workTime * 10; ++i) {
        usleep(100000);
        if(doneThread == threadnum){
            break;
        }
    }
    workDone = true;

    uint64_t total_handled = 0;
    for (int i = 0; i < threadnum; ++i) {
        pthread_join(thread[i], NULL);
        removed += attributes[i].result.size;
        mixT += attributes[i].result.usedTime;
        total_handled += attributes[i].result.handled;
    }
    mixT = mixT / threadnum;

    int inverse_flag = 0;
    double indexThroughput =  total_handled / mixT / 1e3;
#ifdef INVERSE_INDEX
    inverse_flag = 1;
#endif
#ifndef PER_NODE_NUM
#define PER_NODE_NUM 1
#endif
#ifndef MAX_FOUT
#define MAX_FOUT 0
#endif

    printf("%d, %d, %d, %d, %d, %.2lf, %.2lf, %.2lf, %d, %ld, %.3lf, %.3lf, %.3lf, %.3lf, %d, %d, %d, %d, %d\n",
         trie->type, RANGE_FLAG, inverse_flag, PER_NODE_NUM, MAX_FOUT,
         readRatio, scanRatio, indexInsertRatio, TOTAL, total_handled, generateT, putT, mixT, indexThroughput,
         rangeWidth, threadnum, valueSpan, Bloom_table_size, Bloom_number_of_hashes);
    free(queries);
    free(removeQuery);
    free(insertQueries);
    return 0;
}


/*
void TestSelf(IndexTree* trie, BloomFilter* bf, int begin, int end, int thread_id) {
    // AA aa;
    for(int i=begin;i<=end;++i) {
        // aa.f_ = &bf[i];
        trie->Insert2_oldv(bf[i].GetBitTable(), &i, thread_id);
    }
}
*/


/*
void TestTrieTree(IndexTree* trie, int bf_num, int circle_cnt, int thread_num) {
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>>
        start_c = std::chrono::steady_clock::now();
    BloomParameters parameters;
    // 调节BF 大小和 hash数量
    parameters.projected_element_count = 100;
    parameters.false_positive_probability = 0.56;
    parameters.minimum_number_of_hashes = 4;
    parameters.maximum_number_of_hashes = 4;
    parameters.random_seed = 0xA5A5A5A5;
    if(!parameters) {
        std::cout << "Error - Invalid set of bloom filter parameters!" << std::endl;
        return;
    }
    parameters.ComputeOptimalParameters();

    // 测试值数量
    int count_test = 0;
    int test_data_value = 35;
    // 多个 BloomFilter 测试 Trie-tree
    BloomFilter* bf = new BloomFilter[bf_num]();
    BloomFilter* bf_ptr;
    bf_ptr = &(bf[0]);
    for(int i=0;i<bf_num;i++) {
        bf[i] = BloomFilter(parameters);
        for(int j=30;j<30+rand()*100;j++) {
            bf[i].Insert(j);
            if(j==test_data_value) {
                count_test++;
            }
        }
    }
    PrintTime("Init BF", start_c);

    std::cout << "test_bf GetSalt.size:" << bf[0].GetSalt().size() << '\n';
    std::cout << "test_bf GetBitTable.size:" << bf[0].GetBitTable().size() << '\n';
    std::cout << "test_bf HashCount:" << bf[0].HashCount() << '\n';
    std::cout << "test_bf count_test:" << count_test << '\n';


    // Test Trie-tree
    AA aa;
    aa.f_ = bf_ptr;

    // 1 - 单线程测试
    // start_c = std::chrono::steady_clock::now();
    // for(int i=0;i<bf_num;i++) {
    //     aa.f_ = bf+i;
    //     trie->Insert2(bf[i].GetBitTable(), aa);
    // }
    // PrintTime("Init Trie-tree", start_c);


    // 2 - 多线程测试 thread_num = 5
    int base = static_cast<int>(floor(bf_num / thread_num));
    int begin;
    int end;
    start_c = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for(int i=0;i<thread_num;i++) {
        begin = i*base;
        end = std::min((i+1)*base, bf_num);
        threads.push_back(std::thread(TestSelf, trie, bf, begin, end, i));
    }
    for (int i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }
    PrintTime("Init Trie-tree", start_c);


    BloomFilter test_bf_t = BloomFilter(parameters);
    test_bf_t.Insert(test_data_value);
    AA aa_t;
    aa_t.f_ = &test_bf_t;
    int a = 10;
    trie->Insert2_oldv(test_bf_t.GetBitTable(), &a, 1);


    // 打印树结构 不使用
    // trie->PrintAll();


    BloomFilter test_bf = BloomFilter(parameters);
    test_bf.Insert(33);

    start_c = std::chrono::steady_clock::now();
    std::vector<int> res = {};
    for(int i=0;i<circle_cnt;i++) {
        trie->StartsWithReg_oldv(test_bf.GetBitTable(), &res);
    }
    std::cout << "Trie-tree-search-cnt:" << res.size() << '\n';
    PrintTime("Test Trie-tree", start_c);
}
*/

// static bool EnableCoreDumps() {
//     struct rlimit   limit;
//     limit.rlim_cur = RLIM_INFINITY;
//     limit.rlim_max = RLIM_INFINITY;
//     return setrlimit(RLIMIT_CORE, &limit) == 0;
// }



int main(int argc, char* argv[]) {
    valueSpan = 30;
    scanRatio = atof(argv[1]);
    readRatio = 1-scanRatio;
    indexInsertRatio = atof(argv[2]);

    threadnum = atoi(argv[3]);
    TOTAL = atof(argv[4]);
    rangeWidth = 10;
    if(argc > 5){
        rangeWidth = atoi(argv[5]);
    }
    IndexTree* trie = new IndexTree();
    bool timer_end = false;
    auto timer_func = [&](){
        while (!timer_end){
            usleep(1);
            local_timestamp ++;
        }
    };
    auto timer_thread = std::thread{timer_func};

#ifdef LATENCY
    percentile_init(1e6, 1.0, 1e9);
#endif
    test(trie);
    timer_end = true;
    timer_thread.join();
    return 0;
}