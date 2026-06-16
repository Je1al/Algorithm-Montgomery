// SPDX-License-Identifier: MIT
#include "framework.hpp"
#include "montx/bigint.hpp"
#include "montx/modexp.hpp"
#include "montx/primes.hpp"
#include "montx/rng.hpp"

using montx::BigInt;

int main() {
    montx::test::Suite s("primes");
    auto rng = montx::seeded_random(12345);

    // Small known primes / composites.
    MONTX_CHECK(s, montx::is_probable_prime(BigInt(uint64_t(2)), 20, rng));
    MONTX_CHECK(s, montx::is_probable_prime(BigInt(uint64_t(97)), 20, rng));
    MONTX_CHECK(s, !montx::is_probable_prime(BigInt(uint64_t(1)), 20, rng));
    MONTX_CHECK(s, !montx::is_probable_prime(BigInt(uint64_t(100)), 20, rng));
    MONTX_CHECK(s, montx::is_probable_prime(BigInt(uint64_t(2147483647)), 20, rng));  // 2^31-1

    // Composites with NO small factors -> only Miller-Rabin can reject them.
    // 1009 * 1013 = 1022117 and 10007 * 10009 = 100160063 (all factors > table).
    MONTX_CHECK(s, !montx::is_probable_prime(BigInt(uint64_t(1022117)), 20, rng));
    MONTX_CHECK(s, !montx::is_probable_prime(BigInt(uint64_t(100160063)), 20, rng));

    // Generated primes: correct width, pass an independent re-check, and satisfy
    // Fermat's little theorem (a^(p-1) == 1 mod p) for several bases.
    for (size_t bits : {128u, 192u, 256u}) {
        BigInt p = montx::generate_prime(bits, rng);
        s.eq(p.bit_length(), bits, "generated prime has exact width");
        MONTX_CHECK(s, p.is_odd());
        MONTX_CHECK(s, montx::is_probable_prime(p, 40, rng));
        const BigInt pm1 = p.sub(BigInt(1u));
        bool fermat_ok = true;
        for (uint64_t a : {2ull, 3ull, 5ull, 7ull, 65537ull}) {
            if (montx::mod_exp_montgomery(BigInt(a), pm1, p) != BigInt(1u)) fermat_ok = false;
        }
        s.check(fermat_ok, "generated prime satisfies Fermat's little theorem");
    }

    return s.summary();
}
