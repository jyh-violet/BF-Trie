cmake_minimum_required(VERSION 3.16)
project(BF-Trie)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-O0 -std=c++14 -g -fpic -march=native -Wall -fopenmp")


SET(CMAKE_BUILD_TYPE "Debug")
#SET(CMAKE_BUILD_TYPE "Release")

OPTION(use_trie "OPTION" OFF)
OPTION(use_atree "OPTION" OFF)
OPTION(use_list "OPTION" OFF)


OPTION(inverse "OPTION" OFF)
OPTION(inverse_zero "OPTION" OFF)

OPTION(range_bloom "OPTION" ON)

OPTION(bloom_table_size "OPTION" 16)

OPTION(bloom_hash_num "OPTION" 16)

OPTION(per_node_num "OPTION" 32)

OPTION(fout "OPTION" 16)

IF(use_trie)
    add_definitions(-DUSE_Trie)
ENDIF()


IF(use_atree)
    add_definitions(-DUSE_ATREE)
ENDIF()

IF(use_list)
    add_definitions(-DUSE_LIST)
ENDIF()

IF(inverse_zero)
    add_definitions(-DINVERSE_ZERO)
ENDIF()

IF(inverse)
    add_definitions(-DINVERSE_INDEX)
ENDIF()

IF(range_bloom)
    MESSAGE( STATUS "range_bloom = ${range_bloom}.")
ELSE()
    MESSAGE( STATUS "range_bloom = ${range_bloom}.")
    add_definitions(-DNO_RANGE_BLOOM)
ENDIF()

IF(bloom_table_size)
    MESSAGE( STATUS "bloom_table_size = ${bloom_table_size}.")
    add_definitions(-DBloom_table_size=${bloom_table_size})
ENDIF()

IF(bloom_hash_num)
    MESSAGE( STATUS "bloom_hash_num = ${bloom_hash_num}.")
    add_definitions(-DBloom_number_of_hashes=${bloom_hash_num})
ENDIF()

IF (per_node_num)
    MESSAGE( STATUS "per_node_num = ${per_node_num}.")
    add_definitions(-DPER_NODE_NUM=${per_node_num})
ENDIF ()

IF (fout)
    MESSAGE( STATUS "fout = ${fout}.")
    add_definitions(-DMAX_FOUT=${fout})
ENDIF ()

#3.head file path，头文件目录
INCLUDE_DIRECTORIES(
        src
)

add_library(bftool
        src/Tool/Tools.c src/Tool/Tools.h src/Tool/Tools.c src/Tool/ArrayList.h src/Tool/Arraylist.c src/Tool/bloom.c src/Tool/bloom.h
)

add_library(bftree
        src/trie_tree/bftree.cpp
        src/trie_tree/bftree.h
        src/trie_tree/test_a_tree.h
        src/trie_tree/test_bf_trie.h
        src/trie_tree/test_trie_tree_map.h
        src/Tool/Tools.c src/Tool/Tools.h src/Tool/Tools.c src/Tool/ArrayList.h src/Tool/Arraylist.c src/Tool/bloom.c src/Tool/bloom.h
)

ADD_EXECUTABLE(Trie
        src/trie_tree/test_a_tree.h
        src/trie_tree/test_trie_tree_map.h
        src/trie_tree/test_bf_trie.h
        src/trie_tree/test_list.h
        src/Tool/Tools.c src/Tool/Tools.h src/Tool/Tools.c src/Tool/ArrayList.h src/Tool/Arraylist.c src/Tool/bloom.c src/Tool/bloom.h
        src/trie_tree/test_trie_tree.cpp
)
target_link_libraries(Trie PUBLIC -lpthread)
