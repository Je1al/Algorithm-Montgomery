// SPDX-License-Identifier: MIT
#include <cstdint>
#include <string>
#include <vector>

#include "framework.hpp"
#include "montx/bigint.hpp"
#include "montx/modexp.hpp"
#include "montx/montgomery.hpp"

using montx::BigInt;
using montx::Montgomery;

namespace {

struct Kat {
    const char* base;
    const char* exp;
    const char* mod;
    const char* want;  // base^exp mod modulus, computed independently in Python
};

// Computed independently with CPython pow(). Sizes: 128/256/512/1024 bits.
const Kat kKats[] = {
    {"23b8c1e9392456de3eb13b9046685257", "bd9c66b3ad3c2d6d1a3d1fa7bc8960a9",
     "bdd640fb06671ad11c80317fa3b1799d", "4d0909904089773dc7f30d863cc81b64"},
    {"1f50a6572b463bbb878ad8b29ac7fe1a02f361db6b1d574eaff285e35392c73a",
     "4737819096da1dac72ff5d2a386ecbe06b65a6a48b8148f6b38a088ca65ed389",
     "97fc695a07a0ca6e0822e8f36c031199972a846916419f828b9d2434e465e151",
     "376525e10e523133490c20ecbd281c4e63eac66c0cc02ae63e5ecb72e5991e10"},
    {"51f118b4a2f30dd25c804189943a54223e7e10eb9302329fc3fcfdda77854d08d7872e94e7d486892f2e0cad0aab"
     "385599a4df831718807782b8733148875b86",
     "a9488d990bbb259911ce5dd2b45ed1f03139d32c93cd59bf5c941cf0dc98d2c1e2acf72f9e574f7aa0ee89aed453"
     "dd324b0dbb418d5288f1142c3fe860e7a113",
     "9a2a73ed562b0f79c37459eef50bea63371ecd7b27cd813047229389571aa8766c307511b2b9437a28df6ec4ce4a"
     "2bbdc241330b01a9e71fde8a774bcf36d58b",
     "255c1fc8b9c36aed6e3c972bb4066268ae16faf82ddd8d0d8d754afd9bff73a66e324d9d465b4eac27877f87a487"
     "13c9b5efab840d037acc924da0ad32bed2c2"},
    {"366eb16f508ebad7b7c93acfe059a0ee9132b63ef16287e4e9c349e03602f8ac10f1bc81448aaa9e66b2bc5b50c1"
     "87fcce177b4e0837b8a3d261a7ab3aa2e4f90e51f30dc6a7ee39c4b032ccd7c524a55304317faf42e12f3838b326"
     "8e944239b02b61c4a3d70628ece66fa2fd5166e6451b4cf36123fdf77656af7229d4beef",
     "dc713d960c0fd195c17af08a1745d6d87e570ddf827050a82369b584ff5e9ff0ff50bde4382567b85cabcc97663f"
     "1c97956269f0e5d7b8756dadd6c795a76d79bf3c4c06434308bc89fa6a688fb5d27bbeb799193f22faf823bed01d"
     "43cf2fde24933b83757750a9a491f0b2ea1fca65e27a984d654821d07fcd9eb1a7cad415",
     "beabedcbbaa80dd488bd64072bcfbe01a28defe39bf0027312476f57a5e5a5abaefcfad8efc89849b3aa7efe4458"
     "a885ab9099a435a240ae5af305535ec42e0829a3b2e95d65a441d58842dea2bc372f7412b29347294739614ff3d7"
     "19db3ad0ddd1dfb23b982ef8daf61a26146d3f31fc377a4c4a15544dc5e7ce8a3a578a8f",
     "246bba0c0eedb2d8b8db6c6897e4b7826c9dd8a0b2f7304d4b91936935755a76f09272049abc802ca50f5a66e556"
     "12b0b57f04777998dbc7098288e5e7e83076294afc1473c8e534778e951703f14d1468ce62f3aa3db1bcc2955a4e"
     "5129d074edda25e7b556d6af9f28b171d314dd2c9b2eda0d85de23ec42c74c92be87e154"},
};

// xorshift128+ style PRNG -- deterministic, good enough to drive differential
// tests (NOT for cryptographic use).
struct Rng {
    uint64_t s0 = 0x9e3779b97f4a7c15ull, s1 = 0xbf58476d1ce4e5b9ull;
    uint64_t next() {
        uint64_t x = s0, y = s1;
        s0 = y;
        x ^= x << 23;
        s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
        return s1 + y;
    }
    BigInt bits(size_t n) {
        std::vector<uint32_t> limbs((n + 31) / 32, 0);
        for (auto& w : limbs) w = static_cast<uint32_t>(next());
        if (n % 32) limbs.back() &= (1u << (n % 32)) - 1;
        return BigInt(std::move(limbs));
    }
    BigInt odd(size_t n) {
        BigInt v = bits(n);
        std::vector<uint32_t> l = v.limbs_padded((n + 31) / 32);
        l[0] |= 1u;                      // odd
        l[(n - 1) / 32] |= 1u << ((n - 1) % 32);  // top bit set -> full width
        return BigInt(std::move(l));
    }
};

}  // namespace

int main() {
    montx::test::Suite s("montgomery");

    // 1) Known-answer tests against an independent reference (Python pow()).
    for (const auto& k : kKats) {
        const BigInt base(k.base), exp(k.exp), mod(k.mod), want(k.want);
        s.eq(montx::mod_exp_montgomery(base, exp, mod).to_hex(), want.to_hex(),
             "Montgomery pow KAT");
        s.eq(montx::mod_exp_ct(base, exp, mod).to_hex(), want.to_hex(),
             "constant-time ladder KAT");
        s.eq(montx::mod_exp(base, exp, mod).to_hex(), want.to_hex(),
             "schoolbook reference KAT");
    }

    // 2) Domain round trips: from_montgomery(to_montgomery(a)) == a mod N.
    {
        Montgomery ctx(BigInt("97fc695a07a0ca6e0822e8f36c031199972a846916419f828b9d2434e465e151"));
        Rng rng;
        bool ok = true;
        for (int i = 0; i < 200; ++i) {
            BigInt a = rng.bits(256);
            BigInt back = ctx.from_montgomery(ctx.to_montgomery(a));
            if (back != a.mod(ctx.modulus())) ok = false;
        }
        s.check(ok, "to/from Montgomery round trip");
    }

    // 3) Montgomery product: from_mont(MonPro(to_mont(a),to_mont(b))) == a*b mod N.
    {
        Montgomery ctx(BigInt("97fc695a07a0ca6e0822e8f36c031199972a846916419f828b9d2434e465e151"));
        Rng rng;
        bool ok = true;
        for (int i = 0; i < 200; ++i) {
            BigInt a = rng.bits(256), b = rng.bits(256);
            BigInt prod = ctx.mont_product(ctx.to_montgomery(a), ctx.to_montgomery(b));
            if (ctx.from_montgomery(prod) != a.mul(b).mod(ctx.modulus())) ok = false;
        }
        s.check(ok, "Montgomery product matches schoolbook a*b mod N");
    }

    // 4) Differential fuzz: the three exponentiation paths must agree on random
    //    inputs across a range of sizes.
    {
        Rng rng;
        int agree = 0, total = 0;
        for (size_t bits : {64u, 127u, 128u, 200u, 256u, 384u, 512u}) {
            for (int i = 0; i < 60; ++i) {
                BigInt mod = rng.odd(bits);
                BigInt base = rng.bits(bits);
                BigInt exp = rng.bits(bits);
                BigInt ref = montx::mod_exp(base, exp, mod);
                BigInt fast = montx::mod_exp_montgomery(base, exp, mod);
                BigInt ct = montx::mod_exp_ct(base, exp, mod, mod.bit_length());
                ++total;
                if (ref == fast && ref == ct) ++agree;
            }
        }
        s.eq(agree, total, "all three modexp paths agree on random inputs");
    }

    // 5) Edge cases: exp 0, exp 1, base 0, base reduced from > N.
    {
        BigInt mod("97fc695a07a0ca6e0822e8f36c031199972a846916419f828b9d2434e465e151");
        s.eq(montx::mod_exp_ct(BigInt("abc"), BigInt(uint64_t(0)), mod).to_dec(), std::string("1"),
             "x^0 == 1");
        s.eq(montx::mod_exp_ct(BigInt("abc"), BigInt(uint64_t(1)), mod).to_hex(), std::string("abc"),
             "x^1 == x");
        s.eq(montx::mod_exp_ct(BigInt(uint64_t(0)), BigInt(uint64_t(5)), mod).to_dec(),
             std::string("0"), "0^5 == 0");
        // base larger than modulus must be reduced first
        BigInt big = mod.add(BigInt("abc"));
        s.eq(montx::mod_exp_ct(big, BigInt(uint64_t(3)), mod).to_hex(),
             montx::mod_exp(BigInt("abc"), BigInt(uint64_t(3)), mod).to_hex(),
             "base reduced mod N");
    }

    return s.summary();
}
