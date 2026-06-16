# Threat model: why constant-time crypto matters in a car

This library is small, but the property it demonstrates — modular exponentiation
that does not leak the private key through timing or power — is exactly what an
automotive security program cares about. This note explains the connection.

## What is different about an ECU

A server lives in a locked data centre; an Electronic Control Unit lives in a car
that the attacker **owns and can take apart**. That changes the attacker model:

- **Physical access is the baseline, not the worst case.** An attacker can put a
  shunt resistor on the power line, a near-field EM probe over the package, or a
  clock/voltage glitcher on the supply. Power and EM side channels (SPA, DPA, CPA)
  and fault injection are all on the table.
- **Keys are long-lived and shared-fate.** A key extracted from one ECU on a
  bench can unlock a capability across a whole vehicle line. ECU firmware signing
  keys, immobiliser / keyless-entry secrets, and the keys behind **AUTOSAR SecOC**
  message authentication are high-value targets.
- **The crypto is the same primitives.** Secure boot and firmware updates verify
  RSA/ECDSA signatures; key exchange and attestation use DH/ECDH. Underneath every
  one of those is the modular exponentiation (or scalar multiplication) this
  project implements — and the Montgomery ladder is the standard SPA-resistant way
  to do it.

## Where the leak in experiment 1 maps to real hardware

The lab recovers a private exponent from the *operation pattern* of a leaky
square-and-multiply. On a real ECU that same pattern shows up as:

| Lab signal (software model) | Real-world channel |
|---|---|
| "square" vs "square+multiply" per bit | SPA — a single power/EM trace |
| run time ∝ Hamming weight of exponent | remote/timing attack over a bus |
| data-dependent extra REDC subtraction | Schindler/Walter timing attack on RSA |

The defence is identical in both worlds: a powering ladder that does the same
field operations every iteration, built from branchless primitives. That is what
`Montgomery::pow_ct` and `include/montx/ct.hpp` show.

## What a production automotive design adds on top

Constant-time control flow is necessary but not sufficient. A shipping ECU would
also need:

- **Blinding** — randomise the base and/or exponent (and the message) each
  operation so that DPA/CPA cannot average out the noise.
- **Fault-attack countermeasures** — CRT-RSA without a verification step leaks the
  factorisation to a single glitch (the Bellcore attack); production code verifies
  the signature before releasing it, or recomputes.
- **Validated entropy** — key generation seeded from a certified TRNG/DRBG.
- **Hardware support** — many automotive MCUs ship an HSM / SHE / EVITA module;
  the same constant-time discipline applies to any software fallback.
- **Assembly-level verification** — confirm the compiler did not reintroduce
  branches, and test statistically (`dudect`).

## Related work in this portfolio

This sits alongside my other security projects: a CAN-bus / SecOC / IDS / UDS
**automotive security simulator**, a from-scratch **AEAD secure-channel protocol**,
a **CSPRNG / entropy toolkit**, and standard-compliant **RSA** and **SHA-2**
toolkits. Together they cover the stack an ECU security feature relies on — from
the randomness source, through the primitives, up to the in-vehicle protocols.
