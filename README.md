# MontX — Constant-Time Montgomery Arithmetic Toolkit

[![ci](https://github.com/Je1al/Algorithm-Montgomery/actions/workflows/ci.yml/badge.svg)](https://github.com/Je1al/Algorithm-Montgomery/actions/workflows/ci.yml)
[![license: MIT](https://img.shields.io/badge/license-MIT-black.svg)](LICENSE)
![C++17](https://img.shields.io/badge/C%2B%2B-17-black.svg)
![no dependencies](https://img.shields.io/badge/dependencies-none-black.svg)

A from-scratch C++17 big-integer and **Montgomery modular arithmetic** library,
built around the one thing that separates a textbook crypto routine from a usable
one: the secret-key path is **constant-time and side-channel aware**. It ships
with a reproducible **side-channel laboratory** that recovers an entire RSA
private key from a leaky exponentiation — and shows the constant-time ladder
shutting that channel down.

No external dependencies. No `bignum` library. Everything — schoolbook
arithmetic, CIOS Montgomery multiplication, the powering ladder, Miller–Rabin,
RSA — is implemented and tested here.

> Why this exists: modular exponentiation is the engine under RSA, Diffie–Hellman
> and ECDSA, and those run on devices an attacker can hold in their hand (smart
> cards, automotive ECUs, HSMs). Getting the *math* right is the easy half;
> getting it to run in time that does not depend on the key is the half that
> actually keeps the key secret. See [docs/threat-model-automotive.md](docs/threat-model-automotive.md).

---

## Highlights

- **Correct CIOS Montgomery multiplication** (Koç–Acar–Kaliski) and REDC, with a
  division-free setup path — see [docs/montgomery-reduction.md](docs/montgomery-reduction.md).
- **Constant-time modular exponentiation** via the Montgomery powering ladder,
  built on branchless `select`/`cswap` primitives —
  see [docs/constant-time.md](docs/constant-time.md).
- **Three independent exponentiation implementations** (schoolbook reference,
  fast Montgomery, constant-time ladder) that cross-check each other.
- **RSA on top of it all**: Miller–Rabin prime generation, key gen, textbook
  encrypt/decrypt/sign/verify, CRT decryption — a real workload for the arithmetic.
- **A side-channel lab** that *demonstrates the attack and the defence*, not just
  claims them.
- **Engineering**: 88 unit-test checks, differential fuzzing (libFuzzer), ASan +
  UBSan clean, GitHub Actions CI on GCC and Clang. A fuzzer-found correctness bug
  is documented below.

## At a glance

```
include/montx/   ct.hpp  bigint.hpp  montgomery.hpp  modexp.hpp  primes.hpp  rsa.hpp  rng.hpp
src/             the implementations
apps/            montx_cli.cpp          command-line tool
tests/           5 suites, dependency-free micro-framework
bench/           bench_modexp.cpp       Montgomery vs schoolbook, RSA timings
fuzz/            fuzz_montmul.cpp        differential fuzz target + corpus
lab/             timing_attack.cpp      the side-channel demonstration
docs/            the math, constant-time notes, automotive threat model
```

## Build & run

```bash
# CMake (recommended)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# or plain Make (no CMake required)
make test      # build & run all unit tests
make all       # cli + benchmark + lab
make asan      # tests under AddressSanitizer + UBSan
make help      # list every target
```

Requirements: any C++17 compiler (GCC or Clang). Nothing else.

## Command-line tool

```bash
$ ./build/montx modexp 23b8c1e9392456de eb13b90466 bdd640fb06671ad1 --mode ct
mode = ct
hex  = 5b3ce461d0b3c35c
dec  = 6574380664818484060

$ ./build/montx genprime 512          # random 512-bit probable prime
$ ./build/montx rsa 2048              # generate a key and run an encrypt/decrypt demo
$ ./build/montx selftest             # cross-check the three modexp paths
```

All numbers are hexadecimal. `--mode` is `standard` (schoolbook), `montgomery`
(fast) or `ct` (constant-time ladder).

## The side-channel lab

`./build/timing_attack` runs three experiments against this library's own two
exponentiation routines. Abridged output:

```
[1] SPA trace attack: recover the private exponent from ONE trace
  modulus size         : 256 bits
  secret exponent  d   : 8abfce83840c2e769e0511cb...
  recovered from trace : 8abfce83840c2e769e0511cb...
  >> RESULT: leaky impl leaks ALL of 256 key bits  [KEY FULLY RECOVERED]

[2] Timing channel: does run time leak the exponent's Hamming weight?
  A/B at 512 bits (sparse w=32  vs  dense w=496):
    leaky : sparse 298.9 us | dense 552.5 us | gap +253.6 us  <- secret-dependent
    ladder: sparse 563.8 us | dense 562.3 us | gap   -1.5 us  <- flat
  correlation over 150 random exponents:
    corr(HammingWeight, time) leaky :  0.636
    corr(HammingWeight, time) ladder:  0.075

[3] Extra-reduction leak in REDC (Schindler/Walter)
  ...the data-dependent final subtraction, neutralised by a branchless select.
```

The leaky `square-and-multiply` leaks every key bit through its operation
pattern, and its run time tracks the secret's Hamming weight. The
`Montgomery ladder` performs the identical sequence of field operations every
iteration, so both channels go flat. This is a software model of a Simple
Power/EM Analysis adversary; the same control-flow asymmetry is what shows up as
power and EM on real hardware.

## Benchmarks

`./build/bench_modexp` — modular exponentiation, schoolbook (bitwise long
division) vs Montgomery vs the constant-time ladder. Apple clang `-O3`, single
core; numbers are illustrative, the *ratios* are the point.

| modulus | schoolbook | Montgomery | ladder (CT) | speedup |
|--------:|-----------:|-----------:|------------:|--------:|
| 256-bit | 23.19 ms | 0.067 ms | 0.087 ms | **344×** |
| 512-bit | 94.57 ms | 0.448 ms | 0.629 ms | **211×** |
| 1024-bit | 447.3 ms | 3.27 ms | 4.30 ms | **137×** |
| 2048-bit | 2262 ms | 26.4 ms | 35.3 ms | **86×** |

| RSA | keygen | decrypt (CT) | decrypt (CRT) |
|----:|-------:|-------------:|--------------:|
| 1024-bit | 0.96 s | 4.71 ms | **1.98 ms** |
| 2048-bit | 3.45 s | 36.4 ms | **10.5 ms** |

The constant-time ladder costs ~30% over the fast path (it always does the
worst-case work) — the price of not leaking. CRT roughly halves RSA decryption.

## Correctness

```
test_bigint         28 checks   hex/dec, add/sub/mul, divmod, gcd, modular inverse, shifts, bytes
test_montgomery     19 checks   KATs at 128/256/512/1024-bit, domain round-trips, 420-case
                                differential fuzz: schoolbook == fast == constant-time
test_primes         19 checks   Miller-Rabin (incl. composites with no small factors),
                                generated primes pass Fermat's little theorem
test_rsa             6 checks   encrypt/decrypt, sign/verify, CRT == plain decryption
test_constant_time  16 checks   ct primitives + proof that the ladder's op-count is
                                independent of the secret while square-and-multiply leaks it
```

- **Known-answer tests** are cross-checked against CPython's `pow()`.
- **Differential testing**: three independent exponentiation implementations must
  agree on hundreds of random inputs across seven bit-widths.
- **Fuzzing**: `fuzz/fuzz_montmul.cpp` derives `(modulus, a, b)` from arbitrary
  bytes and asserts the Montgomery path equals the schoolbook reference. Build it
  with libFuzzer, or replay the corpus with any compiler (`make fuzz-replay`).
- **Sanitizers**: the suite runs clean under ASan + UBSan (`make asan`).
- **CI** ([.github/workflows/ci.yml](.github/workflows/ci.yml)): build + test on
  GCC and Clang, a sanitizer job, a 60-second libFuzzer smoke run, and cppcheck.

> **Found by the fuzzer:** `pow_ct(..., fixed_bits)` originally iterated over
> exactly `fixed_bits` exponent bits, silently dropping the high bits when the
> exponent was *longer* than the window. RSA never hits this (`d < n`), so the unit
> tests passed — the differential fuzzer caught it in seconds. The fix clamps the
> window to `max(fixed_bits, exp.bit_length())`. This is the whole reason to fuzz.

## Security scope — read this

This is a **portfolio / educational** implementation that is correct and
constant-time *at the source level*. It is **not** a hardened production library:

- The RSA primitives are **textbook** (no OAEP/PSS padding) — they demonstrate the
  number theory and the constant-time private-key path, nothing more.
- Source-level constant-time is not a hardware guarantee: a compiler or CPU can
  reintroduce data-dependent behaviour. Production needs assembly verification and
  statistical tests (`dudect`/`ctgrind`), plus blinding and fault-attack
  countermeasures.
- The key-material RNG is **pluggable**; the default uses `std::random_device`,
  not an audited CSPRNG.

These limits are spelled out in [docs/constant-time.md](docs/constant-time.md) and
[docs/threat-model-automotive.md](docs/threat-model-automotive.md). The goal is to
show the principles correctly and make the leak/no-leak difference *measurable*.

## References

- P. L. Montgomery, *Modular Multiplication Without Trial Division*, Math. Comp. 1985.
- Koç, Acar, Kaliski, *Analyzing and Comparing Montgomery Multiplication Algorithms*, IEEE Micro 1996.
- Kocher, *Timing Attacks on Implementations of Diffie-Hellman, RSA, DSS…*, CRYPTO 1996.
- Schindler (2000) & Walter (2001), the Montgomery "extra reduction" timing leak.
- Menezes, van Oorschot, Vanstone, *Handbook of Applied Cryptography*, ch. 14.

## License

MIT — see [LICENSE](LICENSE).
