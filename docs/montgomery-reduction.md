# Montgomery reduction — the math

Modular exponentiation `a^e mod N` is the workhorse of RSA, Diffie–Hellman and
DSA. Done naively, every multiplication is followed by a `mod N`, and `mod` means
**division**, which is slow on every CPU. Montgomery's 1985 trick removes the
divisions: it trades one expensive setup for many cheap reductions that use only
multiplications, additions and shifts by whole machine words.

## Montgomery form

Pick `R = 2^(w·s)` where `w = 32` is the limb size and `s` is the number of limbs
in `N`. `R` is the next power of two above `N`, and crucially `gcd(R, N) = 1`
because `N` is odd. The **Montgomery form** of `a` is

```
ã = a · R mod N
```

Addition and subtraction work unchanged on Montgomery-form values. The magic is
in multiplication: we need a way to compute `(ã · b̃) · R⁻¹ mod N` that lands back
in Montgomery form, without ever dividing by `N`.

## REDC

`REDC(T)` computes `T · R⁻¹ mod N` for `0 ≤ T < R·N`. With

```
N' = −N⁻¹ mod R        (so that N·N' ≡ −1 mod R)
```

the algorithm is:

```
m = (T mod R) · N'  mod R
t = (T + m·N) / R        // exact division: T + m·N ≡ 0 (mod R)
if t ≥ N: t = t − N
return t
```

`T + m·N` is divisible by `R` by construction, so the `/ R` is just dropping the
low `s` limbs — a shift, not a division. The single conditional subtraction at
the end is the only place the result can exceed `N`.

The Montgomery product is then `MonPro(ã, b̃) = REDC(ã · b̃) = (a·b)·R mod N`.

## CIOS

This library uses **CIOS** (Coarsely Integrated Operand Scanning), the
interleaved form of REDC from Koç, Acar & Kaliski, *"Analyzing and Comparing
Montgomery Multiplication Algorithms"* (IEEE Micro, 1996). Instead of computing
the full product `ã·b̃` and then reducing, CIOS interleaves multiplication and
reduction limb by limb, keeping the running total just `s + 2` limbs wide:

```
for i in 0..s-1:
    T += a · b[i]              # one row of the schoolbook product
    m  = T[0] · N' mod 2^32    # reduce the lowest limb to zero
    T  = (T + m·N) >> 32       # shift one limb down
reduce T once if T ≥ N
```

See `monpro()` in [`src/montgomery.cpp`](../src/montgomery.cpp).

## Converting in and out, division-free

Going into Montgomery form needs `a·R mod N`. Computing it directly would need a
division, so instead we precompute `R² mod N` once and use

```
to_montgomery(a)   = MonPro(a, R² mod N) = a·R mod N
from_montgomery(ã) = MonPro(ã, 1)        = ã·R⁻¹ mod N
```

`R mod N` and `R² mod N` are obtained at setup by doubling-and-conditional-
subtract (`montx::Montgomery`'s constructor), so the whole stack never performs a
big-integer division on the hot path. `N'` reduces to a single 32-bit value
`n0' = −N⁻¹ mod 2^32`, computed by Newton–Raphson lifting (`neg_inv_mod_2_32`).

## Why it is faster here

The benchmark (`bench/bench_modexp.cpp`) compares this against a schoolbook
`a^e mod N` whose reduction is a bitwise long division. Montgomery wins by ~85×
to ~340× depending on size, and CRT halves RSA decryption again on top of that.
The exact numbers are in the top-level README.

## References

- P. L. Montgomery, *"Modular Multiplication Without Trial Division"*,
  Mathematics of Computation 44 (1985).
- Ç. K. Koç, T. Acar, B. S. Kaliski, *"Analyzing and Comparing Montgomery
  Multiplication Algorithms"*, IEEE Micro 16(3), 1996.
- A. Menezes, P. van Oorschot, S. Vanstone, *Handbook of Applied Cryptography*,
  §14.3.2 (Montgomery reduction).
