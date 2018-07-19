Compact Sparse Hash Table
========

The compact sparse hash table is a blend of compact hashing [1] and 
[Google's sparse hash table](https://github.com/sparsehash/sparsehash).
Our hash table is more memory efficient than both variants when the hash table is not much filled.
The restriction is that it can only hash integer keys, but of arbitrary bit width.

# Why?
The main idea is to use the compact sparse hash table as a dynamic dictionary for 
maintaining a set of (key,value)-pairs, or shortly kv-pairs, where the keys are integer values.
It is especially useful if memory efficiency is in focus, since the table stores the keys bit-aligned.
Therefore, it is crucial to specify the bit width of a key. The bit width can be updated online.
For instance, compact hash tables and sparse hash tables are already used for computing LZ78 [2].

# Usage

A minimal example is
```C++
#include <tudocomp/util/compact_sparse_hash.hpp>
...
// creates a hash table with zero entries, set the bit-width of the keys to four
auto map = tdc::compact_sparse_hashmap::compact_sparse_hashmap_t<int>(0, 4); 
for(int i = 0; i <= 15; ++i) { // interval [0..15] can be represented by four bits 
	map.insert(i, std::move(i*i)); // insert key i, value i*i 
	std::cout << i << " -> " << map[i] << std::endl; // map[i] returns value i*i with key i 
} 
```

# How it works
The idea of a hash table is to maintain a set of (key,value)-pairs, or shortly kv-pairs.

It applies the approach of Cleary [1], in which a _bijective_ hash function 
determines the _initial position_, i.e., the position at which to try to store a kv-pair at first place 
(in case of a collision a pair cannot stored there).
The bijective hash functions allows us to store only a fragment of the key, called the _quotient_, in the hash table.
The complete key of a kv-pair can be restored with the quotient and the additional knowledge of the initial address of the kv-pair.
Unfortunately, due to collisions, it happens that the kv-pair is misplaced (i.e., it is not 
stored at its initial address). 
The initial address can be restored by additionally maintaining two bit vectors, and restricting the 
collision resolving to linear probing.
The bit vectors track the misplacements such that we can recalculate the initial address of a stored kv-pair.
Each of the two additional bit vectors stores for each position in the hash table one bit.
In summary, this technique saves space by not saving the full keys, but only their quotients. 

To further slim down the space footprint, we apply the trick of the sparse hash table:
Instead of allocating a large hash table, we allocate a vector of pointers to buckets.
Each bucket represents a section of length `B` of the hash table, such that we have `n/B` buckets if the hash table is of size `n`
(we assure that `n` is divisible by `B` such that all buckets have the same length `B`).
Although a bucket stores up to `B` elements, it only acquires space for the actually saved kv-pairs in it.
For that, it stores a bit vector of length `B` marking with a one all positions in its section of the hash table that are actually occupied by a
kv-pair.
The kv-pair corresponding to the `i`-th one in the bit vector (i.e., the `i`-th one in the bit vector has rank `i`) 
is the `i`-th element stored in the bucket.
Given that we want to access the `j`-th element in the section belonging to a bucket,
we know that the `j`-the position is marked with a one in the bit vector, but not the rank of this one.
To compute the rank of the one at the `j`-th position, we count how many one's up to the `j`-th position are stored in the bit vector in the bucket.
Remember that the rank is the entry number of the element in the bucket we want to access.
By keeping `B` small enough, we argue that the entire bucket can be stored in cache, allowing us to work with the bit vector
with modern CPU instructions like `popcount`. 
When inserting a new kv-pair into the bucket, we update the bit vector, and move the stored elements adequately
(like in a standard std::vector). However, this is not a performance bottleneck, since again, with a sufficiently small bucket size,
this operation is computed efficiently on modern computer hardware.
Currently, we have set the bucket size `B` to 64.


# API
We have a `set` and a `map` interface to the (sparse) compact hash table:
 - `tdc::compact_sparse_hashset::generic_hashset_t`
 - `tdc::compact_sparse_hashmap::generic_hashmap_t`
Each of these hash table classes is templated by the following parameters:
 - the hash function
 - how the storage of the hash table is represented (e.g., sparse)
 - how to maintain entries that are stored not at their initial address, i.e., how the displacement works
   - `cv_bvs_t` : Approach by Cleary using two bit vectors setting a virgin and change bit
   - `displacement_t<T>`: using a displacement array represented by `T`, which can be
     - `compact_displacement_table_t`: the recursive m-Bonsai approach of [3]
     - `elias_gamma_displacement_table_t`: the gamma m-Bonsai approach of [3]

The `generic_hashset_t` has the following helpful methods:
 - `lookup(key)` looks up a key and returns an `entry_t`,
 - `lookup_insert(key)` additionally inserts `key` if not present,
 - `lookup_insert_key_width(key, key_width)` works like above, but additionally increases the bit widths of the keys to `key_width`,
 - `grow_key_width(key_width)` increases the bit width of the keys to `key_width`.
All `lookup*` methods return an `entry_t` object, which contains an _id_ (`uint64_t`)
which is unique and immutable until the hash table needs to be rehashed.
This _id_ is computed based on the displament setting:
 - For `displacement_t<T>` it is the position in the hash table the entry was hashed to. The id needs `log2(table_size)` bits.
 - For `cv_bvs_t` it is the local position within its group (the approach `cv_bvs_t` clusters all entries with the same initial address to one group)
   It is `id = initial_address | (local_position << log2(table_size))`. The id needs `log2(table_size) + log2(x)` bits, where `x` is the size of the specific group (which is at most the maximal number of collisions at an initial address) .

It is possible to let the hash table call an event handler before it rehashes its contents.
For that, methods that can cause a rehashing provide a template parameter `on_resize_t` that can be set to an event handler.
See the class `default_on_resize_t` in `generic_compact_hashset` for an example.

# Constraints

* keys have to be integers
* linear probing for collision handling
* hash table size is always a power of two
* hash function must be bijective
* API is not STL-conform

# Features
* The bit width of the keys can be updated on-line.
  Changing the bit width causes a rehashing of the complete hash table.
* Supports multiple hash functions. Currently, a `xorshift` hash function is implemented.
* On resizing the hash table, each bucket of the old hash table is rehashed and subsequently freed,
  such that there is no high memory peak like in traditional hash tables that need to keep entire old and new hash table
  in RAM during a resize operation.


# Dependencies

The project is written in modern `C++14`.
It uses `cmake` to build the library.

The external dependencies are:

* [Google Logging (glog)](https://github.com/google/glog) (0.34 or later).
* [Google Test](https://github.com/google/googletest) (1.7.0 or later) __[Just for running the unit tests]__.

`cmake` searches the external dependencies first on the system, 
or automatically downloads and builds them from their official repositories.
In that regard, a proper installation of the dependencies is not required.

# License

The code in this repository is published under the
[Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)

# Todo
* When additionally restricting the values to be integers, we can avoid padding: 
  We currently byte-align the values to allow the reinterpretation of its content (just by casting).
  By restricting to integer values, we can write the values bit-compact in a bit vector.
* Additionally, in the case that we work with values that are integers, 
  we want to support setting the width of the integer values online to further slim down memory consumption.
* The hash table currently does not support the deletion of a kv-pair.
* Support variable bucket sizes `B`

# Related Work
* [Dynpdt: dynamic path-decomposed trie](https://github.com/kampersanda/dynpdt), a space-efficient dynamic keyword dictionary. It supports strings as values.
* [mame-Bonsai](https://github.com/Poyias/mBonsai), a compact hash table implementation used as a trie data structure
* [Bonsai trie reimplementation](https://github.com/kampersanda/bonsais), a reimplementation of the previous trie data structure

# References
* [1] J. G. Cleary. Compact hash tables using bidirectional linear probing. IEEE Trans. Computers, 33(9): 828-834, 1984.
* [2] J. Fischer, D. Köppl: Practical Evaluation of Lempel-Ziv-78 and Lempel-Ziv-Welch Tries. SPIRE 2017: 191-207.
* [3] A. Poyias, R. Raman: Improved Practical Compact Dynamic Tries. SPIRE 2015: 324-336
