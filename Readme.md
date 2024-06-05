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
- -Dinverse=ON: testing the index for V-policy
- -Dinverse=OFF: testing the index for V-policy (by default)
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
