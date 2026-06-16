// SPDX-License-Identifier: MIT
//
// Arbitrary-precision unsigned integer.
//
// Little-endian vector of 32-bit limbs, base 2^32. This type is the *general
// purpose* arithmetic layer: it is used for I/O, key setup, modular inverses and
// the schoolbook reference implementations. It is intentionally NOT used on the
// secret-key constant-time hot path -- there the code drops to fixed-width limb
// arrays (see montgomery.hpp), because the variable length of a normalized
// BigInt is itself a side channel.

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace montx {

class BigInt {
public:
    // --- construction -------------------------------------------------------
    BigInt();                                       // 0
    explicit BigInt(uint64_t value);
    explicit BigInt(const std::string& hex);        // hex, no "0x", case-insensitive
    explicit BigInt(std::vector<uint32_t> limbs);   // little-endian, takes ownership

    BigInt(const BigInt&) = default;
    BigInt(BigInt&&) noexcept = default;
    BigInt& operator=(const BigInt&) = default;
    BigInt& operator=(BigInt&&) noexcept = default;
    ~BigInt() = default;

    static BigInt from_hex(const std::string& hex);
    static BigInt from_u64(uint64_t value);
    static BigInt from_bytes_be(const std::vector<uint8_t>& bytes);

    // --- conversion ---------------------------------------------------------
    std::string to_hex() const;
    std::string to_dec() const;
    uint64_t to_u64() const;
    std::vector<uint8_t> to_bytes_be(size_t length = 0) const;

    // --- comparison ---------------------------------------------------------
    int compare(const BigInt& o) const;
    bool operator==(const BigInt& o) const { return compare(o) == 0; }
    bool operator!=(const BigInt& o) const { return compare(o) != 0; }
    bool operator<(const BigInt& o) const { return compare(o) < 0; }
    bool operator>(const BigInt& o) const { return compare(o) > 0; }
    bool operator<=(const BigInt& o) const { return compare(o) <= 0; }
    bool operator>=(const BigInt& o) const { return compare(o) >= 0; }

    // --- arithmetic ---------------------------------------------------------
    BigInt add(const BigInt& o) const;
    BigInt sub(const BigInt& o) const;        // requires *this >= o
    BigInt mul(const BigInt& o) const;
    BigInt mod(const BigInt& m) const;        // *this mod m
    BigInt div(const BigInt& d) const;        // floor(*this / d)

    // Full division: *this = q*d + r, with 0 <= r < d.
    static void divmod(const BigInt& a, const BigInt& d, BigInt& q, BigInt& r);

    BigInt operator+(const BigInt& o) const { return add(o); }
    BigInt operator-(const BigInt& o) const { return sub(o); }
    BigInt operator*(const BigInt& o) const { return mul(o); }
    BigInt operator%(const BigInt& m) const { return mod(m); }
    BigInt operator/(const BigInt& d) const { return div(d); }

    // --- modular arithmetic helpers ----------------------------------------
    static BigInt gcd(const BigInt& a, const BigInt& b);
    // Modular inverse: returns x with (a*x) mod m == 1. Throws if gcd(a,m) != 1.
    static BigInt mod_inverse(const BigInt& a, const BigInt& m);

    // --- bit manipulation ---------------------------------------------------
    BigInt shl(size_t bits) const;
    BigInt shr(size_t bits) const;
    BigInt operator<<(size_t bits) const { return shl(bits); }
    BigInt operator>>(size_t bits) const { return shr(bits); }
    bool test_bit(size_t i) const;

    // --- introspection ------------------------------------------------------
    bool is_zero() const;
    bool is_odd() const { return (limbs_[0] & 1u) != 0; }
    bool is_even() const { return !is_odd(); }
    size_t bit_length() const;
    size_t word_count() const { return limbs_.size(); }
    const uint32_t* data() const { return limbs_.data(); }

    // Little-endian limbs, zero-padded (or truncated) to exactly `words`.
    std::vector<uint32_t> limbs_padded(size_t words) const;

private:
    std::vector<uint32_t> limbs_;  // little-endian, always normalized (no high zeros, never empty)
    void normalize();
};

}  // namespace montx
