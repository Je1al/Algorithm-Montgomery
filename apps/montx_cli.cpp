// SPDX-License-Identifier: MIT
//
// montx -- command-line front-end for the Montgomery arithmetic toolkit.
//
//   montx modexp <base> <exp> <mod> [--mode standard|montgomery|ct]
//   montx genprime <bits>
//   montx rsa <bits>            generate a key and run an encrypt/decrypt demo
//   montx selftest             quick cross-check of the three modexp paths
//
// All numeric arguments are hexadecimal (no "0x" needed).

#include <cstdio>
#include <cstring>
#include <string>

#include "montx/bigint.hpp"
#include "montx/modexp.hpp"
#include "montx/primes.hpp"
#include "montx/rng.hpp"
#include "montx/rsa.hpp"

using namespace montx;

namespace {

int usage() {
    std::printf(
        "montx -- Montgomery arithmetic toolkit\n\n"
        "usage:\n"
        "  montx modexp <base_hex> <exp_hex> <mod_hex> [--mode standard|montgomery|ct]\n"
        "  montx genprime <bits>\n"
        "  montx rsa <bits>\n"
        "  montx selftest\n");
    return 2;
}

int cmd_modexp(int argc, char** argv) {
    if (argc < 5) return usage();
    BigInt base(argv[2]), exp(argv[3]), mod(argv[4]);
    std::string mode = "montgomery";
    for (int i = 5; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--mode") == 0) mode = argv[i + 1];
    }
    BigInt r;
    if (mode == "standard") {
        r = mod_exp(base, exp, mod);
    } else if (mode == "ct") {
        r = mod_exp_ct(base, exp, mod, mod.bit_length());
    } else {
        r = mod_exp_montgomery(base, exp, mod);
    }
    std::printf("mode = %s\n", mode.c_str());
    std::printf("hex  = %s\n", r.to_hex().c_str());
    std::printf("dec  = %s\n", r.to_dec().c_str());
    return 0;
}

int cmd_genprime(int argc, char** argv) {
    if (argc < 3) return usage();
    const size_t bits = static_cast<size_t>(std::stoul(argv[2]));
    auto rng = os_random();
    BigInt p = generate_prime(bits, rng);
    std::printf("prime (%zu bits)\n", p.bit_length());
    std::printf("hex = %s\n", p.to_hex().c_str());
    std::printf("dec = %s\n", p.to_dec().c_str());
    return 0;
}

int cmd_rsa(int argc, char** argv) {
    if (argc < 3) return usage();
    const size_t bits = static_cast<size_t>(std::stoul(argv[2]));
    auto rng = os_random();
    std::printf("generating RSA-%zu key (textbook, no padding -- demo only)...\n", bits);
    RsaKeyPair kp = rsa_generate(bits, rng);
    std::printf("n = %s\n", kp.pub.n.to_hex().c_str());
    std::printf("e = %s\n", kp.pub.e.to_hex().c_str());
    std::printf("d = %s...\n", kp.priv.d.to_hex().substr(0, 32).c_str());

    BigInt m = random_below(kp.pub.n, rng);
    BigInt c = rsa_encrypt(kp.pub, m);
    BigInt m2 = rsa_decrypt_crt(kp.priv, c);
    std::printf("\nmessage   m = %s...\n", m.to_hex().substr(0, 32).c_str());
    std::printf("decrypted m = %s...\n", m2.to_hex().substr(0, 32).c_str());
    std::printf("round trip  : %s\n", (m == m2) ? "OK" : "FAILED");
    return (m == m2) ? 0 : 1;
}

int cmd_selftest() {
    auto rng = seeded_random(1);
    int fails = 0;
    for (size_t bits : {128u, 256u, 512u}) {
        BigInt mod = generate_prime(bits, rng);
        BigInt base = random_below(mod, rng);
        BigInt exp = random_bits(bits, rng);
        BigInt a = mod_exp(base, exp, mod);
        BigInt b = mod_exp_montgomery(base, exp, mod);
        BigInt c = mod_exp_ct(base, exp, mod, bits);
        const bool ok = (a == b && b == c);
        std::printf("  %4zu-bit modexp agreement: %s\n", bits, ok ? "OK" : "MISMATCH");
        if (!ok) ++fails;
    }
    std::printf("selftest: %s\n", fails == 0 ? "PASSED" : "FAILED");
    return fails == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    const std::string cmd = argv[1];
    try {
        if (cmd == "modexp") return cmd_modexp(argc, argv);
        if (cmd == "genprime") return cmd_genprime(argc, argv);
        if (cmd == "rsa") return cmd_rsa(argc, argv);
        if (cmd == "selftest") return cmd_selftest();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return usage();
}
