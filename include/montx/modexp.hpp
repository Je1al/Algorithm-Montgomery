// SPDX-License-Identifier: MIT
//
// Free-function modular exponentiation, three ways. Having three independent
// implementations that must agree is itself a strong correctness test (see
// tests/test_montgomery.cpp), and it lets the benchmark and the side-channel lab
// compare them directly.

#pragma once

#include <cstddef>

#include "montx/bigint.hpp"

namespace montx {

// Schoolbook square-and-multiply with explicit `mod` reductions. Reference
// implementation: simplest to audit, works for any modulus, not constant time.
BigInt mod_exp(const BigInt& base, const BigInt& exp, const BigInt& mod);

// Fast Montgomery exponentiation. Requires an odd modulus. Not constant time
// (the multiply is conditional on each exponent bit).
BigInt mod_exp_montgomery(const BigInt& base, const BigInt& exp, const BigInt& mod);

// Constant-time Montgomery powering ladder. Requires an odd modulus. For a secret
// exponent pass fixed_bits = mod.bit_length() so the loop count cannot leak |exp|.
BigInt mod_exp_ct(const BigInt& base, const BigInt& exp, const BigInt& mod,
                  size_t fixed_bits = 0);

}  // namespace montx
