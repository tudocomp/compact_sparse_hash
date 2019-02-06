#pragma once

#include <glog/logging.h>
#include <tudocomp/util/serialization.hpp>

// Source: https://github.com/kampersanda/poplar-trie/blob/master/include/poplar/bijective_hash.hpp
namespace poplar{namespace bijective_hash {

class size_p2_t {
public:
  size_p2_t() = default;

  explicit size_p2_t(uint32_t bits)
    : bits_{bits}, mask_{(1ULL << bits) - 1} {}

  uint32_t bits() const { return bits_; }
  uint64_t mask() const { return mask_; }
  uint64_t size() const { return mask_ + 1; }

private:
  uint32_t bits_{};
  uint64_t mask_{};
};

// (p, q): p < 2**w is a prime and q < 2**w is an integer such that pq mod m = 1
constexpr uint64_t PRIME_TABLE[][2][3] = {
  {{0ULL, 0ULL, 0ULL}, {0ULL, 0ULL, 0ULL}}, // 0
  {{1ULL, 1ULL, 1ULL}, {1ULL, 1ULL, 1ULL}}, // 1
  {{3ULL, 1ULL, 3ULL}, {3ULL, 1ULL, 3ULL}}, // 2
  {{7ULL, 5ULL, 3ULL}, {7ULL, 5ULL, 3ULL}}, // 3
  {{13ULL, 11ULL, 7ULL}, {5ULL, 3ULL, 7ULL}}, // 4
  {{31ULL, 29ULL, 23ULL}, {31ULL, 21ULL, 7ULL}}, // 5
  {{61ULL, 59ULL, 53ULL}, {21ULL, 51ULL, 29ULL}}, // 6
  {{127ULL, 113ULL, 109ULL}, {127ULL, 17ULL, 101ULL}}, // 7
  {{251ULL, 241ULL, 239ULL}, {51ULL, 17ULL, 15ULL}}, // 8
  {{509ULL, 503ULL, 499ULL}, {341ULL, 455ULL, 315ULL}}, // 9
  {{1021ULL, 1019ULL, 1013ULL}, {341ULL, 819ULL, 93ULL}}, // 10
  {{2039ULL, 2029ULL, 2027ULL}, {455ULL, 1509ULL, 195ULL}}, // 11
  {{4093ULL, 4091ULL, 4079ULL}, {1365ULL, 819ULL, 3855ULL}}, // 12
  {{8191ULL, 8179ULL, 8171ULL}, {8191ULL, 4411ULL, 4291ULL}}, // 13
  {{16381ULL, 16369ULL, 16363ULL}, {5461ULL, 4369ULL, 12483ULL}}, // 14
  {{32749ULL, 32719ULL, 32717ULL}, {13797ULL, 10031ULL, 1285ULL}}, // 15
  {{65521ULL, 65519ULL, 65497ULL}, {4369ULL, 3855ULL, 36969ULL}}, // 16
  {{131071ULL, 131063ULL, 131059ULL}, {131071ULL, 29127ULL, 110907ULL}}, // 17
  {{262139ULL, 262133ULL, 262127ULL}, {209715ULL, 95325ULL, 200463ULL}}, // 18
  {{524287ULL, 524269ULL, 524261ULL}, {524287ULL, 275941ULL, 271853ULL}}, // 19
  {{1048573ULL, 1048571ULL, 1048559ULL}, {349525ULL, 209715ULL, 986895ULL}}, // 20
  {{2097143ULL, 2097133ULL, 2097131ULL}, {1864135ULL, 1324517ULL, 798915ULL}}, // 21
  {{4194301ULL, 4194287ULL, 4194277ULL}, {1398101ULL, 986895ULL, 3417581ULL}}, // 22
  {{8388593ULL, 8388587ULL, 8388581ULL}, {1118481ULL, 798915ULL, 3417581ULL}}, // 23
  {{16777213ULL, 16777199ULL, 16777183ULL}, {5592405ULL, 986895ULL, 15760415ULL}}, // 24
  {{33554393ULL, 33554383ULL, 33554371ULL}, {17207401ULL, 31500079ULL, 15952107ULL}}, // 25
  {{67108859ULL, 67108837ULL, 67108819ULL}, {53687091ULL, 62137837ULL, 50704475ULL}}, // 26
  {{134217689ULL, 134217649ULL, 134217617ULL}, {17207401ULL, 113830225ULL, 82223473ULL}}, // 27
  {{268435399ULL, 268435367ULL, 268435361ULL}, {131863031ULL, 96516119ULL, 186492001ULL}}, // 28
  {{536870909ULL, 536870879ULL, 536870869ULL}, {357913941ULL, 32537631ULL, 274678141ULL}}, // 29
  {{1073741789ULL, 1073741783ULL, 1073741741ULL}, {889671797ULL, 1047552999ULL, 349289509ULL}}, // 30
  {{2147483647ULL, 2147483629ULL, 2147483587ULL}, {2147483647ULL, 1469330917ULL, 1056139499ULL}}, // 31
  {{4294967291ULL, 4294967279ULL, 4294967231ULL}, {858993459ULL, 252645135ULL, 1057222719ULL}}, // 32
  {{8589934583ULL, 8589934567ULL, 8589934543ULL}, {7635497415ULL, 1030792151ULL, 3856705327ULL}}, // 33
  {{17179869143ULL, 17179869107ULL, 17179869071ULL}, {9637487591ULL, 11825104763ULL, 12618841967ULL}}, // 34
  {{34359738337ULL, 34359738319ULL, 34359738307ULL}, {1108378657ULL, 21036574511ULL, 22530975979ULL}}, // 35
  {{68719476731ULL, 68719476719ULL, 68719476713ULL}, {13743895347ULL, 64677154575ULL, 8963410009ULL}}, // 36
  {{137438953447ULL, 137438953441ULL, 137438953427ULL}, {43980465111ULL, 35468117025ULL, 70246576219ULL}}, // 37
  {{274877906899ULL, 274877906857ULL, 274877906837ULL}, {207685529691ULL, 41073710233ULL, 208085144509ULL}}, // 38
  {{549755813881ULL, 549755813869ULL, 549755813821ULL}, {78536544841ULL, 347214198245ULL, 369238979477ULL}}, // 39
  {{1099511627689ULL, 1099511627609ULL, 1099511627581ULL}, {315951617177ULL, 928330176745ULL, 343949791253ULL}}, // 40
  {{2199023255531ULL, 2199023255521ULL, 2199023255497ULL}, {209430786243ULL, 1134979744801ULL, 1119502748281ULL}}, // 41
  {{4398046511093ULL, 4398046511087ULL, 4398046511071ULL}, {1199467230301ULL, 3363212037903ULL, 3331853417503ULL}}, // 42
  {{8796093022151ULL, 8796093022141ULL, 8796093022091ULL}, {8178823336439ULL, 918994793365ULL, 2405769031715ULL}}, // 43
  {{17592186044399ULL, 17592186044299ULL, 17592186044297ULL}, {16557351571215ULL, 2405769031715ULL, 2365335938745ULL}}, // 44
  {{35184372088777ULL, 35184372088763ULL, 35184372088751ULL}, {27507781814905ULL, 17847145262451ULL, 11293749065551ULL}}, // 45
  {{70368744177643ULL, 70368744177607ULL, 70368744177601ULL}, {13403570319555ULL, 34567102403063ULL, 4467856773185ULL}}, // 46
  {{140737488355213ULL, 140737488355201ULL, 140737488355181ULL}, {88113905752901ULL, 4432676798593ULL, 22020151239269ULL}}, // 47
  {{281474976710597ULL, 281474976710591ULL, 281474976710567ULL}, {100186008659725ULL, 4330384257087ULL, 123342967322647ULL}}, // 48
  {{562949953421231ULL, 562949953421201ULL, 562949953421189ULL}, {222399981598543ULL, 25358106009969ULL, 366146311168333ULL}}, // 49
  {{1125899906842597ULL, 1125899906842589ULL, 1125899906842573ULL}, {667199944795629ULL, 289517118902389ULL, 286994093901061ULL}}, // 50
  {{2251799813685119ULL, 2251799813685109ULL, 2251799813685083ULL}, {558586000294015ULL, 161999986596061ULL, 232003617167571ULL}}, // 51
  {{4503599627370449ULL, 4503599627370353ULL, 4503599627370323ULL}, {3449565672028465ULL, 3558788516733329ULL, 3514369651416283ULL}}, // 52
  {{9007199254740881ULL, 9007199254740847ULL, 9007199254740761ULL}, {2840107873116529ULL, 496948924399503ULL, 4991002184445225ULL}}, // 53
  {{18014398509481951ULL, 18014398509481931ULL, 18014398509481853ULL}, {16922616781634591ULL, 13595772459986403ULL, 6600695637062101ULL}}, // 54
  {{36028797018963913ULL, 36028797018963901ULL, 36028797018963869ULL}, {20962209174669945ULL, 20434243085382549ULL, 11645671763705525ULL}}, // 55
  {{72057594037927931ULL, 72057594037927909ULL, 72057594037927889ULL}, {14411518807585587ULL, 18681598454277613ULL, 21463964181510449ULL}}, // 56
  {{144115188075855859ULL, 144115188075855823ULL, 144115188075855811ULL}, {88686269585142075ULL, 44116894308935471ULL, 18900352534538475ULL}}, // 57
  {{288230376151711687ULL, 288230376151711681ULL, 288230376151711607ULL}, {126416831645487607ULL, 18300341342965825ULL, 136751638320155207ULL}}, // 58
  {{576460752303423263ULL, 576460752303423061ULL, 576460752303422971ULL}, {5124095576030431ULL, 2700050362076925ULL, 198471980483577139ULL}}, // 59
  {{1152921504606846883ULL, 1152921504606846803ULL, 1152921504606846697ULL}, {12397005425880075ULL, 566464323072728283ULL, 4132335141960025ULL}}, // 60
  {{2305843009213693951ULL, 2305843009213693669ULL, 2305843009213693613ULL}, {2305843009213693951ULL, 1768084568902373101ULL, 360500529464087845ULL}}, // 61
  {{4611686018427387733ULL, 4611686018427387421ULL, 4611686018427387271ULL}, {4557748170258646525ULL, 152768066863019061ULL, 1515372340968241207ULL}}, // 62
  {{9223372036854775291ULL, 9223372036854775279ULL, 9223372036854775181ULL}, {3657236494304118067ULL, 2545580940228350223ULL, 3339243145719352645ULL}} // 63
};

class Xorshift {
public:
  /// runtime initilization arguments, if any
  struct config_args {};

  /// get the config of this instance
  inline config_args current_config() const { return config_args{}; }

  Xorshift() = default;

  explicit Xorshift(uint32_t univ_bits, config_args config) {
    DCHECK(0 < univ_bits && univ_bits < 64);

    shift_ = univ_bits / 2 + 1;
    univ_size_ = size_p2_t{univ_bits};
  }

  uint64_t hash(uint64_t x) const {
    DCHECK(x < univ_size_.size());
    x = hash_<0>(x);
    x = hash_<1>(x);
    x = hash_<2>(x);
    return x;
  }

  uint64_t hash_inv(uint64_t x) const {
    DCHECK(x < univ_size_.size());
    x = hash_inv_<2>(x);
    x = hash_inv_<1>(x);
    x = hash_inv_<0>(x);
    return x;
  }

  uint64_t size() const {
    return univ_size_.size();
  }

  uint64_t bits() const {
    return univ_size_.bits();
  }

  void show_stat(std::ostream& os) const {
    os << "Statistics of Xorshift\n";
    os << " - size: " << size() << "\n";
    os << " - bits: " << bits() << "\n";
  }

private:
  uint32_t shift_{};
  size_p2_t univ_size_{};

  template<typename T>
  friend struct ::tdc::serialize;

  template<typename T>
  friend struct ::tdc::heap_size;

  template <uint32_t N>
  uint64_t hash_(uint64_t x) const {
    x = x ^ (x >> (shift_ + N));
    x = (x * PRIME_TABLE[univ_size_.bits()][0][N]) & univ_size_.mask();
    return x;
  }
  template <uint32_t N>
  uint64_t hash_inv_(uint64_t x) const {
    x = (x * PRIME_TABLE[univ_size_.bits()][1][N]) & univ_size_.mask();
    x = x ^ (x >> (shift_ + N));
    return x;
  }
};

}} //ns - poplar::bijective_hash


namespace tdc {namespace compact_hash {

class xorshift_t {
    uint64_t m_j;
    uint64_t m_w_mask;

    template<typename T>
    friend struct ::tdc::serialize;

    template<typename T>
    friend struct ::tdc::heap_size;

    xorshift_t() = default;
public:
    /// runtime initilization arguments, if any
    struct config_args {};

    /// get the config of this instance
    inline config_args current_config() const { return config_args{}; }

    /// Constructs a hash function for values with a width of `w` bits.
    xorshift_t(uint32_t w, config_args config):
        m_j((w / 2ull) + 1)
    {
        DCHECK_LT((w / 2ull), m_j);
        DCHECK_NE(w, 0);

        // NB: Two shifts because a single shift with w == 64 is undefined
        // behavior for a uint64_t according to the C++ standard.
        m_w_mask = (1ull << (w - 1ull) << 1ull) - 1ull;
    }

    /// This takes a value `x` with a width of `w` bits,
    /// and calculates a hash value with a width of `w` bits.
    inline uint64_t hash(uint64_t x) const {
        uint64_t j = m_j;
        uint64_t w_mask = m_w_mask;

        return (x xor ((x << j) & w_mask)) & w_mask;
    }

    /// This takes a hash value `x` with a width of `w` bits,
    /// and reverses the hash function to the original value.
    inline uint64_t hash_inv(uint64_t x) const {
        return hash(x);
    }
};

using poplar_xorshift_t = poplar::bijective_hash::Xorshift;

}

template<>
struct heap_size<compact_hash::xorshift_t> {
    using T = compact_hash::xorshift_t;

    static object_size_t compute(T const& val) {
        using namespace compact_hash;

        auto bytes = object_size_t::empty();

        bytes += heap_size<uint64_t>::compute(val.m_j);
        bytes += heap_size<uint64_t>::compute(val.m_w_mask);

        return bytes;
    }
};

template<>
struct serialize<compact_hash::xorshift_t> {
    using T = compact_hash::xorshift_t;

    static object_size_t write(std::ostream& out, T const& val) {
        using namespace compact_hash;

        auto bytes = object_size_t::empty();

        bytes += serialize<uint64_t>::write(out, val.m_j);
        bytes += serialize<uint64_t>::write(out, val.m_w_mask);

        return bytes;
    }
    static T read(std::istream& in) {
        using namespace compact_hash;

        T ret;
        ret.m_j = serialize<uint64_t>::read(in);
        ret.m_w_mask = serialize<uint64_t>::read(in);
        return ret;
    }
    static bool equal_check(T const& lhs, T const& rhs) {
        return gen_equal_check(m_j)
        && gen_equal_check(m_w_mask);
    }
};

template<>
struct heap_size<poplar::bijective_hash::Xorshift> {
    using T = poplar::bijective_hash::Xorshift;

    static object_size_t compute(T const& val) {
        using namespace compact_hash;

        auto bytes = object_size_t::empty();

        bytes += heap_size<uint64_t>::compute(val.shift_);
        bytes += heap_size<uint64_t>::compute(val.univ_size_.bits());

        return bytes;
    }
};

template<>
struct serialize<poplar::bijective_hash::Xorshift> {
    using T = poplar::bijective_hash::Xorshift;

    static object_size_t write(std::ostream& out, T const& val) {
        using namespace compact_hash;

        auto bytes = object_size_t::empty();

        bytes += serialize<uint64_t>::write(out, val.shift_);
        bytes += serialize<uint64_t>::write(out, val.univ_size_.bits());

        return bytes;
    }
    static T read(std::istream& in) {
        using namespace compact_hash;

        T ret;
        ret.shift_ = serialize<uint64_t>::read(in);
        ret.univ_size_ = poplar::bijective_hash::size_p2_t(serialize<uint64_t>::read(in));
        return ret;
    }
    static bool equal_check(T const& lhs, T const& rhs) {
        return gen_equal_check(shift_)
        && gen_equal_check(univ_size_.bits());

    }
};

}
