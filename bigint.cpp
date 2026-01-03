#include "bigint.hpp"
#include <algorithm>
#include <cctype>
#include <random>

namespace {
    bool isValidHex(const std::string& str) {
        if (str.empty()) return false;
        for (char c : str) {
            if (!std::isxdigit(c)) return false;
        }
        return true;
    }

    uint32_t hexCharToValue(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return 0;
    }

    char valueToHexChar(uint32_t v) {
        if (v < 10) return '0' + v;
        return 'A' + (v - 10);
    }

    uint32_t addWithCarry(uint32_t a, uint32_t b, uint32_t& carry) {
        uint64_t sum = static_cast<uint64_t>(a) + b + carry;
        carry = static_cast<uint32_t>(sum >> 32);
        return static_cast<uint32_t>(sum);
    }

    uint32_t mulWithCarry(uint32_t a, uint32_t b, uint32_t& carry) {
        uint64_t product = static_cast<uint64_t>(a) * b + carry;
        carry = static_cast<uint32_t>(product >> 32);
        return static_cast<uint32_t>(product);
    }

    uint32_t subWithBorrow(uint32_t a, uint32_t b, uint32_t& borrow) {
        uint64_t diff = static_cast<uint64_t>(a) - b - borrow;
        borrow = (diff >> 32) & 1;
        return static_cast<uint32_t>(diff);
    }
}

BigInt::BigInt() : digits_(1, 0) {}

BigInt::BigInt(const std::string& hexstr) {
    if (!isValidHex(hexstr)) {
        throw std::invalid_argument("Invalid hexadecimal string");
    }

    // Пропускаем ведущие нули
    size_t start = 0;
    while (start < hexstr.size() - 1 && hexstr[start] == '0') {
        start++;
    }

    std::string cleaned = hexstr.substr(start);
    size_t hex_len = cleaned.size();
    size_t word_count = (hex_len + 7) / 8;

    digits_.resize(word_count, 0);

    for (size_t i = 0; i < hex_len; i++) {
        size_t pos = hex_len - 1 - i;
        uint32_t digit = hexCharToValue(cleaned[pos]);
        digits_[i / 8] |= digit << (4 * (i % 8));
    }

    normalize();
}

BigInt::BigInt(uint64_t value) {
    digits_.push_back(static_cast<uint32_t>(value));
    if (value > UINT32_MAX) {
        digits_.push_back(static_cast<uint32_t>(value >> 32));
    }
    normalize();
}

BigInt::BigInt(const std::vector<uint32_t>& digits) : digits_(digits) {
    normalize();
}

void BigInt::normalize() {
    while (digits_.size() > 1 && digits_.back() == 0) {
        digits_.pop_back();
    }
    if (digits_.empty()) {
        digits_.push_back(0);
    }
}

std::string BigInt::toHex() const {
    if (isZero()) return "0";

    std::string result;
    bool leading = true;

    for (size_t i = digits_.size(); i-- > 0; ) {
        for (int j = 7; j >= 0; j--) {
            uint32_t nibble = (digits_[i] >> (4 * j)) & 0xF;
            if (nibble != 0 || !leading) {
                result += valueToHexChar(nibble);
                leading = false;
            }
        }
    }

    return result.empty() ? "0" : result;
}

std::string BigInt::toDec() const {
    if (isZero()) return "0";

    BigInt temp = *this;
    std::string result;

    while (!temp.isZero()) {
        // Делим на 10
        uint64_t remainder = 0;
        for (size_t i = temp.digits_.size(); i-- > 0; ) {
            uint64_t current = (static_cast<uint64_t>(remainder) << 32) | temp.digits_[i];
            temp.digits_[i] = static_cast<uint32_t>(current / 10);
            remainder = current % 10;
        }
        result = static_cast<char>('0' + remainder) + result;
        temp.normalize();
    }

    return result;
}

uint64_t BigInt::toU64() const {
    if (digits_.size() > 2) {
        throw std::overflow_error("Value too large for uint64_t");
    }

    uint64_t result = 0;
    if (digits_.size() >= 1) result |= digits_[0];
    if (digits_.size() >= 2) result |= static_cast<uint64_t>(digits_[1]) << 32;
    return result;
}

int BigInt::compare(const BigInt& other) const {
    if (digits_.size() != other.digits_.size()) {
        return digits_.size() < other.digits_.size() ? -1 : 1;
    }

    for (size_t i = digits_.size(); i-- > 0; ) {
        if (digits_[i] != other.digits_[i]) {
            return digits_[i] < other.digits_[i] ? -1 : 1;
        }
    }

    return 0;
}

BigInt BigInt::add(const BigInt& other) const {
    size_t max_size = std::max(digits_.size(), other.digits_.size()) + 1;
    std::vector<uint32_t> result(max_size, 0);

    uint32_t carry = 0;
    for (size_t i = 0; i < max_size; i++) {
        uint32_t a_val = i < digits_.size() ? digits_[i] : 0;
        uint32_t b_val = i < other.digits_.size() ? other.digits_[i] : 0;
        result[i] = addWithCarry(a_val, b_val, carry);
    }

    BigInt temp(result);
    temp.normalize();
    return temp;
}

BigInt BigInt::subtract(const BigInt& other) const {
    if (*this < other) {
        throw std::invalid_argument("Result would be negative");
    }

    size_t max_size = digits_.size();
    std::vector<uint32_t> result(max_size, 0);

    uint32_t borrow = 0;
    for (size_t i = 0; i < max_size; i++) {
        uint32_t a_val = digits_[i];
        uint32_t b_val = i < other.digits_.size() ? other.digits_[i] : 0;
        result[i] = subWithBorrow(a_val, b_val, borrow);
    }

    BigInt temp(result);
    temp.normalize();
    return temp;
}

BigInt BigInt::multiply(const BigInt& other) const {
    size_t result_size = digits_.size() + other.digits_.size();
    std::vector<uint32_t> result(result_size, 0);

    for (size_t i = 0; i < digits_.size(); i++) {
        uint32_t carry = 0;
        for (size_t j = 0; j < other.digits_.size(); j++) {
            uint64_t product = static_cast<uint64_t>(digits_[i]) * other.digits_[j]
                + result[i + j] + carry;
            result[i + j] = static_cast<uint32_t>(product);
            carry = static_cast<uint32_t>(product >> 32);
        }
        if (carry > 0) {
            result[i + other.digits_.size()] = carry;
        }
    }

    BigInt temp(result);
    temp.normalize();
    return temp;
}

BigInt BigInt::mod(const BigInt& other) const {
    if (other.isZero()) {
        throw std::invalid_argument("Division by zero");
    }

    if (*this < other) {
        return *this;
    }

    // Простая реализация через вычитание
    BigInt remainder = *this;
    BigInt divisor = other;

    // Находим степень 2, чтобы сдвинуть делитель
    size_t shift = 0;
    while (divisor < remainder) {
        divisor = divisor.shiftLeft(1);
        shift++;
    }

    while (shift > 0) {
        divisor = divisor.shiftRight(1);
        shift--;

        if (remainder >= divisor) {
            remainder = remainder.subtract(divisor);
        }
    }

    return remainder;
}

BigInt BigInt::shiftLeft(size_t bits) const {
    if (isZero()) return *this;

    size_t word_shift = bits / 32;
    size_t bit_shift = bits % 32;

    std::vector<uint32_t> result(digits_.size() + word_shift + 1, 0);

    if (bit_shift == 0) {
        for (size_t i = 0; i < digits_.size(); i++) {
            result[i + word_shift] = digits_[i];
        }
    }
    else {
        uint32_t carry = 0;
        for (size_t i = 0; i < digits_.size(); i++) {
            uint64_t val = (static_cast<uint64_t>(digits_[i]) << bit_shift) | carry;
            result[i + word_shift] = static_cast<uint32_t>(val);
            carry = static_cast<uint32_t>(val >> 32);
        }
        if (carry > 0) {
            result[digits_.size() + word_shift] = carry;
        }
    }

    BigInt temp(result);
    temp.normalize();
    return temp;
}

BigInt BigInt::shiftRight(size_t bits) const {
    if (isZero()) return *this;

    size_t word_shift = bits / 32;
    size_t bit_shift = bits % 32;

    if (word_shift >= digits_.size()) {
        return BigInt();
    }

    std::vector<uint32_t> result(digits_.size() - word_shift, 0);

    if (bit_shift == 0) {
        for (size_t i = word_shift; i < digits_.size(); i++) {
            result[i - word_shift] = digits_[i];
        }
    }
    else {
        uint32_t carry = 0;
        for (size_t i = digits_.size(); i-- > word_shift; ) {
            uint64_t val = (static_cast<uint64_t>(digits_[i]) << 32) | carry;
            result[i - word_shift] = static_cast<uint32_t>(val >> bit_shift);
            carry = static_cast<uint32_t>(val) & ((1 << bit_shift) - 1);
        }
    }

    BigInt temp(result);
    temp.normalize();
    return temp;
}

bool BigInt::isZero() const {
    return digits_.size() == 1 && digits_[0] == 0;
}

size_t BigInt::bitLength() const {
    if (isZero()) return 0;

    size_t word_idx = digits_.size() - 1;
    uint32_t top_word = digits_[word_idx];

    size_t bits = word_idx * 32;
    while (top_word > 0) {
        bits++;
        top_word >>= 1;
    }

    return bits;
}

size_t BigInt::wordCount() const {
    return digits_.size();
}

BigInt BigInt::fromHex(const std::string& hex) {
    return BigInt(hex);
}

BigInt BigInt::fromU64(uint64_t value) {
    return BigInt(value);
}
