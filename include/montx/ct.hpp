// SPDX-License-Identifier: MIT
//
// Constant-time building blocks.
//
// Side-channel resistance starts here: every primitive in this header runs in
// time and with a memory-access pattern that is independent of the *values* of
// its inputs. No data-dependent branches, no data-dependent table indices, no
// early returns. These are the bricks the secret-key code paths are built from
// (the Montgomery ladder, the conditional final subtraction in REDC, ...).
//
// The functions deliberately avoid `if`/`?:` on secret data and instead compute
// with masks: a mask is all-ones (0xFFFF'FFFF) for "true" and all-zeros for
// "false", so that `select`/`cswap` collapse to straight-line bit arithmetic
// that a constant-time-aware reader (and, ideally, the CPU) treats uniformly.

#pragma once

#include <cstddef>
#include <cstdint>

namespace montx::ct {

using mask32 = uint32_t;

// 0 -> 0x00000000, 1 -> 0xFFFFFFFF. `bit` must be 0 or 1.
inline mask32 mask_from_bit(uint32_t bit) noexcept {
    return static_cast<mask32>(0) - (bit & 1u);
}

// Returns 0xFFFFFFFF if x != 0, else 0. No branch on x.
inline mask32 is_nonzero(uint32_t x) noexcept {
    return static_cast<mask32>(0) - ((x | (0u - x)) >> 31);
}

inline mask32 is_zero(uint32_t x) noexcept {
    return ~is_nonzero(x);
}

// Branchless select: returns (mask ? a : b). `mask` is all-ones or all-zeros.
inline uint32_t select(mask32 mask, uint32_t a, uint32_t b) noexcept {
    return b ^ (mask & (a ^ b));
}

// Branchless conditional swap of two words (swaps iff mask is all-ones).
inline void cswap(mask32 mask, uint32_t& a, uint32_t& b) noexcept {
    const uint32_t t = mask & (a ^ b);
    a ^= t;
    b ^= t;
}

// dst[i] = mask ? a[i] : b[i], over n little-endian limbs.
inline void select_array(mask32 mask, uint32_t* dst, const uint32_t* a,
                         const uint32_t* b, size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) {
        dst[i] = select(mask, a[i], b[i]);
    }
}

// Swap a[0..n) with b[0..n) iff mask is all-ones. Touches every limb either way.
inline void cswap_array(mask32 mask, uint32_t* a, uint32_t* b, size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) {
        cswap(mask, a[i], b[i]);
    }
}

// Constant-time unsigned comparison of two n-limb little-endian arrays.
// Returns 0xFFFFFFFF if a >= b, else 0. Always scans all n limbs.
inline mask32 geq(const uint32_t* a, const uint32_t* b, size_t n) noexcept {
    uint64_t borrow = 0;
    for (size_t i = 0; i < n; ++i) {
        const uint64_t d = static_cast<uint64_t>(a[i]) - b[i] - borrow;
        borrow = (d >> 32) & 1u;  // 1 iff a[i]-b[i]-borrow underflowed
    }
    // No final borrow <=> a >= b.
    return mask_from_bit(static_cast<uint32_t>(1 - borrow));
}

// Constant-time equality of two n-limb arrays: 0xFFFFFFFF if equal, else 0.
inline mask32 equal(const uint32_t* a, const uint32_t* b, size_t n) noexcept {
    uint32_t diff = 0;
    for (size_t i = 0; i < n; ++i) {
        diff |= a[i] ^ b[i];
    }
    return is_zero(diff);
}

}  // namespace montx::ct
