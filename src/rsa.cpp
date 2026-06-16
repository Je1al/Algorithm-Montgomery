// SPDX-License-Identifier: MIT
#include "montx/rsa.hpp"

#include <stdexcept>

#include "montx/modexp.hpp"
#include "montx/primes.hpp"

namespace montx {

RsaKeyPair rsa_generate(size_t bits, const RandomBytes& rng, const BigInt& e) {
    if (bits < 16) throw std::invalid_argument("RSA modulus too small");
    const BigInt one(1u);
    const size_t pbits = bits / 2;
    const size_t qbits = bits - pbits;

    while (true) {
        BigInt p = generate_prime(pbits, rng);
        BigInt q = generate_prime(qbits, rng);
        if (p == q) continue;
        BigInt n = p.mul(q);
        if (n.bit_length() != bits) continue;  // force exact modulus width

        BigInt phi = p.sub(one).mul(q.sub(one));
        if (!(BigInt::gcd(e, phi) == one)) continue;  // e must be invertible mod phi

        RsaKeyPair kp;
        kp.pub.n = n;
        kp.pub.e = e;
        kp.priv.n = n;
        kp.priv.d = BigInt::mod_inverse(e, phi);
        kp.priv.p = p;
        kp.priv.q = q;
        kp.priv.dp = kp.priv.d.mod(p.sub(one));
        kp.priv.dq = kp.priv.d.mod(q.sub(one));
        kp.priv.qinv = BigInt::mod_inverse(q, p);
        kp.priv.bits = bits;
        return kp;
    }
}

BigInt rsa_encrypt(const RsaPublicKey& pk, const BigInt& m) {
    if (m >= pk.n) throw std::invalid_argument("message must be < modulus");
    return mod_exp_montgomery(m, pk.e, pk.n);
}

BigInt rsa_decrypt(const RsaPrivateKey& sk, const BigInt& c) {
    if (c >= sk.n) throw std::invalid_argument("ciphertext must be < modulus");
    return mod_exp_ct(c, sk.d, sk.n, sk.n.bit_length());
}

BigInt rsa_decrypt_crt(const RsaPrivateKey& sk, const BigInt& c) {
    if (c >= sk.n) throw std::invalid_argument("ciphertext must be < modulus");
    // m1 = c^dp mod p,  m2 = c^dq mod q  (exponentiations are constant-time).
    const BigInt m1 = mod_exp_ct(c, sk.dp, sk.p, sk.p.bit_length());
    const BigInt m2 = mod_exp_ct(c, sk.dq, sk.q, sk.q.bit_length());
    // Garner recombination: m = m2 + q * ((m1 - m2) * qinv mod p).
    const BigInt m2p = m2.mod(sk.p);
    const BigInt diff = m1.add(sk.p).sub(m2p).mod(sk.p);  // (m1 - m2) mod p, kept non-negative
    const BigInt h = diff.mul(sk.qinv).mod(sk.p);
    return m2.add(h.mul(sk.q));
}

}  // namespace montx
