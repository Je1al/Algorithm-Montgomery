// SPDX-License-Identifier: MIT
#include "framework.hpp"
#include "montx/bigint.hpp"
#include "montx/primes.hpp"
#include "montx/rng.hpp"
#include "montx/rsa.hpp"

using montx::BigInt;

int main() {
    montx::test::Suite s("rsa");
    auto rng = montx::seeded_random(0xC0FFEE);

    // 512-bit demo key (kept modest so the test stays fast).
    montx::RsaKeyPair kp = montx::rsa_generate(512, rng);
    s.eq(kp.pub.n.bit_length(), size_t(512), "modulus has requested width");
    MONTX_CHECK(s, kp.pub.n == kp.priv.p.mul(kp.priv.q));
    // e*d == 1 mod lcm-ish: verify via e*d mod phi == 1
    {
        BigInt phi = kp.priv.p.sub(BigInt(1u)).mul(kp.priv.q.sub(BigInt(1u)));
        s.eq(kp.pub.e.mul(kp.priv.d).mod(phi).to_dec(), std::string("1"), "e*d == 1 mod phi");
    }

    bool roundtrip = true, crt_match = true, sign_ok = true;
    for (int i = 0; i < 8; ++i) {
        BigInt m = montx::random_below(kp.pub.n, rng);
        BigInt c = montx::rsa_encrypt(kp.pub, m);
        if (montx::rsa_decrypt(kp.priv, c) != m) roundtrip = false;
        if (montx::rsa_decrypt_crt(kp.priv, c) != m) crt_match = false;
        // sign then verify recovers the message
        BigInt sig = montx::rsa_sign(kp.priv, m);
        if (montx::rsa_verify(kp.pub, sig) != m) sign_ok = false;
    }
    s.check(roundtrip, "encrypt -> decrypt (constant-time) round trip");
    s.check(crt_match, "CRT decryption matches plain decryption");
    s.check(sign_ok, "sign -> verify round trip");

    return s.summary();
}
