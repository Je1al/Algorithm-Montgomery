# BigInt: Arbitrary-Precision Arithmetic & Montgomery Library

This repository contains a C++ implementation of a **BigInt** library for arbitrary-precision integer arithmetic.  
The project focuses on modular exponentiation and includes an optimized implementation of **Montgomery reduction**, which is widely used in cryptographic algorithms.

The library is intended primarily for **educational and experimental purposes**, with an emphasis on clarity and understanding of big-number arithmetic.

---

## Overview

The library allows manipulation of integers of virtually unlimited size (restricted only by system memory).  
Internally, numbers are represented as a vector of 32-bit words, providing flexibility and control over low-level arithmetic operations.

---

## Key Features

- **Arbitrary Precision**
  - Internal storage based on `std::vector<uint32_t>`
  - Supports very large integers

- **Arithmetic Operations**
  - Addition
  - Subtraction
  - Multiplication
  - Modulo

- **Modular Exponentiation**
  - **Standard mode**: classic modular exponentiation
  - **Montgomery mode**: high-performance exponentiation using Montgomery reduction (for odd moduli)

- **Bitwise Manipulation**
  - Efficient left and right bit shifts

- **Format Conversion**
  - Conversion to and from hexadecimal and decimal string representations

- **Interactive CLI**
  - Built-in calculator for testing all library features in real time

---

## Montgomery Reduction Algorithm

One of the core features of this project is **Montgomery multiplication**, which speeds up computations of  
`b^e mod m` by avoiding costly division operations.

### Algorithm outline

1. **Preparation**
   - Transform values into the Montgomery domain using a helper constant `R = 2^k`

2. **REDC (Reduction)**
   - Computes `T · R⁻¹ mod m` using only additions and bit shifts

3. **Modular Exponentiation**
   - Performs square-and-multiply entirely inside the Montgomery domain

4. **Final Conversion**
   - Converts the result back to the standard integer representation

---

## Getting Started

### Prerequisites

- C++ compatible compiler (GCC, Clang, or MSVC)

### Compilation

Compile the library together with the interactive calculator:

```bash
g++ -O3 main.cpp bigint.cpp -o bigint_calc
