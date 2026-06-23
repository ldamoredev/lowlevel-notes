---
title: "IEEE 754 floating point"
description: How the machine stores real numbers — sign, exponent, and mantissa in binary scientific notation. Why finite bits make most decimals inexact (so 0.1 + 0.2 != 0.3), what inf and NaN are, and why you must never compare floats with ==.
tags: [machine-model, floating-point, ieee-754, precision, nan]
order: 10
updated: 2026-06-22
---
# IEEE 754 floating point

Integers are exact; real numbers are not. A `float` or `double` stores a number in binary
scientific notation — **sign × 1.mantissa × 2^exponent** — with a fixed number of bits for
each part. Because the bits are finite, the vast majority of decimal fractions cannot be
represented exactly, the same way 1/3 has no finite decimal expansion. **IEEE 754** is the
standard that pins down this format, the rounding rules, and the special values (infinity,
NaN) that every modern CPU and language implements. The headline consequence — `0.1 + 0.2
!= 0.3` — isn't a bug; it's finite precision in base 2, and once you see the encoding it's
obvious.

> The reset: a `double` is an *approximation* of a real number, not the number itself.
> Equality (`==`) on floats is almost always wrong; arithmetic accumulates rounding error;
> and some "numbers" (NaN) aren't even equal to themselves.

## The format: sign, exponent, mantissa

A value is `(-1)^sign × 1.mantissa × 2^(exponent − bias)`. The leading `1.` is implicit
(for normalized numbers), buying one free bit of precision. The two common sizes:

| | `float` (32-bit) | `double` (64-bit) |
|---|---|---|
| Sign | 1 bit | 1 bit |
| Exponent | 8 bits (bias 127) | 11 bits (bias 1023) |
| Mantissa | 23 bits | 52 bits |
| Decimal digits | ~7 | ~15–16 |

The exponent is stored **biased** (add 127 / 1023) so it can represent negatives without a
sign of its own. Worked examples from a 32-bit `float`:

- `1.0` → `0x3F800000`: sign 0, exponent 127 (→ 2⁰), mantissa 0 → `1.0 × 2⁰`.
- `0.5` → `0x3F000000`: exponent 126 (→ 2⁻¹) → `1.0 × 2⁻¹`.
- `-2.0` → `0xC0000000`: sign 1, exponent 128 (→ 2¹) → `−1.0 × 2¹`.
- `0.1` → `0x3DCCCCCD`: exponent 123 (→ 2⁻⁴), mantissa `0x4CCCCD` — note the repeating
  `CCC…D`, the fingerprint of a value that *can't* be stored exactly.

## Why 0.1 + 0.2 != 0.3

In binary, `0.1` is `0.0001100110011…` repeating forever — there's no finite sum of powers
of two that equals one tenth. So the stored `0.1` is the *nearest representable double*,
slightly off; likewise `0.2`. Add the two rounded values and round again, and the result
lands one ULP (unit in the last place) above the nearest double to `0.3`:

```
0.1 + 0.2 = 0.30000000000000004
0.3       = 0.29999999999999999
equal? NO
```

Both lines are 17 significant digits — the most a `double` can distinguish. Nothing is
broken: each step did correct round-to-nearest, but the rounding of the inputs and the sum
don't cancel. The same effect makes `for (double x = 0; x != 1.0; x += 0.1)` an infinite
loop. This isn't C-specific — Python, JavaScript, and Java print the identical
`0.30000000000000004`.

## Special values

The exponent field's extreme values are reserved to encode non-finite numbers:

- **±0** — yes, signed zero; `+0.0 == -0.0` is true but `1/+0` and `1/-0` differ.
- **±infinity** — exponent all ones, mantissa zero. Produced by overflow or `x / 0.0`.
  Arithmetic with it is defined: `inf + 1 == inf`.
- **NaN (Not a Number)** — exponent all ones, mantissa nonzero. Produced by `0.0/0.0`,
  `sqrt(-1)`, `inf - inf`. Its defining trap: **`NaN == NaN` is false** — a NaN compares
  unequal to everything, including itself. That's how you test for it (`x != x`, or
  `isnan(x)`).
- **Subnormals** — exponent zero: very small numbers near 0 that drop the implicit leading
  1 to degrade precision gracefully instead of jumping straight to zero.

## See it

```c
// floats.c — the inequality, the bit layout, and the specials.
// gcc -O0 -Wall -Wextra floats.c -o floats -lm && ./floats
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>

static void show_float(const char *name, float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof bits);             // type-pun via memcpy (safe)
    uint32_t sign = bits >> 31;
    uint32_t exp  = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF;
    printf("%-6s = %-10g 0x%08X  sign=%u exp=%u(2^%d) mant=0x%06X\n",
           name, (double)f, bits, sign, exp, (int)exp - 127, mant);
}

int main(void) {
    printf("0.1 + 0.2 = %.17f\n", 0.1 + 0.2);
    printf("0.3       = %.17f\n", 0.3);
    printf("equal? %s\n\n", (0.1 + 0.2 == 0.3) ? "yes" : "NO");

    show_float("1.0", 1.0f);
    show_float("0.5", 0.5f);
    show_float("-2.0", -2.0f);
    show_float("0.1", 0.1f);                     // mantissa repeats: inexact

    float inf = 1.0f / 0.0f, nan = 0.0f / 0.0f;
    printf("\n1.0/0.0 = %g (isnan %d)\n", (double)inf, isnan(inf));
    printf("0.0/0.0 = %g (isnan %d)\n", (double)nan, isnan(nan));
    printf("nan == nan ? %s\n", (nan == nan) ? "true" : "false");

    printf("\ntolerant compare: fabs((0.1+0.2)-0.3) < 1e-9 ? %s\n",
           fabs((0.1 + 0.2) - 0.3) < 1e-9 ? "true" : "false");
    return 0;
}
```

The `memcpy` into a `uint32_t` is the portable way to inspect a float's bits (a `float*`→
`int*` cast would violate strict aliasing). The output makes the encoding concrete:
exponents map to powers of two, `0.1`'s mantissa visibly repeats, NaN fails its own
equality test, and the tolerant comparison succeeds where `==` failed.

## Doing it right

- **Never compare floats with `==`.** Use a tolerance: `fabs(a - b) < eps` for values near
  1, or a *relative* epsilon (`fabs(a-b) <= eps * fmax(fabs(a), fabs(b))`) across scales.
  Choosing `eps` is problem-specific — `FLT_EPSILON`/`DBL_EPSILON` are the gap at 1.0, a
  starting point, not a universal answer.
- **Prefer `double` by default;** use `float` only when memory bandwidth or SIMD width
  demands it. The extra precision prevents many surprises for the cost of 4 bytes.
- **Never use floating point for money.** Use integer cents (or a decimal library); `0.10`
  isn't representable, and cents must be exact.
- **Mind accumulation and order.** Summing many values loses precision; adding largest-last
  or using compensated summation (Kahan) helps. `(a + b) + c != a + (b + c)` in general.

## Failure modes & trade-offs

- **Equality and loop counters.** `x == 0.3` and `x != 1.0` loop guards fail
  unpredictably. Count with integers; compute the float from the integer.
- **NaN poisoning.** One NaN propagates through every subsequent operation, and every
  comparison with it is false — a sort or `max` can silently misbehave. Check with
  `isnan`.
- **Catastrophic cancellation.** Subtracting two nearly-equal floats annihilates the
  significant digits, leaving mostly rounding noise. Reformulate the math to avoid it.
- **`float`↔`double` surprises.** Mixed-precision expressions promote to `double`, and a
  literal like `0.1` is a `double` unless you write `0.1f`; silent conversions shift
  results.

## In practice

- **Treat every float as "correct to ~15 digits, then fuzzy."** Design comparisons,
  formats, and tests around approximation, not exactness.
- **`0.30000000000000004` is the canonical tell.** When a total is off in the 16th digit,
  it's floating-point rounding, not a logic bug.
- **Exact domains use integers.** Money, counters, array indices, hashes — keep them in
  integer types; reserve floats for measured/continuous quantities.
- **When debugging, print `%.17g` and the bits.** Default printing hides the error that's
  actually there; full precision and the hex layout reveal it.

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words & addresses]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Two's complement & integers]] · [[lowlevel/machine-model/endianness|Endianness]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **David Goldberg — *What Every Computer Scientist Should Know About Floating-Point Arithmetic*** — the canonical deep treatment of rounding, error, and IEEE 754; start here. https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 2.4 — the float/double bit layout, rounding, and pitfalls, at exactly this atlas's level. https://csapp.cs.cmu.edu/
- **IEEE 754-2019 — Standard for Floating-Point Arithmetic** — the authoritative spec for formats, special values, and rounding modes. https://ieeexplore.ieee.org/document/8766229
- **Bruce Dawson — *Comparing Floating Point Numbers, 2012 Edition*** — the practical guide to epsilon, ULPs, and why naive tolerance checks fail. https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
- **cppreference — floating-point types & `<math.h>`** — `float`/`double` semantics, `isnan`, `INFINITY`, `FLT_EPSILON`, and friends. https://en.cppreference.com/w/c/language/arithmetic_types
