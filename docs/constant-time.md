# Constant-time implementation notes

A modular exponentiation can be mathematically perfect and still hand an attacker
the private key, if *how long it runs* or *which memory it touches* depends on the
secret. This document records what is hardened in this library, how, and — just
as importantly — what is **not**.

## The threat: the secret steers the control flow

Textbook left-to-right exponentiation:

```
for each bit of e (MSB→LSB):
    x = x²            (square — always)
    if bit == 1:
        x = x · base  (multiply — only when the bit is set)
```

The multiply happens **iff** the exponent bit is 1. An adversary who can tell a
"square" apart from a "square+multiply" — via timing, power draw, EM emanation or
cache state — reads the exponent straight off, bit for bit. `lab/timing_attack.cpp`
does exactly this and recovers a full RSA private exponent from a single trace.

## The defence 1: the Montgomery powering ladder

`Montgomery::pow_ct` runs the ladder (Joye–Yen / "Montgomery powering ladder"):

```
R0 = 1 ; R1 = base
for each bit of e (MSB→LSB):
    cswap(R0, R1, bit)        # constant-time conditional swap
    R1 = R0 · R1
    R0 = R0 · R0
    cswap(R0, R1, bit)
```

Every iteration performs **exactly one multiply and one square**, no matter the
bit. The only data-dependent action is the conditional swap, done branchlessly
(see below). The operation sequence is therefore identical for all exponents of a
given length — `tests/test_constant_time.cpp` asserts this by counting MonPro
calls for sparse, dense and mid-weight exponents and checking they are equal.

To stop the *number of iterations* from leaking `⌊log2 e⌋`, pass
`fixed_bits = modulus.bit_length()`; the ladder then always runs the same number
of steps for any secret exponent below the modulus (which is the RSA case).

## The defence 2: branchless field arithmetic

The building blocks live in [`include/montx/ct.hpp`](../include/montx/ct.hpp) and
compute with masks instead of branches:

- `mask_from_bit(b)` → `0xFFFFFFFF` or `0x00000000`
- `select(mask, a, b)` → `a` or `b` with no branch
- `cswap(mask, a, b)` → conditional swap by XOR
- `geq` / `equal` → compare fixed-width limb arrays in constant time

The final conditional subtraction inside REDC (`monpro`) uses `select` over a
*fixed-width* limb array: the subtraction is **always computed**, and a mask
chooses whether to keep it. A naive `if (t >= N) t -= N;` would instead run the
subtraction only sometimes — the "extra reduction" leak of Schindler (2000) and
Walter (2001). Experiment 3 in the lab exposes the data-dependent count.

## Why fixed-width limbs

The general `BigInt` is variable length (it strips leading zero limbs), and its
length is itself a side channel. So the secret-key hot path never uses `BigInt`
loop bounds: `monpro` and the ladder operate on arrays of exactly
`modulus.word_count()` limbs, touching every limb on every call.

## Honest scope — what is NOT covered

This is a software, source-level constant-time effort. It is **not** a guarantee
against a real-world side-channel adversary, because:

- **Compilers and CPUs may reintroduce branches.** `select`/`cswap` are written
  branchlessly, but an aggressive optimizer can undo that, and microarchitectural
  effects (branch predictors, variable-latency multipliers on some cores) are out
  of a portable C++ file's control. Production code should verify the emitted
  assembly and test statistically (e.g. `dudect`, `ctgrind`).
- **No hardware countermeasures.** Real targets add base/exponent blinding,
  message randomisation, and fault-attack detection. CRT-RSA here has no
  Bellcore-fault countermeasure — see `docs/threat-model-automotive.md`.
- **The RNG is pluggable, not audited.** Key material should come from a vetted
  CSPRNG, not `std::random_device`.

The point of the library is to demonstrate the *principles* correctly and to make
the leak/no-leak difference measurable. See the threat model for where this sits
in a real automotive security program.
