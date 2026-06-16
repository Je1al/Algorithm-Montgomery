// SPDX-License-Identifier: MIT
#include "framework.hpp"
#include "montx/bigint.hpp"

using montx::BigInt;

int main() {
    montx::test::Suite s("bigint");

    // hex <-> dec round trips
    s.eq(BigInt("ff").to_dec(), std::string("255"), "0xff == 255");
    s.eq(BigInt(uint64_t(255)).to_hex(), std::string("ff"), "255 == 0xff");
    s.eq(BigInt("0").to_dec(), std::string("0"), "zero dec");
    s.eq(BigInt("0x1A2B3C4D5E6F").to_hex(), std::string("1a2b3c4d5e6f"), "0x prefix + case");

    // arithmetic against known values
    s.eq((BigInt("ffffffff") + BigInt("1")).to_hex(), std::string("100000000"), "carry across limb");
    s.eq((BigInt("100000000") - BigInt("1")).to_hex(), std::string("ffffffff"), "borrow across limb");
    s.eq((BigInt("deadbeef") * BigInt("cafebabe")).to_dec(),
         std::string("12723420444339690338"), "mul 32x32");

    // big multiply checked in decimal: (2^64) * (2^64) = 2^128
    BigInt two64("10000000000000000");  // 2^64 in hex
    s.eq((two64 * two64).to_hex(), std::string("100000000000000000000000000000000"), "2^64 squared");

    // divmod: 123456789 / 1000 = 123456 r 789
    {
        BigInt q, r;
        BigInt::divmod(BigInt(uint64_t(123456789)), BigInt(uint64_t(1000)), q, r);
        s.eq(q.to_dec(), std::string("123456"), "div quotient");
        s.eq(r.to_dec(), std::string("789"), "div remainder");
    }

    // mod with a large operand (2^96 - 1) mod 0x100000007
    s.eq((BigInt("ffffffffffffffffffffffff") % BigInt("100000007")).to_dec(),
         std::string("4294966959"), "mod large operand KAT");

    // shifts
    s.eq((BigInt("1") << 64).to_hex(), std::string("10000000000000000"), "shl 64");
    s.eq((BigInt("10000000000000000") >> 64).to_hex(), std::string("1"), "shr 64");
    s.eq((BigInt("abcdef") << 4).to_hex(), std::string("abcdef0"), "shl 4");
    s.eq((BigInt("abcdef0") >> 4).to_hex(), std::string("abcdef"), "shr 4");
    s.eq((BigInt("123456789abcdef0") >> 13).to_hex(),
         (BigInt("123456789abcdef0") / (BigInt("1") << 13)).to_hex(), "shr == div by 2^k");

    // gcd / modular inverse
    s.eq(BigInt::gcd(BigInt(uint64_t(1071)), BigInt(uint64_t(462))).to_dec(),
         std::string("21"), "gcd(1071,462)=21");
    {
        // 3 * 4 = 12 == 1 mod 11
        BigInt inv = BigInt::mod_inverse(BigInt(uint64_t(3)), BigInt(uint64_t(11)));
        s.eq(inv.to_dec(), std::string("4"), "3^-1 mod 11 = 4");
        // inverse mod a composite (RSA-style): 17^-1 mod 3120 = 2753
        BigInt d = BigInt::mod_inverse(BigInt(uint64_t(17)), BigInt(uint64_t(3120)));
        s.eq(d.to_dec(), std::string("2753"), "17^-1 mod 3120 = 2753 (RSA classic)");
        s.eq((BigInt(uint64_t(17)) * d % BigInt(uint64_t(3120))).to_dec(), std::string("1"),
             "e*d == 1 mod phi");
    }

    // byte round trip
    {
        BigInt v("0123456789abcdef");
        auto bytes = v.to_bytes_be();
        s.eq(BigInt::from_bytes_be(bytes).to_hex(), v.to_hex(), "bytes_be round trip");
    }

    // introspection
    MONTX_CHECK(s, BigInt("8000").bit_length() == 16);
    MONTX_CHECK(s, BigInt("ffffffff").bit_length() == 32);
    MONTX_CHECK(s, BigInt("100000000").bit_length() == 33);
    MONTX_CHECK(s, BigInt(uint64_t(5)).is_odd());
    MONTX_CHECK(s, BigInt(uint64_t(6)).is_even());
    MONTX_CHECK(s, BigInt("100000000").test_bit(32));
    MONTX_CHECK(s, !BigInt("100000000").test_bit(31));

    return s.summary();
}
