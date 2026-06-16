// SPDX-License-Identifier: MIT
//
// Microbenchmarks: the whole point of Montgomery arithmetic is speed, so measure
// it. Compares the schoolbook (division-based) modular exponentiation against the
// Montgomery fast path and the constant-time ladder across modulus sizes, then
// times RSA key generation and decryption (plain vs CRT).

#include <chrono>
#include <cstdio>
#include <vector>

#include "montx/bigint.hpp"
#include "montx/modexp.hpp"
#include "montx/montgomery.hpp"
#include "montx/primes.hpp"
#include "montx/rng.hpp"
#include "montx/rsa.hpp"

using namespace montx;
using Clock = std::chrono::steady_clock;

namespace {

template <typename F>
double time_ms(int iters, F&& f) {
    auto t0 = Clock::now();
    for (int i = 0; i < iters; ++i) {
        volatile auto out = f();
        (void)out;
    }
    auto t1 = Clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;
}

}  // namespace

int main() {
    auto rng = seeded_random(7);

    std::printf("Montgomery vs schoolbook modular exponentiation\n");
    std::printf("(lower is better; speedup = schoolbook / montgomery)\n\n");
    std::printf("%-8s %14s %14s %14s %9s\n", "bits", "schoolbook", "montgomery", "ladder(CT)",
                "speedup");
    std::printf("--------------------------------------------------------------------\n");

    for (size_t bits : {256u, 512u, 1024u, 2048u}) {
        BigInt mod = generate_prime(bits, rng);
        BigInt base = random_below(mod, rng);
        BigInt exp = random_bits(bits, rng);
        Montgomery ctx(mod);

        const int iters = bits >= 2048 ? 20 : (bits >= 1024 ? 60 : 300);
        const double t_school = time_ms(iters, [&] { return mod_exp(base, exp, mod); });
        const double t_mont = time_ms(iters, [&] { return ctx.pow(base, exp); });
        const double t_ladder = time_ms(iters, [&] { return ctx.pow_ct(base, exp, bits); });

        std::printf("%-8zu %11.3f ms %11.3f ms %11.3f ms %8.2fx\n", bits, t_school, t_mont,
                    t_ladder, t_school / t_mont);
    }

    std::printf("\nRSA operations\n");
    std::printf("--------------------------------------------------------------------\n");
    for (size_t bits : {1024u, 2048u}) {
        const double t_keygen = time_ms(bits >= 2048 ? 3 : 6, [&] { return rsa_generate(bits, rng); });
        RsaKeyPair kp = rsa_generate(bits, rng);
        BigInt c = rsa_encrypt(kp.pub, random_below(kp.pub.n, rng));
        const double t_dec = time_ms(20, [&] { return rsa_decrypt(kp.priv, c); });
        const double t_crt = time_ms(20, [&] { return rsa_decrypt_crt(kp.priv, c); });
        std::printf("RSA-%zu  keygen %8.1f ms | decrypt(CT) %7.3f ms | decrypt(CRT) %7.3f ms\n",
                    bits, t_keygen, t_dec, t_crt);
    }
    return 0;
}
