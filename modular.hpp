#pragma once
#ifndef MODULAR_HPP
#define MODULAR_HPP

#include "bigint.hpp"
#include <vector>

class MontgomeryCtx {
private:
    BigInt mod_;
    BigInt R_;
    BigInt R_inv_;
    uint32_t n_prime_;
    size_t n_words_;

    void computeParameters();
    uint32_t computeNPrime() const;
    BigInt computeR() const;
    BigInt computeRInv() const;
    BigInt montgomeryReduce(const BigInt& t) const;

public:
    explicit MontgomeryCtx(const BigInt& mod);
    ~MontgomeryCtx() = default;

    // Конвертация представлений
    BigInt toMontgomery(const BigInt& a) const;
    BigInt fromMontgomery(const BigInt& a) const;

    // Операции в представлении Монтгомери
    BigInt multiply(const BigInt& a, const BigInt& b) const;
    BigInt exponentiate(const BigInt& base, const BigInt& exp) const;

    // Геттеры
    const BigInt& modulus() const { return mod_; }
    const BigInt& R() const { return R_; }
    const BigInt& R_inv() const { return R_inv_; }
    uint32_t n_prime() const { return n_prime_; }
    size_t n_words() const { return n_words_; }
};

// Функции модульной арифметики
BigInt mod_exp(const BigInt& base, const BigInt& exp, const BigInt& mod);
BigInt mod_exp_montgomery(const BigInt& base, const BigInt& exp, const BigInt& mod);

#endif // MODULAR_HPP

