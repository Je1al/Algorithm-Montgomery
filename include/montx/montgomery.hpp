// SPDX-License-Identifier: MIT
//
// Montgomery modular arithmetic for a fixed odd modulus N.
//
// Montgomery's trick (P. Montgomery, 1985) replaces the expensive `mod N`
// reductions in modular multiplication with cheap shifts, by working in the
// "Montgomery domain" where a is represented as a*R mod N with R = 2^(32*s).
// The core operation is MonPro(x,y) = x*y*R^-1 mod N, implemented here with the
// CIOS (Coarsely Integrated Operand Scanning) algorithm of Koc, Acar & Kaliski.
//
// Security note: the raw-limb `monpro` and `pow_ct` are written to be constant
// time -- fixed limb width, branchless conditional subtraction, and a Montgomery
// powering ladder that performs the same multiply+square every iteration
// regardless of the secret exponent bits. See docs/constant-time.md.

#pragma once

#include <cstdint>
#include <vector>

#include "montx/bigint.hpp"

namespace montx {

class Montgomery {
public:
    // Throws if modulus is even or < 3 (Montgomery requires an odd modulus).
    explicit Montgomery(const BigInt& modulus);

    const BigInt& modulus() const { return n_; }
    size_t num_words() const { return s_; }
    uint32_t n_prime() const { return n_prime_; }

    // Domain conversions (high-level BigInt API).
    BigInt to_montgomery(const BigInt& a) const;    // a*R mod N
    BigInt from_montgomery(const BigInt& a) const;  // a*R^-1 mod N
    BigInt mont_product(const BigInt& a, const BigInt& b) const;  // a*b*R^-1 mod N
    BigInt one_mont() const;                          // R mod N (= "1" in the domain)

    // base^exp mod N. `pow` is fast but NOT constant time (square-and-multiply).
    // `pow_ct` is the constant-time Montgomery ladder; for a secret exponent pass
    // fixed_bits = modulus().bit_length() so the iteration count cannot leak |exp|.
    BigInt pow(const BigInt& base, const BigInt& exp) const;
    BigInt pow_ct(const BigInt& base, const BigInt& exp, size_t fixed_bits = 0) const;

    // --- raw fixed-width limb API (constant-time hot path) ------------------
    // Arrays are exactly num_words() little-endian limbs holding a value in [0,N).
    // out = a*b*R^-1 mod N. `out` may alias `a` and/or `b`.
    void monpro(uint32_t* out, const uint32_t* a, const uint32_t* b) const;

    // Side-channel lab hook. When set, *counter is incremented by 1 each time the
    // final conditional subtraction in REDC would change the result -- the classic
    // "extra reduction" leak (Schindler 2000 / Walter 2001). The constant-time
    // code always performs the subtraction as a branchless select; this only
    // *reports* the data-dependent bit an attacker could observe in a naive build.
    void set_extra_reduction_counter(unsigned long long* counter) const {
        extra_redc_ = counter;
    }

    // Side-channel lab hook: counts every MonPro (multiply/square) executed.
    // Used to prove, in tests, that the ladder's operation count is independent
    // of the secret exponent while square-and-multiply's is not.
    void set_op_counter(unsigned long long* counter) const { op_counter_ = counter; }

private:
    BigInt n_;
    size_t s_ = 0;
    uint32_t n_prime_ = 0;             // -N^-1 mod 2^32
    std::vector<uint32_t> n_limbs_;    // modulus, exactly s_ limbs
    std::vector<uint32_t> r2_;         // R^2 mod N
    std::vector<uint32_t> one_mont_;   // R mod N
    mutable unsigned long long* extra_redc_ = nullptr;
    mutable unsigned long long* op_counter_ = nullptr;

    std::vector<uint32_t> to_mont_limbs(const BigInt& a) const;
};

}  // namespace montx
