// SPDX-License-Identifier: MIT
#include "montx/rng.hpp"

#include <memory>
#include <random>

namespace montx {

RandomBytes os_random() {
    return [](uint8_t* buf, size_t n) {
        std::random_device rd;
        size_t i = 0;
        while (i < n) {
            const uint32_t r = rd();
            for (int b = 0; b < 4 && i < n; ++b, ++i) {
                buf[i] = static_cast<uint8_t>(r >> (8 * b));
            }
        }
    };
}

RandomBytes seeded_random(uint64_t seed) {
    auto state = std::make_shared<std::mt19937_64>(seed);
    return [state](uint8_t* buf, size_t n) {
        size_t i = 0;
        while (i < n) {
            const uint64_t r = (*state)();
            for (int b = 0; b < 8 && i < n; ++b, ++i) {
                buf[i] = static_cast<uint8_t>(r >> (8 * b));
            }
        }
    };
}

}  // namespace montx
