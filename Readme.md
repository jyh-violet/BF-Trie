BF-Trie: an index for Bloom filters, to find Bloom filters including in or included by a given Bloom filter.


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
- IndexInsert(index, meta): insert a signature `meta` into the index. Specifically, each `meta` holds a Bloom filter which should be generated for a DB query. For the D-method, the `meta` is generated for a DQL query and the  `queryId` attribute of the `meta` should also be filled to identify the related cached entry during Search. For V-method, a `meta` is generated for each updated tuple. Specifically, to insert an identifier into a Bloom filter, you can use the API `BloomInsertKey`, which implements the prefix hashing.
-  IndexSearch(index, meta,list): lookup based on the give `meta`. For the D-method, the found matched cases are stored in list; for V-method, it returns `true` if any matched case is found, i.e., the checked cached entry should be invalidated.


