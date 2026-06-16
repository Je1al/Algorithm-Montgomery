// SPDX-License-Identifier: MIT
#include "montx/montgomery.hpp"

#include <algorithm>
#include <stdexcept>

#include "montx/ct.hpp"

namespace montx {

namespace {

// -N^-1 mod 2^32 via Newton-Raphson lifting. Each step doubles the number of
// correct low bits: starting from n0 (3 bits correct for odd n0) four steps
// reach 48 >= 32 bits.
uint32_t neg_inv_mod_2_32(uint32_t n0) {
    uint32_t inv = n0;  // n0 * n0 == 1 (mod 8) for odd n0
    for (int i = 0; i < 4; ++i) inv *= 2u - n0 * inv;
    return 0u - inv;  // -inv mod 2^32
}

}  // namespace

Montgomery::Montgomery(const BigInt& modulus) : n_(modulus) {
    if (n_.is_even()) throw std::invalid_argument("Montgomery modulus must be odd");
    if (n_ < BigInt(uint64_t(3))) throw std::invalid_argument("Montgomery modulus must be >= 3");

    s_ = n_.word_count();
    n_limbs_ = n_.limbs_padded(s_);
    n_prime_ = neg_inv_mod_2_32(n_limbs_[0]);

    // R mod N, computed by doubling 1 a total of 32*s times modulo N (no division).
    BigInt r_mod_n(1u);
    for (size_t i = 0; i < 32 * s_; ++i) {
        r_mod_n = r_mod_n.shl(1);
        if (r_mod_n >= n_) r_mod_n = r_mod_n.sub(n_);
    }
    one_mont_ = r_mod_n.limbs_padded(s_);

    // R^2 mod N = (R mod N)^2 mod N.
    r2_ = (r_mod_n.mul(r_mod_n).mod(n_)).limbs_padded(s_);
}

void Montgomery::monpro(uint32_t* out, const uint32_t* a, const uint32_t* b) const {
    if (op_counter_) ++(*op_counter_);  // diagnostics only; off by default
    const size_t s = s_;
    const uint32_t* n = n_limbs_.data();

    // T has s+2 limbs; thread_local so the ladder doesn't allocate per multiply.
    static thread_local std::vector<uint32_t> T;
    static thread_local std::vector<uint32_t> tmp;
    T.assign(s + 2, 0);
    tmp.assign(s, 0);

    for (size_t i = 0; i < s; ++i) {
        // T += a * b[i]
        uint64_t carry = 0;
        for (size_t j = 0; j < s; ++j) {
            const uint64_t p =
                static_cast<uint64_t>(a[j]) * b[i] + T[j] + carry;
            T[j] = static_cast<uint32_t>(p);
            carry = p >> 32;
        }
        uint64_t sum = static_cast<uint64_t>(T[s]) + carry;
        T[s] = static_cast<uint32_t>(sum);
        T[s + 1] = static_cast<uint32_t>(sum >> 32);

        // m = T[0] * n' mod 2^32, then T = (T + m*N) / 2^32
        const uint32_t m = static_cast<uint32_t>(static_cast<uint64_t>(T[0]) * n_prime_);
        carry = (static_cast<uint64_t>(m) * n[0] + T[0]) >> 32;  // T[0] becomes 0, shifted out
        for (size_t j = 1; j < s; ++j) {
            const uint64_t p = static_cast<uint64_t>(m) * n[j] + T[j] + carry;
            T[j - 1] = static_cast<uint32_t>(p);
            carry = p >> 32;
        }
        sum = static_cast<uint64_t>(T[s]) + carry;
        T[s - 1] = static_cast<uint32_t>(sum);
        T[s] = T[s + 1] + static_cast<uint32_t>(sum >> 32);
    }

    // Now value = T[s]*2^(32s) + T[0..s-1], guaranteed < 2N. Conditionally
    // subtract N in constant time.
    uint64_t borrow = 0;
    for (size_t j = 0; j < s; ++j) {
        const uint64_t d = static_cast<uint64_t>(T[j]) - n[j] - borrow;
        tmp[j] = static_cast<uint32_t>(d);
        borrow = (d >> 32) & 1u;
    }
    // T >= N  <=>  the borrow chain (including the extra high limb T[s]) does not
    // underflow. final = T[s] - borrow; underflow (top bit set) means T < N.
    const uint64_t final = static_cast<uint64_t>(T[s]) - borrow;
    const uint32_t ge_bit = (final >> 63) ? 0u : 1u;
    if (extra_redc_) *extra_redc_ += ge_bit;  // diagnostics only; off by default

    const ct::mask32 mask = ct::mask_from_bit(ge_bit);
    ct::select_array(mask, out, tmp.data(), T.data(), s);
}

std::vector<uint32_t> Montgomery::to_mont_limbs(const BigInt& a) const {
    std::vector<uint32_t> x = a.mod(n_).limbs_padded(s_);
    std::vector<uint32_t> out(s_, 0);
    monpro(out.data(), x.data(), r2_.data());  // a*R^2*R^-1 = a*R mod N
    return out;
}

BigInt Montgomery::to_montgomery(const BigInt& a) const {
    return BigInt(to_mont_limbs(a));
}

BigInt Montgomery::from_montgomery(const BigInt& a) const {
    std::vector<uint32_t> one(s_, 0);
    one[0] = 1;
    std::vector<uint32_t> x = a.mod(n_).limbs_padded(s_);
    std::vector<uint32_t> out(s_, 0);
    monpro(out.data(), x.data(), one.data());  // a*1*R^-1 mod N
    return BigInt(out);
}

BigInt Montgomery::mont_product(const BigInt& a, const BigInt& b) const {
    std::vector<uint32_t> x = a.mod(n_).limbs_padded(s_);
    std::vector<uint32_t> y = b.mod(n_).limbs_padded(s_);
    std::vector<uint32_t> out(s_, 0);
    monpro(out.data(), x.data(), y.data());
    return BigInt(out);
}

BigInt Montgomery::one_mont() const { return BigInt(one_mont_); }

BigInt Montgomery::pow(const BigInt& base, const BigInt& exp) const {
    // Left-to-right square-and-multiply in the Montgomery domain.
    // NOT constant time: the multiply is skipped when the exponent bit is 0.
    std::vector<uint32_t> r = one_mont_;             // 1 in the domain
    std::vector<uint32_t> b = to_mont_limbs(base);
    for (size_t i = exp.bit_length(); i-- > 0;) {
        monpro(r.data(), r.data(), r.data());        // square
        if (exp.test_bit(i)) monpro(r.data(), r.data(), b.data());  // multiply
    }
    std::vector<uint32_t> one(s_, 0);
    one[0] = 1;
    std::vector<uint32_t> out(s_, 0);
    monpro(out.data(), r.data(), one.data());        // back from the domain
    return BigInt(out);
}

BigInt Montgomery::pow_ct(const BigInt& base, const BigInt& exp, size_t fixed_bits) const {
    // Iterate over a fixed window so a secret exponent's length cannot leak. The
    // window is padded UP to fixed_bits but never below the exponent's true
    // length -- truncating high bits would silently corrupt the result. For a
    // secret exponent pass fixed_bits = modulus().bit_length() (>= any exponent).
    size_t bits = std::max(fixed_bits, exp.bit_length());
    if (bits == 0) bits = 1;  // exp == 0 -> result 1

    // Montgomery powering ladder. Invariant: (R0, R1) hold (g^k, g^(k+1)) in the
    // Montgomery domain. Every iteration does exactly one multiply and one square
    // plus two constant-time conditional swaps, independent of the bit value.
    std::vector<uint32_t> R0 = one_mont_;            // g^0 = 1
    std::vector<uint32_t> R1 = to_mont_limbs(base);  // g^1
    for (size_t i = bits; i-- > 0;) {
        const uint32_t bit = exp.test_bit(i) ? 1u : 0u;
        const ct::mask32 mask = ct::mask_from_bit(bit);
        ct::cswap_array(mask, R0.data(), R1.data(), s_);
        monpro(R1.data(), R0.data(), R1.data());     // R1 = R0 * R1
        monpro(R0.data(), R0.data(), R0.data());     // R0 = R0^2
        ct::cswap_array(mask, R0.data(), R1.data(), s_);
    }
    std::vector<uint32_t> one(s_, 0);
    one[0] = 1;
    std::vector<uint32_t> out(s_, 0);
    monpro(out.data(), R0.data(), one.data());
    return BigInt(out);
}

}  // namespace montx
