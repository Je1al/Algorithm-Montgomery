#include "modular.hpp"
#include <algorithm>
#include <stdexcept>

namespace {
    // Функция для вычисления обратного по модулю 2^32 методом Ньютона
    uint32_t modInverse32(uint32_t a) {
        // Монтгомери требует нечетный модуль
        if ((a & 1) == 0) {
            throw std::runtime_error("Modulus must be odd for Montgomery arithmetic");
        }

        // Метод Ньютона для нахождения обратного по модулю 2^32
        uint32_t inv = a;
        for (int i = 0; i < 5; i++) {
            inv = inv * (2 - a * inv);
        }

        return inv;
    }
}

MontgomeryCtx::MontgomeryCtx(const BigInt& mod) : mod_(mod) {
    computeParameters();
}

void MontgomeryCtx::computeParameters() {
    n_words_ = mod_.wordCount();

    // R = 2^(32 * n_words)
    R_ = computeR();

    // n' = -N^(-1) mod 2^32
    n_prime_ = computeNPrime();

    // R_inv = R^(-1) mod N (вычисляем через R^(N-2) mod N, если N простое)
    R_inv_ = computeRInv();
}

uint32_t MontgomeryCtx::computeNPrime() const {
    uint32_t n0 = mod_.data()[0];
    uint32_t inv = modInverse32(n0);

    // Исправление: корректное вычисление -N^(-1) mod 2^32
    // Вместо применения унарного минуса к беззнаковому типу,
    // используем формулу 2^32 - inv
    const uint64_t BASE = 1ULL << 32;
    uint64_t result = BASE - static_cast<uint64_t>(inv);

    return static_cast<uint32_t>(result);
}

BigInt MontgomeryCtx::computeR() const {
    // R = 2^(32 * n_words)
    std::vector<uint32_t> r_digits(n_words_ + 1, 0);
    r_digits[n_words_] = 1;
    return BigInt(r_digits);
}

BigInt MontgomeryCtx::computeRInv() const {
    // R_inv = R^(-1) mod N
    // Используем расширенный алгоритм Евклида для больших чисел
    // Упрощенная реализация: R_inv = R^(phi(N)-1) mod N
    // Для простого N: phi(N) = N-1, поэтому R_inv = R^(N-2) mod N

    if (mod_.isZero()) {
        throw std::invalid_argument("Modulus cannot be zero");
    }

    // Вычисляем R^(N-2) mod N
    BigInt exponent = mod_.subtract(BigInt(2));
    BigInt result = BigInt(1);
    BigInt base = R_;

    // Быстрое возведение в степень
    while (!exponent.isZero()) {
        if (exponent.data()[0] & 1) {
            result = result.multiply(base).mod(mod_);
        }
        base = base.multiply(base).mod(mod_);
        exponent = exponent.shiftRight(1);
    }

    return result;
}

BigInt MontgomeryCtx::toMontgomery(const BigInt& a) const {
    // a_bar = a * R mod N
    BigInt aR = a.multiply(R_);
    return aR.mod(mod_);
}

BigInt MontgomeryCtx::fromMontgomery(const BigInt& a_bar) const {
    // a = a_bar * R_inv mod N
    BigInt result = a_bar.multiply(R_inv_);
    return result.mod(mod_);
}

BigInt MontgomeryCtx::montgomeryReduce(const BigInt& t) const {
    // Упрощенная редукция Монтгомери
    if (t.wordCount() == 0) {
        return BigInt();
    }

    uint32_t m = t.data()[0] * n_prime_;

    // Вычисляем m * N
    BigInt m_big = BigInt(std::vector<uint32_t>{m});
    BigInt mN = m_big.multiply(mod_);

    // Вычисляем t + mN
    BigInt sum = t.add(mN);

    // Делим на R (сдвиг вправо на 32 бита)
    BigInt u = sum.shiftRight(32);

    // Если u >= N, вычитаем N
    if (u.compare(mod_) >= 0) {
        u = u.subtract(mod_);
    }

    return u;
}

BigInt MontgomeryCtx::multiply(const BigInt& a, const BigInt& b) const {
    // Умножаем a и b, затем делаем редукцию Монтгомери
    BigInt product = a.multiply(b);
    return montgomeryReduce(product);
}

BigInt MontgomeryCtx::exponentiate(const BigInt& base, const BigInt& exp) const {
    // Конвертируем основание в представление Монтгомери
    BigInt base_mont = toMontgomery(base);

    // 1 в представлении Монтгомери
    BigInt one_mont = toMontgomery(BigInt(1));

    BigInt result = one_mont;
    BigInt current = base_mont;
    BigInt e = exp;

    // Алгоритм square-and-multiply
    while (!e.isZero()) {
        if (e.data()[0] & 1) {
            result = multiply(result, current);
        }

        current = multiply(current, current);
        e = e.shiftRight(1);
    }

    // Конвертируем результат обратно
    return fromMontgomery(result);
}

BigInt mod_exp(const BigInt& base, const BigInt& exp, const BigInt& mod) {
    if (mod.isZero()) {
        throw std::invalid_argument("Modulus cannot be zero");
    }

    if (exp.isZero()) {
        return BigInt(1).mod(mod);
    }

    BigInt result = BigInt(1);
    BigInt current = base.mod(mod);
    BigInt e = exp;

    // Алгоритм square-and-multiply
    while (!e.isZero()) {
        if (e.data()[0] & 1) {
            result = result.multiply(current).mod(mod);
        }

        current = current.multiply(current).mod(mod);
        e = e.shiftRight(1);
    }

    return result;
}

BigInt mod_exp_montgomery(const BigInt& base, const BigInt& exp, const BigInt& mod) {
    MontgomeryCtx ctx(mod);
    return ctx.exponentiate(base, exp);
}
