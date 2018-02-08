#pragma once

namespace tdc {namespace compact_sparse_hashtable {

/*
    uint64_t x;

uint64_t xorshift64star(uint64_t state[static 1]) {
    uint64_t x = state[0];
    x ^= x >> 12; // a
    x ^= x << 25; // b
    x ^= x >> 27; // c
    state[0] = x;
    return x * 0x2545F4914F6CDD1D;
}

unsigned long long xor64(){
static unsigned long long x=88172645463325252LL;
xˆ=(x<<13); xˆ=(x>>7); return (xˆ=(x<<17));
*/

inline uint64_t compact_hashfn(uint64_t x, uint64_t w)  {
    uint64_t j = (w / 2ull) + 1;
    DCHECK_LT((w / 2ull), j);
    DCHECK_NE(w, 0);

    // NB: Two shifts because a single shift with w == 64 is undefined
    // behavior
    uint64_t w_mask = (1ull << (w - 1ull) << 1ull) - 1ull;

    return (x xor ((x << j) & w_mask)) & w_mask;
}

inline uint64_t compact_reverse_hashfn(uint64_t x, uint64_t w)  {
    return compact_hashfn(x, w);
}

}}
