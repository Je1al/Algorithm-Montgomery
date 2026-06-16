// SPDX-License-Identifier: MIT
#include "montx/modexp.hpp"

#include <stdexcept>

#include "montx/montgomery.hpp"

namespace montx {

BigInt mod_exp(const BigInt& base, const BigInt& exp, const BigInt& mod) {
    if (mod.is_zero()) throw std::invalid_argument("modulus cannot be zero");
    BigInt result(1u);
    BigInt cur = base.mod(mod);
    for (size_t i = 0; i < exp.bit_length(); ++i) {
        if (exp.test_bit(i)) result = result.mul(cur).mod(mod);
        cur = cur.mul(cur).mod(mod);
    }
    return result.mod(mod);  // collapses to 0 when mod == 1
}

BigInt mod_exp_montgomery(const BigInt& base, const BigInt& exp, const BigInt& mod) {
    Montgomery ctx(mod);
    return ctx.pow(base, exp);
}

BigInt mod_exp_ct(const BigInt& base, const BigInt& exp, const BigInt& mod,
                  size_t fixed_bits) {
    Montgomery ctx(mod);
    return ctx.pow_ct(base, exp, fixed_bits);
}

}  // namespace montx
