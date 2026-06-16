// SPDX-License-Identifier: MIT
#include "montx/bigint.hpp"

#include <algorithm>
#include <cctype>

namespace montx {

namespace {

uint32_t hex_value(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint32_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint32_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<uint32_t>(c - 'A' + 10);
    throw std::invalid_argument("invalid hex digit");
}

char hex_char(uint32_t v) {
    return v < 10 ? static_cast<char>('0' + v) : static_cast<char>('a' + (v - 10));
}

}  // namespace

// --- construction -----------------------------------------------------------

BigInt::BigInt() : limbs_(1, 0) {}

BigInt::BigInt(uint64_t value) {
    limbs_.push_back(static_cast<uint32_t>(value));
    limbs_.push_back(static_cast<uint32_t>(value >> 32));
    normalize();
}

BigInt::BigInt(std::vector<uint32_t> limbs) : limbs_(std::move(limbs)) {
    if (limbs_.empty()) limbs_.push_back(0);
    normalize();
}

BigInt::BigInt(const std::string& hex) {
    if (hex.empty()) throw std::invalid_argument("empty hex string");
    std::string s = hex;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s = s.substr(2);
    }
    if (s.empty()) throw std::invalid_argument("empty hex string");

    const size_t words = (s.size() + 7) / 8;
    limbs_.assign(words, 0);
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[s.size() - 1 - i];
        const uint32_t digit = hex_value(c);  // validates
        limbs_[i / 8] |= digit << (4 * (i % 8));
    }
    normalize();
}

BigInt BigInt::from_hex(const std::string& hex) { return BigInt(hex); }
BigInt BigInt::from_u64(uint64_t value) { return BigInt(value); }

BigInt BigInt::from_bytes_be(const std::vector<uint8_t>& bytes) {
    const size_t words = (bytes.size() + 3) / 4;
    std::vector<uint32_t> limbs(words == 0 ? 1 : words, 0);
    for (size_t i = 0; i < bytes.size(); ++i) {
        const size_t pos = bytes.size() - 1 - i;  // little-endian byte index
        limbs[pos / 4] |= static_cast<uint32_t>(bytes[i]) << (8 * (pos % 4));
    }
    return BigInt(std::move(limbs));
}

void BigInt::normalize() {
    while (limbs_.size() > 1 && limbs_.back() == 0) limbs_.pop_back();
    if (limbs_.empty()) limbs_.push_back(0);
}

// --- conversion -------------------------------------------------------------

std::string BigInt::to_hex() const {
    if (is_zero()) return "0";
    std::string out;
    bool leading = true;
    for (size_t i = limbs_.size(); i-- > 0;) {
        for (int j = 7; j >= 0; --j) {
            const uint32_t nibble = (limbs_[i] >> (4 * j)) & 0xF;
            if (nibble != 0 || !leading) {
                out += hex_char(nibble);
                leading = false;
            }
        }
    }
    return out;
}

std::string BigInt::to_dec() const {
    if (is_zero()) return "0";
    BigInt t = *this;
    std::string out;
    while (!t.is_zero()) {
        uint64_t rem = 0;
        for (size_t i = t.limbs_.size(); i-- > 0;) {
            const uint64_t cur = (rem << 32) | t.limbs_[i];
            t.limbs_[i] = static_cast<uint32_t>(cur / 10);
            rem = cur % 10;
        }
        out.push_back(static_cast<char>('0' + rem));
        t.normalize();
    }
    std::reverse(out.begin(), out.end());
    return out;
}

uint64_t BigInt::to_u64() const {
    if (limbs_.size() > 2) throw std::overflow_error("value too large for uint64");
    uint64_t r = limbs_[0];
    if (limbs_.size() == 2) r |= static_cast<uint64_t>(limbs_[1]) << 32;
    return r;
}

std::vector<uint8_t> BigInt::to_bytes_be(size_t length) const {
    const size_t need = (bit_length() + 7) / 8;
    const size_t n = length == 0 ? (need == 0 ? 1 : need) : length;
    if (length != 0 && need > length) throw std::overflow_error("value does not fit in length");
    std::vector<uint8_t> out(n, 0);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t limb = (i / 4) < limbs_.size() ? limbs_[i / 4] : 0;
        out[n - 1 - i] = static_cast<uint8_t>(limb >> (8 * (i % 4)));
    }
    return out;
}

// --- comparison -------------------------------------------------------------

int BigInt::compare(const BigInt& o) const {
    if (limbs_.size() != o.limbs_.size()) return limbs_.size() < o.limbs_.size() ? -1 : 1;
    for (size_t i = limbs_.size(); i-- > 0;) {
        if (limbs_[i] != o.limbs_[i]) return limbs_[i] < o.limbs_[i] ? -1 : 1;
    }
    return 0;
}

// --- arithmetic -------------------------------------------------------------

BigInt BigInt::add(const BigInt& o) const {
    const size_t n = std::max(limbs_.size(), o.limbs_.size()) + 1;
    std::vector<uint32_t> r(n, 0);
    uint64_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        const uint64_t a = i < limbs_.size() ? limbs_[i] : 0;
        const uint64_t b = i < o.limbs_.size() ? o.limbs_[i] : 0;
        const uint64_t s = a + b + carry;
        r[i] = static_cast<uint32_t>(s);
        carry = s >> 32;
    }
    return BigInt(std::move(r));
}

BigInt BigInt::sub(const BigInt& o) const {
    if (compare(o) < 0) throw std::invalid_argument("BigInt::sub would be negative");
    std::vector<uint32_t> r(limbs_.size(), 0);
    uint64_t borrow = 0;
    for (size_t i = 0; i < limbs_.size(); ++i) {
        const uint64_t a = limbs_[i];
        const uint64_t b = i < o.limbs_.size() ? o.limbs_[i] : 0;
        const uint64_t d = a - b - borrow;
        r[i] = static_cast<uint32_t>(d);
        borrow = (d >> 32) & 1u;
    }
    return BigInt(std::move(r));
}

BigInt BigInt::mul(const BigInt& o) const {
    if (is_zero() || o.is_zero()) return BigInt();
    std::vector<uint32_t> r(limbs_.size() + o.limbs_.size(), 0);
    for (size_t i = 0; i < limbs_.size(); ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < o.limbs_.size(); ++j) {
            const uint64_t p =
                static_cast<uint64_t>(limbs_[i]) * o.limbs_[j] + r[i + j] + carry;
            r[i + j] = static_cast<uint32_t>(p);
            carry = p >> 32;
        }
        r[i + o.limbs_.size()] += static_cast<uint32_t>(carry);
    }
    return BigInt(std::move(r));
}

void BigInt::divmod(const BigInt& a, const BigInt& d, BigInt& q, BigInt& r) {
    if (d.is_zero()) throw std::invalid_argument("division by zero");
    if (a.compare(d) < 0) {
        q = BigInt();
        r = a;
        return;
    }
    // Bitwise long division (MSB->LSB). Correct and simple; the performance
    // critical modular path lives in Montgomery code and avoids division.
    BigInt quotient;
    BigInt rem;
    const BigInt one(1u);
    for (size_t i = a.bit_length(); i-- > 0;) {
        rem = rem.shl(1);
        if (a.test_bit(i)) rem = rem.add(one);
        quotient = quotient.shl(1);
        if (rem.compare(d) >= 0) {
            rem = rem.sub(d);
            quotient = quotient.add(one);
        }
    }
    q = std::move(quotient);
    r = std::move(rem);
}

BigInt BigInt::mod(const BigInt& m) const {
    BigInt q, r;
    divmod(*this, m, q, r);
    return r;
}

BigInt BigInt::div(const BigInt& d) const {
    BigInt q, r;
    divmod(*this, d, q, r);
    return q;
}

// --- modular helpers --------------------------------------------------------

BigInt BigInt::gcd(const BigInt& a, const BigInt& b) {
    BigInt x = a, y = b;
    while (!y.is_zero()) {
        BigInt r = x.mod(y);
        x = std::move(y);
        y = std::move(r);
    }
    return x;
}

namespace {

// Minimal signed big integer, used only by the extended Euclidean algorithm so
// that BigInt itself can stay unsigned.
struct SBig {
    bool neg = false;
    BigInt mag;

    static SBig of(BigInt v) { return {false, std::move(v)}; }
    bool is_zero() const { return mag.is_zero(); }
};

SBig sadd(const SBig& a, const SBig& b);

SBig ssub(const SBig& a, const SBig& b) {
    SBig nb = b;
    if (!nb.is_zero()) nb.neg = !nb.neg;
    return sadd(a, nb);
}

SBig sadd(const SBig& a, const SBig& b) {
    if (a.is_zero()) return b;
    if (b.is_zero()) return a;
    if (a.neg == b.neg) return {a.neg, a.mag.add(b.mag)};
    const int cmp = a.mag.compare(b.mag);
    if (cmp == 0) return SBig{};  // a + (-a) = 0
    if (cmp > 0) return {a.neg, a.mag.sub(b.mag)};
    return {b.neg, b.mag.sub(a.mag)};
}

SBig smul(const SBig& a, const BigInt& b) {
    if (a.is_zero() || b.is_zero()) return SBig{};
    return {a.neg, a.mag.mul(b)};
}

}  // namespace

BigInt BigInt::mod_inverse(const BigInt& a, const BigInt& m) {
    if (m.is_zero() || (m.word_count() == 1 && m.data()[0] == 1)) {
        throw std::invalid_argument("modulus must be > 1");
    }
    // Extended Euclidean algorithm tracking the Bezout coefficient of `a`.
    BigInt old_r = a.mod(m), r = m;
    SBig old_s = SBig::of(BigInt(1u)), s = SBig{};  // 1, 0
    while (!r.is_zero()) {
        BigInt q, rem;
        divmod(old_r, r, q, rem);
        old_r = r;
        r = std::move(rem);
        SBig next_s = ssub(old_s, smul(s, q));
        old_s = s;
        s = std::move(next_s);
    }
    if (!(old_r.word_count() == 1 && old_r.data()[0] == 1)) {
        throw std::invalid_argument("no modular inverse: gcd(a, m) != 1");
    }
    // old_s is the coefficient; reduce into [0, m).
    BigInt res = old_s.mag.mod(m);
    if (old_s.neg && !res.is_zero()) res = m.sub(res);
    return res;
}

// --- bit manipulation -------------------------------------------------------

BigInt BigInt::shl(size_t bits) const {
    if (is_zero() || bits == 0) return *this;
    const size_t word_shift = bits / 32;
    const size_t bit_shift = bits % 32;
    std::vector<uint32_t> r(limbs_.size() + word_shift + 1, 0);
    if (bit_shift == 0) {
        for (size_t i = 0; i < limbs_.size(); ++i) r[i + word_shift] = limbs_[i];
    } else {
        uint32_t carry = 0;
        for (size_t i = 0; i < limbs_.size(); ++i) {
            const uint64_t v = (static_cast<uint64_t>(limbs_[i]) << bit_shift) | carry;
            r[i + word_shift] = static_cast<uint32_t>(v);
            carry = static_cast<uint32_t>(v >> 32);
        }
        r[limbs_.size() + word_shift] = carry;
    }
    return BigInt(std::move(r));
}

BigInt BigInt::shr(size_t bits) const {
    if (is_zero() || bits == 0) return *this;
    const size_t word_shift = bits / 32;
    const size_t bit_shift = bits % 32;
    if (word_shift >= limbs_.size()) return BigInt();
    std::vector<uint32_t> r(limbs_.size() - word_shift, 0);
    if (bit_shift == 0) {
        for (size_t i = word_shift; i < limbs_.size(); ++i) r[i - word_shift] = limbs_[i];
    } else {
        // Process limbs high->low, carrying the bits that fall off the bottom
        // of limb i into the top of result limb (i-1).
        uint32_t carry = 0;
        for (size_t i = limbs_.size(); i-- > word_shift;) {
            const uint32_t cur = limbs_[i];
            r[i - word_shift] = (cur >> bit_shift) | (carry << (32 - bit_shift));
            carry = cur & ((1u << bit_shift) - 1u);
        }
    }
    return BigInt(std::move(r));
}

bool BigInt::test_bit(size_t i) const {
    const size_t w = i / 32;
    if (w >= limbs_.size()) return false;
    return (limbs_[w] >> (i % 32)) & 1u;
}

// --- introspection ----------------------------------------------------------

bool BigInt::is_zero() const { return limbs_.size() == 1 && limbs_[0] == 0; }

size_t BigInt::bit_length() const {
    if (is_zero()) return 0;
    size_t bits = (limbs_.size() - 1) * 32;
    uint32_t top = limbs_.back();
    while (top) {
        ++bits;
        top >>= 1;
    }
    return bits;
}

std::vector<uint32_t> BigInt::limbs_padded(size_t words) const {
    std::vector<uint32_t> r(words, 0);
    const size_t n = std::min(words, limbs_.size());
    for (size_t i = 0; i < n; ++i) r[i] = limbs_[i];
    return r;
}

}  // namespace montx
