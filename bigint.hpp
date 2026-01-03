#pragma once
#ifndef BIGINT_HPP
#define BIGINT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>

class BigInt {
private:
    std::vector<uint32_t> digits_;

    void normalize();

public:
    // Конструкторы
    BigInt();
    explicit BigInt(const std::string& hexstr);
    explicit BigInt(uint64_t value);
    explicit BigInt(const std::vector<uint32_t>& digits);

    // Правило пяти
    BigInt(const BigInt& other) = default;
    BigInt(BigInt&& other) noexcept = default;
    BigInt& operator=(const BigInt& other) = default;
    BigInt& operator=(BigInt&& other) noexcept = default;
    ~BigInt() = default;

    // Преобразование в строки
    std::string toHex() const;
    std::string toDec() const;
    uint64_t toU64() const;

    // Сравнения
    int compare(const BigInt& other) const;
    bool operator==(const BigInt& other) const { return compare(other) == 0; }
    bool operator!=(const BigInt& other) const { return compare(other) != 0; }
    bool operator<(const BigInt& other) const { return compare(other) < 0; }
    bool operator>(const BigInt& other) const { return compare(other) > 0; }
    bool operator<=(const BigInt& other) const { return compare(other) <= 0; }
    bool operator>=(const BigInt& other) const { return compare(other) >= 0; }

    // Арифметические операции
    BigInt add(const BigInt& other) const;
    BigInt subtract(const BigInt& other) const;
    BigInt multiply(const BigInt& other) const;
    BigInt mod(const BigInt& other) const;

    // Операторы
    BigInt operator+(const BigInt& other) const { return add(other); }
    BigInt operator-(const BigInt& other) const { return subtract(other); }
    BigInt operator*(const BigInt& other) const { return multiply(other); }
    BigInt operator%(const BigInt& other) const { return mod(other); }

    // Битовые операции
    BigInt shiftLeft(size_t bits) const;
    BigInt shiftRight(size_t bits) const;

    // Утилиты
    bool isZero() const;
    size_t bitLength() const;
    size_t wordCount() const;
    const uint32_t* data() const { return digits_.data(); }
    size_t size() const { return digits_.size(); }

    // Статические методы
    static BigInt fromHex(const std::string& hex);
    static BigInt fromU64(uint64_t value);
};

#endif // BIGINT_HPP
