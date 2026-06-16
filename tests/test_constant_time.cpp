// SPDX-License-Identifier: MIT
//
// Tests for the constant-time primitives, and -- more importantly -- an
// automated proof of the security property the whole project is about: the
// Montgomery ladder performs a number of field operations that does NOT depend
// on the secret exponent, whereas square-and-multiply leaks its Hamming weight.

#include "framework.hpp"
#include "montx/bigint.hpp"
#include "montx/ct.hpp"
#include "montx/montgomery.hpp"

using montx::BigInt;
using montx::Montgomery;
namespace ct = montx::ct;

int main() {
    montx::test::Suite s("constant_time");

    // 1) Primitive correctness.
    s.eq(ct::mask_from_bit(1), uint32_t(0xFFFFFFFF), "mask_from_bit(1)");
    s.eq(ct::mask_from_bit(0), uint32_t(0), "mask_from_bit(0)");
    s.eq(ct::select(ct::mask_from_bit(1), 0xAAAA, 0xBBBB), uint32_t(0xAAAA), "select true");
    s.eq(ct::select(ct::mask_from_bit(0), 0xAAAA, 0xBBBB), uint32_t(0xBBBB), "select false");
    s.eq(ct::is_nonzero(0), uint32_t(0), "is_nonzero(0)");
    s.eq(ct::is_nonzero(7), uint32_t(0xFFFFFFFF), "is_nonzero(7)");
    {
        uint32_t a = 1, b = 2;
        ct::cswap(ct::mask_from_bit(1), a, b);
        s.check(a == 2 && b == 1, "cswap swaps when mask set");
        ct::cswap(ct::mask_from_bit(0), a, b);
        s.check(a == 2 && b == 1, "cswap no-op when mask clear");
    }
    {
        uint32_t x[3] = {1, 0, 0}, y[3] = {2, 0, 0};
        s.eq(ct::geq(y, x, 3), uint32_t(0xFFFFFFFF), "geq 2 >= 1");
        s.eq(ct::geq(x, y, 3), uint32_t(0), "geq 1 >= 2 is false");
        s.eq(ct::geq(x, x, 3), uint32_t(0xFFFFFFFF), "geq x >= x");
        s.eq(ct::equal(x, x, 3), uint32_t(0xFFFFFFFF), "equal x == x");
        s.eq(ct::equal(x, y, 3), uint32_t(0), "equal x != y");
    }

    // 2) Operation-count leakage test.
    Montgomery ctx(BigInt("97fc695a07a0ca6e0822e8f36c031199972a846916419f828b9d2434e465e151"));
    const size_t W = ctx.modulus().bit_length();  // fixed window for secret exponents

    // Three exponents of identical width but wildly different Hamming weight.
    BigInt e_sparse = BigInt(1u) << 200;                 // popcount 1
    BigInt e_dense = (BigInt(1u) << 201).sub(BigInt(1u));// popcount 201 (all ones)
    BigInt e_mid = BigInt("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");  // ~half ones

    auto count_ladder = [&](const BigInt& e) {
        unsigned long long ops = 0;
        ctx.set_op_counter(&ops);
        ctx.pow_ct(BigInt(uint64_t(3)), e, W);
        ctx.set_op_counter(nullptr);
        return ops;
    };
    auto count_naive = [&](const BigInt& e) {
        unsigned long long ops = 0;
        ctx.set_op_counter(&ops);
        ctx.pow(BigInt(uint64_t(3)), e);
        ctx.set_op_counter(nullptr);
        return ops;
    };

    const unsigned long long l_sparse = count_ladder(e_sparse);
    const unsigned long long l_dense = count_ladder(e_dense);
    const unsigned long long l_mid = count_ladder(e_mid);
    s.check(l_sparse == l_dense && l_dense == l_mid,
            "ladder: MonPro count is independent of the secret exponent");
    s.eq(l_sparse, 2ull + 2ull * W, "ladder: exactly 2 + 2*width MonPro operations");

    const unsigned long long n_sparse = count_naive(e_sparse);
    const unsigned long long n_dense = count_naive(e_dense);
    s.check(n_sparse != n_dense,
            "square-and-multiply: MonPro count LEAKS the exponent (counts differ)");

    return s.summary();
}
