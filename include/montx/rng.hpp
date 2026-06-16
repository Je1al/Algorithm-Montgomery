// SPDX-License-Identifier: MIT
//
// Randomness abstraction. `RandomBytes` is a sink that fills a buffer with
// uniformly random bytes; everything that needs entropy (prime search, RSA key
// generation, the timing lab) takes one, so a caller can plug in a vetted CSPRNG.
//
// `os_random` pulls from the platform entropy source via std::random_device. For
// a real product the key-material RNG should be a hardened, well-tested CSPRNG
// (e.g. an SP 800-90A DRBG seeded from a validated entropy source) -- see my
// separate CSPRNG / entropy-toolkit project. It is kept pluggable on purpose.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace montx {

using RandomBytes = std::function<void(uint8_t*, size_t)>;

// Entropy from the operating system (std::random_device).
RandomBytes os_random();

// Deterministic RNG for tests and reproducible demos. NOT for key material.
RandomBytes seeded_random(uint64_t seed);

}  // namespace montx
