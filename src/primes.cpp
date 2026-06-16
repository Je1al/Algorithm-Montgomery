// SPDX-License-Identifier: MIT
#include "montx/primes.hpp"

#include <array>
#include <vector>

#include "montx/modexp.hpp"

namespace montx {

namespace {

// Small primes for cheap trial division before the expensive Miller-Rabin test.
constexpr std::array<uint32_t, 54> kSmallPrimes = {
    2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,
    47,  53,  59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107,
    109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181,
    191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251};

}  // namespace

BigInt random_bits(size_t bits, const RandomBytes& rng) {
    if (bits == 0) return BigInt();
    const size_t words = (bits + 31) / 32;
    std::vector<uint8_t> buf(words * 4);
    rng(buf.data(), buf.size());
    std::vector<uint32_t> limbs(words, 0);
    for (size_t i = 0; i < words; ++i) {
        limbs[i] = static_cast<uint32_t>(buf[4 * i]) |
                   (static_cast<uint32_t>(buf[4 * i + 1]) << 8) |
                   (static_cast<uint32_t>(buf[4 * i + 2]) << 16) |
                   (static_cast<uint32_t>(buf[4 * i + 3]) << 24);
    }
    const size_t top = bits % 32;
    if (top != 0) limbs.back() &= (1u << top) - 1u;  // trim to exact width
    limbs[(bits - 1) / 32] |= 1u << ((bits - 1) % 32);  // force top bit -> full width
    return BigInt(std::move(limbs));
}

BigInt random_below(const BigInt& n, const RandomBytes& rng) {
    const size_t bits = n.bit_length();
    while (true) {
        // Random value of the same width, then reject if it is >= n.
        BigInt v = random_bits(bits, rng);
        // random_bits forces the top bit, so also allow shorter values:
        std::vector<uint32_t> l = v.limbs_padded((bits + 31) / 32);
        const size_t top = bits % 32;
        if (top != 0) l.back() &= (1u << top) - 1u;  // clear forced top bit region
        v = BigInt(std::move(l));
        if (v < n) return v;
    }
}

bool is_probable_prime(const BigInt& n, int rounds, const RandomBytes& rng) {
    if (n < BigInt(uint64_t(2))) return false;
    for (uint32_t p : kSmallPrimes) {
        const BigInt bp(static_cast<uint64_t>(p));
        if (n == bp) return true;
        if (n.mod(bp).is_zero()) return false;
    }

    // Write n-1 = 2^r * d with d odd.
    const BigInt one(1u), two(uint64_t(2));
    BigInt d = n.sub(one);
    size_t r = 0;
    while (d.is_even()) {
        d = d.shr(1);
        ++r;
    }

    const BigInt n_minus_1 = n.sub(one);
    const BigInt witness_hi = n.sub(BigInt(uint64_t(2)));  // upper bound for witnesses
    for (int i = 0; i < rounds; ++i) {
        BigInt a = random_below(witness_hi, rng).add(two);  // a in [2, n-2]
        BigInt x = mod_exp_montgomery(a, d, n);
        if (x == one || x == n_minus_1) continue;
        bool composite = true;
        for (size_t j = 0; j + 1 < r; ++j) {
            x = x.mul(x).mod(n);
            if (x == n_minus_1) {
                composite = false;
                break;
            }
        }
        if (composite) return false;
    }
    return true;
}

BigInt generate_prime(size_t bits, const RandomBytes& rng, int mr_rounds) {
    while (true) {
        BigInt cand = random_bits(bits, rng);
        if (cand.is_even()) cand = cand.add(BigInt(1u));
        // Scan odd candidates upward; cheap trial division weeds out most.
        for (int step = 0; step < 4096; ++step) {
            bool small_factor = false;
            for (uint32_t p : kSmallPrimes) {
                const BigInt bp(static_cast<uint64_t>(p));
                if (cand != bp && cand.mod(bp).is_zero()) {
                    small_factor = true;
                    break;
                }
            }
            if (!small_factor && cand.bit_length() == bits &&
                is_probable_prime(cand, mr_rounds, rng)) {
                return cand;
            }
            cand = cand.add(BigInt(uint64_t(2)));
        }
        // Overflowed the scan window; draw a fresh candidate.
    }
}

}  // namespace montx
