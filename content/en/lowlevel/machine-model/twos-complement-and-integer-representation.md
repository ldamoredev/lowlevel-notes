---
title: "Two's complement & integer representation"
description: How the machine stores signed integers — two's complement, the encoding where the same add/subtract hardware works for signed and unsigned, -1 is all ones, and INT_MIN has no positive twin. Why signed overflow is UB but unsigned wraps, and the signed/unsigned comparison trap.
tags: [machine-model, integers, twos-complement, overflow, signedness]
order: 8
updated: 2026-06-22
---
# Two's complement & integer representation

The previous note said a number is just bytes. Now: *which* bytes? For unsigned integers
it's obvious — plain base-2. For **signed** integers nearly every machine uses **two's
complement**, an encoding chosen for one beautiful property: the *same* addition and
subtraction circuitry works for signed and unsigned values, with no special-casing for
the sign. The cost is a deliberate asymmetry — one more negative number than positive —
and a set of sharp edges (overflow, signed/unsigned conversion) that cause some of the
most common and dangerous bugs in C. This note makes the encoding concrete so those edges
stop being surprises.

> The reset: there is no "minus sign" stored anywhere. Negativity is just a bit pattern
> the hardware agreed to interpret a certain way. `-1` and `4294967295` are the *same 32
> bits* — only the type decides which one you meant.

## How two's complement works

For an N-bit signed integer, the top bit has **negative** weight. In 32 bits the value is:

```
value = -b31·2^31 + b30·2^30 + ... + b1·2^1 + b0·2^0
```

So the high bit being 1 doesn't mean "subtract" as a flag — it contributes −2³¹ directly.
The rule to **negate** a value is "invert all bits, then add 1" (`-x == ~x + 1`):

| Value | 32-bit hex | Top bit |
|---|---|---|
| `0` | `0x00000000` | 0 |
| `1` | `0x00000001` | 0 |
| `-1` | `0xFFFFFFFF` | 1 |
| `INT_MAX` (2147483647) | `0x7FFFFFFF` | 0 |
| `INT_MIN` (−2147483648) | `0x80000000` | 1 |

Two consequences fall straight out:

- **−1 is all ones.** Every bit set is the additive inverse of 1 — which is why bitmasks
  and `-1` show up interchangeably in low-level code.
- **The range is asymmetric.** `INT_MIN = −2³¹` but `INT_MAX = 2³¹−1`. There are
  2³¹ negatives and only 2³¹−1 positives, so **`−INT_MIN` is not representable** — negating
  it overflows. This single fact breaks naive `abs()`, naive parsing, and more.

The payoff: because the encoding wraps modulo 2ᴺ, `a + b` and `a − b` produce the correct
bit pattern whether you read the operands as signed or unsigned. The CPU has *one* `add`,
not two. (This is why an `add` instruction sets flags for both interpretations; the
[[lowlevel/machine-model/registers-and-the-isa|ISA]] doesn't track signedness — your types
do.)

## Signed overflow is UB; unsigned wraps

This is the part that bites hardest, and it's asymmetric in C:

- **Unsigned overflow is defined.** Unsigned arithmetic is modular: `0u - 1 == UINT_MAX`,
  `UINT_MAX + 1 == 0`. Guaranteed by the standard, perfectly portable.
- **Signed overflow is undefined behavior.** `INT_MAX + 1` is **UB** — not "wraps to
  INT_MIN." The compiler is allowed to assume signed overflow *never happens* and optimize
  on that basis, so an overflowing signed expression can do literally anything, including
  miscompiling the surrounding code.

This trips up everyone coming from languages where `int` silently wraps. In C, `for (int i
= 0; i <= n; i++)` where `n == INT_MAX` is an *infinite loop the optimizer may delete or
mangle*, because the exit test assumes no overflow. The fix is to know which integers can
overflow and use unsigned (or wider types, or `-fwrapv`, or checked arithmetic) where it
matters. See [[lowlevel/c-from-the-metal/index|C from the metal]] for the full UB contract.

## The signed/unsigned conversion trap

When you mix signed and unsigned in one expression, C converts the **signed operand to
unsigned** (the "usual arithmetic conversions"). The bits don't change — but their meaning
flips, and a negative number becomes a huge positive one:

```c
int a = -1;
unsigned b = 0;
if (a > b) { /* THIS RUNS */ }   // -1 converts to UINT_MAX (4294967295) > 0
```

`-1 > 0u` is **true**, because `-1` reinterpreted as unsigned is `0xFFFFFFFF`. The same trap
hides in `for (unsigned i = n; i >= 0; i--)` — an infinite loop, since unsigned `i` is never
< 0 — and in comparing a signed length to `strlen`'s unsigned `size_t`. Modern compilers warn
(`-Wsign-compare`); heed it.

## See the bits

```c
// integers.c — bit patterns, negation, wraparound, and the comparison trap.
// gcc -O0 -Wall -Wextra integers.c -o integers && ./integers
#include <stdio.h>
#include <limits.h>

static void bits32(const char *name, unsigned int v) {
    printf("%-8s = %11d  0x%08X  ", name, (int)v, v);
    for (int i = 31; i >= 0; i--) {
        putchar((v >> i) & 1 ? '1' : '0');
        if (i % 8 == 0) putchar(' ');
    }
    putchar('\n');
}

int main(void) {
    printf("INT_MAX=%d  INT_MIN=%d  UINT_MAX=%u\n\n", INT_MAX, INT_MIN, UINT_MAX);

    bits32("0", 0);
    bits32("1", 1);
    bits32("-1", (unsigned)-1);                 // all ones
    bits32("INT_MAX", (unsigned)INT_MAX);       // 0x7FFFFFFF
    bits32("INT_MIN", (unsigned)INT_MIN);       // 0x80000000

    int x = 5;
    printf("\nnegate 5 via ~x + 1 = %d\n", ~x + 1);

    unsigned u = 0;
    printf("unsigned 0u - 1     = %u   (wraps to UINT_MAX)\n", u - 1);

    int a = -1; unsigned b = 0;                 // the conversion trap
    printf("in (a > b): (unsigned)(-1) = %u, so a > b is %s\n",
           (unsigned)a, ((unsigned)a > b) ? "true (!)" : "false");
    return 0;
}
```

Output confirms it: `-1` is `0xFFFFFFFF`, `INT_MIN` is `0x80000000`, `~5 + 1 == -5`,
`0u - 1 == 4294967295`, and the signed `-1` becomes `4294967295` the instant it's compared
against an unsigned. (Note the `(unsigned)a` casts are deliberate — writing the comparison
as `a > b` directly is exactly what `-Wsign-compare` flags.)

## Failure modes & trade-offs

- **`abs(INT_MIN)` is UB.** `-INT_MIN` overflows because there's no positive twin. The same
  asymmetry breaks `-x`, `x / -1`, and naive integer-parsing of the most-negative value.
- **Assuming signed wrap.** `INT_MAX + 1` is undefined, not `INT_MIN`. Optimizers exploit
  this; "it worked in debug" is not safety. Use unsigned or checked math for values that may
  overflow.
- **Mixing signedness in comparisons/loops.** `size_t` (unsigned) vs `int` (signed) is the
  classic; `i >= 0` on an unsigned loop counter never ends. Keep loop counters and sizes the
  same signedness.
- **Right-shifting signed values.** `>>` on a negative signed int is implementation-defined
  (usually arithmetic shift). For bit work, shift **unsigned** types.

## In practice

- **Sizes and indices are `size_t` (unsigned); use signed `int` for values that go
  negative.** Don't mix them in one comparison — convert explicitly so the reader sees it.
- **Treat `-Wsign-compare` and `-Wconversion` as errors.** They catch the conversion trap
  before it ships; in this atlas's OS work, `-Werror` is on.
- **For exact wraparound semantics, use unsigned;** for trapping on overflow, use checked
  helpers (`__builtin_add_overflow`) or `-fsanitize=signed-integer-overflow` in tests.
- **Remember `-1 == 0xFFFF...`** Seeing all-ones in a debugger and recognizing "that's −1
  (or an unsigned max, or a bitmask)" is everyday low-level fluency.

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words & addresses]] · [[lowlevel/machine-model/registers-and-the-isa|Registers & the ISA]] · [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 2 — integer representation, two's complement, overflow, and signed/unsigned conversions; the spine for this note. https://csapp.cs.cmu.edu/
- **Jens Gustedt — *Modern C*** — C's integer model, the usual arithmetic conversions, and why unsigned wraps but signed doesn't. https://gustedt.gitlabpages.inria.fr/modern-c/
- **cppreference — Arithmetic operators & implicit conversions** — the exact rules for usual arithmetic conversions and integer overflow. https://en.cppreference.com/w/c/language/operator_arithmetic
- **Dietz et al. — *Understanding Integer Overflow in C/C++*** — empirical study of overflow bugs and why signed overflow UB matters in practice. https://www.cs.utah.edu/~regehr/papers/overflow12.pdf
- **ISO/IEC 9899 (C standard), §6.2.5 & §6.3.1.8** — integer types, the conversion rank rules, and overflow semantics. https://www.open-std.org/jtc1/sc22/wg14/
