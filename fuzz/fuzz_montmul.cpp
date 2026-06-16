// SPDX-License-Identifier: MIT
//
// Differential fuzz target. For arbitrary attacker-controlled bytes it derives
// (modulus, a, b) and checks that the Montgomery code agrees with the simple
// schoolbook reference -- across domain conversion, the Montgomery product, and
// all three exponentiation routines. Any disagreement traps.
//
// Build with libFuzzer (clang):
//   clang++ -std=c++17 -g -O1 -fsanitize=fuzzer,address,undefined -Iinclude \
//       fuzz/fuzz_montmul.cpp src/*.cpp -o fuzz_montmul
//
// Or build a standalone replayer (any compiler) to run it over a corpus:
//   clang++ -std=c++17 -O2 -DMONTX_FUZZ_STANDALONE -Iinclude \
//       fuzz/fuzz_montmul.cpp src/*.cpp -o fuzz_replay && ./fuzz_replay seed1 seed2 ...

#include <cstdint>
#include <cstdio>
#include <vector>

#include "montx/bigint.hpp"
#include "montx/modexp.hpp"
#include "montx/montgomery.hpp"

using namespace montx;

namespace {
// Trap on mismatch regardless of NDEBUG -- a fuzzer finding must always crash.
void must(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "fuzz check failed: %s\n", msg);
        __builtin_trap();
    }
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 6) return 0;
    const size_t third = size / 3;
    auto slice = [&](size_t off, size_t len) {
        return std::vector<uint8_t>(data + off, data + off + len);
    };

    BigInt m = BigInt::from_bytes_be(slice(0, third));
    BigInt a = BigInt::from_bytes_be(slice(third, third));
    BigInt b = BigInt::from_bytes_be(slice(2 * third, size - 2 * third));

    if (m.is_even()) m = m.add(BigInt(1u));         // Montgomery needs an odd modulus
    if (m < BigInt(uint64_t(3))) return 0;

    Montgomery ctx(m);

    // 1) from_mont(MonPro(to_mont(a), to_mont(b))) == a*b mod m
    BigInt mont = ctx.from_montgomery(ctx.mont_product(ctx.to_montgomery(a), ctx.to_montgomery(b)));
    must(mont == a.mul(b).mod(m), "Montgomery product != schoolbook a*b mod m");

    // 2) all three exponentiation paths agree (base a, exponent b)
    BigInt ref = mod_exp(a, b, m);
    must(ref == mod_exp_montgomery(a, b, m), "fast Montgomery pow != reference");
    must(ref == mod_exp_ct(a, b, m, m.bit_length()), "constant-time ladder != reference");

    return 0;
}

#ifdef MONTX_FUZZ_STANDALONE
#include <cstdio>
int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        FILE* f = std::fopen(argv[i], "rb");
        if (!f) continue;
        std::vector<uint8_t> buf;
        int c;
        while ((c = std::fgetc(f)) != EOF) buf.push_back(static_cast<uint8_t>(c));
        std::fclose(f);
        LLVMFuzzerTestOneInput(buf.data(), buf.size());
        std::printf("ok: %s (%zu bytes)\n", argv[i], buf.size());
    }
    return 0;
}
#endif
