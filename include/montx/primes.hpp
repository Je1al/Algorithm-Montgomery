// SPDX-License-Identifier: MIT
//
// Primality testing and random prime generation -- the inputs RSA needs.
// The Miller-Rabin witness loop is built on the Montgomery exponentiation from
// montgomery.hpp, which is exactly where Montgomery arithmetic earns its keep:
// generating a key runs thousands of modular exponentiations.

#pragma once

#include <cstddef>

#include "montx/bigint.hpp"
#include "montx/rng.hpp"

namespace montx {

// Uniform random integer with exactly `bits` bits (the top bit is forced set).
BigInt random_bits(size_t bits, const RandomBytes& rng);

// Uniform random integer in [0, n).
BigInt random_below(const BigInt& n, const RandomBytes& rng);

// Miller-Rabin probabilistic primality test with `rounds` random witnesses.
// Composite numbers pass with probability < 4^-rounds.
bool is_probable_prime(const BigInt& n, int rounds, const RandomBytes& rng);

// Generate a probable prime of exactly `bits` bits.
BigInt generate_prime(size_t bits, const RandomBytes& rng, int mr_rounds = 40);

}  // namespace montx
