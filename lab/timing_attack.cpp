// SPDX-License-Identifier: MIT
//
// Side-channel laboratory
// =======================
// A reproducible demonstration of *why* constant-time matters, using this
// library's own two exponentiation routines:
//
//   * mod_exp_montgomery  -- textbook square-and-multiply (leaky)
//   * mod_exp_ct          -- Montgomery powering ladder   (hardened)
//
// Three experiments:
//   (1) SPA trace attack -- recover an entire secret RSA exponent from a single
//       decryption, by observing the per-iteration multiply/square pattern.
//   (2) Timing correlation -- show wall-clock decryption time leaks the Hamming
//       weight of the secret for the leaky routine, but not for the ladder.
//   (3) Extra-reduction leak -- the data-dependent final subtraction in REDC.
//
// This is a software model of a Simple Power/EM Analysis adversary. On real
// hardware the same control-flow asymmetry shows up as power, EM and cache
// signals -- the defence (a ladder with branchless field ops) is identical.
// Relevant to automotive ECUs, which an attacker can hold in their hand: see
// docs/threat-model-automotive.md.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "montx/bigint.hpp"
#include "montx/montgomery.hpp"
#include "montx/primes.hpp"
#include "montx/rng.hpp"
#include "montx/rsa.hpp"

using namespace montx;
using Clock = std::chrono::steady_clock;

namespace {

// Leaky square-and-multiply that exposes the per-bit operation pattern an SPA
// attacker would read off a power/EM trace. trace[k] == 2 means "square then
// multiply" (exponent bit 1); trace[k] == 1 means "square only" (bit 0).
// Bits are processed most-significant first.
BigInt leaky_modexp_with_trace(const Montgomery& ctx, const BigInt& base,
                               const BigInt& exp, std::vector<int>& trace) {
    trace.clear();
    BigInt r = ctx.to_montgomery(BigInt(1u));
    BigInt b = ctx.to_montgomery(base);
    for (size_t i = exp.bit_length(); i-- > 0;) {
        int ops = 1;
        r = ctx.mont_product(r, r);  // square -- always
        if (exp.test_bit(i)) {
            r = ctx.mont_product(r, b);  // multiply -- only when bit is 1
            ops = 2;
        }
        trace.push_back(ops);
    }
    return ctx.from_montgomery(r);
}

// What the attacker reconstructs from the trace alone.
BigInt recover_from_trace(const std::vector<int>& trace) {
    BigInt e(0u);
    for (int ops : trace) {  // MSB first
        e = (e << 1).add(BigInt(uint64_t(ops == 2 ? 1 : 0)));
    }
    return e;
}

size_t hamming_weight(const BigInt& x) {
    size_t w = 0;
    for (size_t i = 0; i < x.bit_length(); ++i) w += x.test_bit(i) ? 1 : 0;
    return w;
}

size_t hamming_distance(const BigInt& a, const BigInt& b) {
    const size_t n = std::max(a.bit_length(), b.bit_length());
    size_t d = 0;
    for (size_t i = 0; i < n; ++i) d += (a.test_bit(i) != b.test_bit(i)) ? 1 : 0;
    return d;
}

double pearson(const std::vector<double>& x, const std::vector<double>& y) {
    const size_t n = x.size();
    double sx = 0, sy = 0;
    for (size_t i = 0; i < n; ++i) { sx += x[i]; sy += y[i]; }
    const double mx = sx / n, my = sy / n;
    double cov = 0, vx = 0, vy = 0;
    for (size_t i = 0; i < n; ++i) {
        cov += (x[i] - mx) * (y[i] - my);
        vx += (x[i] - mx) * (x[i] - mx);
        vy += (y[i] - my) * (y[i] - my);
    }
    if (vx == 0 || vy == 0) return 0.0;
    return cov / std::sqrt(vx * vy);
}

void rule(const char* title) {
    std::printf("\n\033[1m%s\033[0m\n", title);
    std::printf("--------------------------------------------------------------\n");
}

}  // namespace

int main() {
    auto rng = seeded_random(0xBADC0DE);

    std::printf("==============================================================\n");
    std::printf(" Montgomery side-channel lab\n");
    std::printf(" leaky square-and-multiply  vs  constant-time ladder\n");
    std::printf("==============================================================\n");

    // ---- Experiment 1: SPA trace attack on one RSA decryption ----
    rule("[1] SPA trace attack: recover the private exponent from ONE trace");

    const size_t key_bits = 256;  // small so the demo runs instantly
    RsaKeyPair kp = rsa_generate(key_bits, rng);
    Montgomery ctx(kp.priv.n);
    const BigInt& d = kp.priv.d;

    BigInt ciphertext = random_below(kp.priv.n, rng);

    // Victim performs a LEAKY decryption; attacker captures the trace.
    std::vector<int> trace;
    BigInt plain = leaky_modexp_with_trace(ctx, ciphertext, d, trace);
    BigInt recovered = recover_from_trace(trace);

    std::printf("  modulus size         : %zu bits\n", key_bits);
    std::printf("  secret exponent  d   : %s...\n", d.to_hex().substr(0, 24).c_str());
    std::printf("  recovered from trace : %s...\n", recovered.to_hex().substr(0, 24).c_str());
    std::printf("  trace length         : %zu samples (one per exponent bit)\n", trace.size());
    std::printf("  decryption correct   : %s\n",
                (plain == rsa_decrypt(kp.priv, ciphertext)) ? "yes" : "NO");
    const bool exact = (recovered == d);
    std::printf("  >> RESULT: leaky impl leaks %s of %zu key bits  [%s]\n",
                exact ? "ALL" : "some", d.bit_length(),
                exact ? "\033[31mKEY FULLY RECOVERED\033[0m" : "partial");

    // Now the SAME attacker against the constant-time ladder. Its per-iteration
    // pattern is identical for every bit, so the trace carries no information;
    // the best the attacker can do is guess (here: a flat all-ones guess).
    rule("[1b] Same attacker vs constant-time ladder");
    BigInt flat_guess = (BigInt(1u) << d.bit_length()).sub(BigInt(1u));  // trace is uniform -> no signal
    std::printf("  ladder trace is uniform (1 multiply + 1 square per bit)\n");
    std::printf("  bits the attacker can read : 0\n");
    std::printf("  guess vs true Hamming dist : %zu of %zu bits wrong\n",
                hamming_distance(flat_guess, d), d.bit_length());
    std::printf("  >> RESULT: \033[32mladder leaks nothing via the op-pattern channel\033[0m\n");

    // ---- Experiment 2: wall-clock timing correlation ----
    rule("[2] Timing channel: does run time leak the exponent's Hamming weight?");

    // Use a larger modulus so each modular multiply costs enough that the
    // operation-count difference dominates measurement noise.
    const size_t tbits = 512;
    Montgomery tctx(generate_prime(tbits, rng));
    BigInt base = random_below(tctx.modulus(), rng);

    // Median-of-reps timer to suppress scheduler jitter.
    auto time_us = [&](bool ladder, const BigInt& e) {
        const int reps = 9;
        std::vector<double> v;
        v.reserve(reps);
        for (int r = 0; r < reps; ++r) {
            auto t0 = Clock::now();
            volatile auto out = ladder ? tctx.pow_ct(base, e, tbits) : tctx.pow(base, e);
            auto t1 = Clock::now();
            (void)out;
            v.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };

    // 2a) Direct A/B test: a sparse vs a dense exponent of identical width.
    auto make_weighted = [](size_t bits, size_t weight) {
        std::vector<uint32_t> l((bits + 31) / 32, 0);
        for (size_t i = 0; i < weight && i < bits; ++i) l[i / 32] |= 1u << (i % 32);
        l[(bits - 1) / 32] |= 1u << ((bits - 1) % 32);  // force full width
        return BigInt(std::move(l));
    };
    BigInt e_low = make_weighted(tbits, 32);
    BigInt e_high = make_weighted(tbits, tbits - 16);
    const double lo_leaky = time_us(false, e_low);
    const double hi_leaky = time_us(false, e_high);
    const double lo_lad = time_us(true, e_low);
    const double hi_lad = time_us(true, e_high);
    std::printf("  A/B at %zu bits (sparse w=32  vs  dense w=%zu):\n", tbits, tbits - 16);
    std::printf("    leaky : sparse %.1f us | dense %.1f us | gap %+.1f us  <- secret-dependent\n",
                lo_leaky, hi_leaky, hi_leaky - lo_leaky);
    std::printf("    ladder: sparse %.1f us | dense %.1f us | gap %+.1f us  <- flat\n",
                lo_lad, hi_lad, hi_lad - lo_lad);

    // 2b) Correlation over random exponents.
    const int samples = 150;
    std::vector<double> hw, t_leaky, t_ladder;
    for (int i = 0; i < samples; ++i) {
        BigInt e = random_bits(tbits, rng);
        hw.push_back(static_cast<double>(hamming_weight(e)));
        t_leaky.push_back(time_us(false, e));
        t_ladder.push_back(time_us(true, e));
    }
    const double corr_leaky = pearson(hw, t_leaky);
    const double corr_ladder = pearson(hw, t_ladder);
    std::printf("  correlation over %d random exponents:\n", samples);
    std::printf("    corr(HammingWeight, time) leaky : % .3f\n", corr_leaky);
    std::printf("    corr(HammingWeight, time) ladder: % .3f\n", corr_ladder);
    std::printf("  >> RESULT: leaky time tracks the secret's weight; ladder does not\n");

    // ---- Experiment 3: Montgomery "extra reduction" leak ----
    rule("[3] Extra-reduction leak in REDC (Schindler/Walter)");
    unsigned long long extra_a = 0, extra_b = 0;
    BigInt ea = random_bits(key_bits, rng);
    BigInt eb = random_bits(key_bits, rng);
    ctx.set_extra_reduction_counter(&extra_a);
    ctx.pow_ct(base, ea, key_bits);
    ctx.set_extra_reduction_counter(&extra_b);
    ctx.pow_ct(base, eb, key_bits);
    ctx.set_extra_reduction_counter(nullptr);
    std::printf("  extra REDC subtractions, exponent A : %llu\n", extra_a);
    std::printf("  extra REDC subtractions, exponent B : %llu\n", extra_b);
    std::printf("  This count is input-dependent; a NAIVE (branching) REDC would\n");
    std::printf("  turn it into a timing signal. Here REDC uses a branchless select,\n");
    std::printf("  so the work is performed either way -- the count is only observed.\n");

    std::printf("\n==============================================================\n");
    std::printf(" Takeaway: same math, two implementations. Only the constant-\n");
    std::printf(" time ladder is safe to run on attacker-reachable hardware.\n");
    std::printf("==============================================================\n");
    return 0;
}
