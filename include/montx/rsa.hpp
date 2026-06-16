// SPDX-License-Identifier: MIT
//
// RSA -- as a demonstration of Montgomery arithmetic doing real cryptographic
// work (key generation, encryption, signing). It exercises the whole stack:
// prime search -> modular inverse -> constant-time modular exponentiation.
//
// SCOPE: these are the *textbook* RSA primitives (no message padding). They show
// the number theory and the constant-time private-key path; they are NOT a
// secure encryption/signature scheme on their own. A production design needs
// RSAES-OAEP / RSASSA-PSS padding (see my separate RSA toolkit project), and
// CRT-RSA additionally needs fault-injection countermeasures (Bellcore attack).

#pragma once

#include <cstddef>

#include "montx/bigint.hpp"
#include "montx/rng.hpp"

namespace montx {

struct RsaPublicKey {
    BigInt n;  // modulus
    BigInt e;  // public exponent
};

struct RsaPrivateKey {
    BigInt n;     // modulus
    BigInt d;     // private exponent
    BigInt p, q;  // secret primes
    BigInt dp;    // d mod (p-1)
    BigInt dq;    // d mod (q-1)
    BigInt qinv;  // q^-1 mod p
    size_t bits = 0;
};

struct RsaKeyPair {
    RsaPublicKey pub;
    RsaPrivateKey priv;
};

// Generate a `bits`-bit RSA key (p, q each ~bits/2). Default public exponent 65537.
RsaKeyPair rsa_generate(size_t bits, const RandomBytes& rng,
                        const BigInt& e = BigInt(uint64_t(65537)));

// m^e mod n. Public exponent, so a fast (non-constant-time) path is fine.
BigInt rsa_encrypt(const RsaPublicKey& pk, const BigInt& m);

// c^d mod n via the constant-time Montgomery ladder.
BigInt rsa_decrypt(const RsaPrivateKey& sk, const BigInt& c);

// CRT decryption: ~4x faster. Uses constant-time exponentiation mod p and mod q.
BigInt rsa_decrypt_crt(const RsaPrivateKey& sk, const BigInt& c);

// Textbook signing is decryption with the private key; verification is encryption.
inline BigInt rsa_sign(const RsaPrivateKey& sk, const BigInt& m) {
    return rsa_decrypt_crt(sk, m);
}
inline BigInt rsa_verify(const RsaPublicKey& pk, const BigInt& sig) {
    return rsa_encrypt(pk, sig);
}

}  // namespace montx
