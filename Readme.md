BF-Trie: an index for Bloom filters, to find Bloom filters including in or included by a given Bloom filter.

# Introduction

A Bloom filter can be represented as a set of the holding positions of the '1's in its bit array. A BF-Trie is a Trie tree on the position sets of '1's for Bloom filters.
In the tree, each node represents a position of '1', and each path represents a Bloom filter whose positions of '1' are recorded as the nodes on the path.

However, firstly, the BF-Trie is typically a sparse tree with many nodes having only one child. Secondly, it is usually a skew tree, where smaller positions are likely to have more child nodes.
Considering these characteristics, we applied two compression optimization strategies to the BF-Trie.

- **Height compression**. To compress the height of the BF-Trie, each node maintains a Bloom filter list with a maximum size _S_. 
A node will create its children nodes only if its list is full. When a Bloom filter is inserted, it travels along the tree and will be inserted into the first node whose list is not full. 
A new node is created only if all nodes in the path are full. 

- **Width compression**. Considering the skewness of the tree, instead of creating all possible children for a node, we define a maximum fan-out _F_. 
For any node representing position _p_, it will create _F_ children, with the _i_th child (where _i_ \in {1, 2, ..., _F_-1\}) indicating that the next '1' lies in position _p_ + _i_, while its last child (also referred to as the tail node) indicates that the next '1' lies in a position no less than _p_ + _F_. 


Height compression allows fewer nodes to be created during insertion and accessed during searching, while width compression allows smaller nodes, both beneficial to CPU caches.



# Compile

```shell
cmake ${opt}  .
make
```
opt:

- -Duse_atree=ON: testing A-Tree
- -Duse_trie=ON: testing Trie tree
- -Duse_list=ON: testing Arraylist
- -Dinverse=ON: testing the index for V-method
- -Dinverse=OFF: testing the index for V-method (by default)
- -Dbloom_table_size=8: the bloom filter bitmap size (int bytes) (as 8 by default)
- -Dbloom_table_size=16: the number of hash functions (as 16 by default)

# Run

```shell
 ./Trie ${sr} ${ir} ${t} ${n} ${range}
 
```
- ${sr}: ratio of S-type bloom filters
- ${ir}: ratio of insert operations
- ${t}: number of threads
- ${n}: number of preloaded bloom filter
- ${range}: number of elements inserted into S-type bloom filters (optimal, 10 by default)

# Others
- BloomFilterTest/: calculate the false positive rate of normal Bloom filters and prefix Bloom filters based on simulation.
- analysis projects/: the codes of projects that were done the code survey.

# Usage
To use the index for cache invalidation, you should include the libbftree.a in your code, and we provide a set of APIs:

- CreateIndex(): create an empty index;
- IndexInsert(index, meta): insert a signature `meta` into the index. Specifically, each `meta` holds a Bloom filter which should be generated for a DB query. For the D-method, the `meta` is generated for a DQL query and the  `queryId` attribute of the `meta` should also be filled to identify the related cached entry during Search. For V-method, a `meta` is generated for each updated tuple. Specifically, to insert an identifier into a Bloom filter, you can also include this lib and use the API `BloomInsertKey`, which implements the prefix hashing.
-  IndexSearch(index, meta,list): lookup based on the give `meta`. For the D-method, the found matched cases are stored in list; for V-method, it returns `true` if any matched case is found, i.e., the checked cached entry should be invalidated.


