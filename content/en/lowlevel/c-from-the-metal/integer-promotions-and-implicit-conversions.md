---
title: "Integer promotions and implicit conversions"
description: C silently converts many integer expressions before it evaluates them. Integer promotions, usual arithmetic conversions, signed/unsigned mixing, narrowing, and implementation-defined char behavior are where many legal bugs start.
tags: [c, integers, promotions, conversions, signedness]
order: 10
updated: 2026-06-29
---
# Integer promotions and implicit conversions

C arithmetic is not performed in the tiny type you wrote just because the variable has that
type. Before most integer operations, C applies **integer promotions** and then the
**usual arithmetic conversions** to pick a common type. That is why two `uint8_t` values
usually add as `int`, why `-1 < 1u` is false on common targets, and why assigning a result
back to a small type can quietly narrow it. The expression's type is a rule, not a vibe.

> The reset: every integer expression has a type after conversions. If you do not know that
> type, you do not yet know what arithmetic, comparison, or assignment you wrote.

## Promotions happen before arithmetic

Small integer types such as `char`, `signed char`, `unsigned char`, `short`, `uint8_t`, and
`uint16_t` are usually promoted to `int` before arithmetic. If `int` can represent all
values of the original type, the promoted type is `int`; otherwise it is `unsigned int`.

That means this:

```c
uint8_t a = 250;
uint8_t b = 10;
int promoted_sum = a + b;
uint8_t narrowed_sum = (uint8_t)(a + b);
```

does not add in 8 bits. On the demo platform, `a + b` has type `int` and value `260`.
Only the explicit conversion back to `uint8_t` wraps it to `4`. The promotion is useful:
it avoids many accidental overflows during intermediate arithmetic. The trap is assuming
the small storage type controls the expression.

## How it really works

After integer promotions, binary operators often apply the usual arithmetic conversions.
The rough rule is: choose a common type that can represent both operands, or convert to an
unsigned type when the signed type cannot represent all values of the unsigned type. That
last clause is where signed/unsigned bugs live.

```c
int left = -1;
unsigned int right = 1;
left < right;  // left is converted to unsigned int before comparison
```

On a 32-bit `unsigned int`, converting `-1` produces `4294967295`, so the comparison is
false. The source reads like math over integers; the language evaluates it as a comparison
after conversion.

Assignment is another conversion point. If the destination type cannot represent the
source value, the result depends on the destination category. Unsigned integer conversion
wraps modulo one more than the maximum value. Signed narrowing that cannot represent the
value is implementation-defined or may raise an implementation-defined signal. Floating to
integer, integer to floating, and enum conversions each have their own rules. Warnings are
your friend because the base language permits a lot.

Plain `char` is its own small trap. It is a distinct type from both `signed char` and
`unsigned char`, and whether plain `char` behaves signed or unsigned is
implementation-defined. Use `signed char` or `unsigned char` when signedness matters; use
`unsigned char` for raw bytes.

## Executable artifact: watch the conversions

The demo lives in
`examples/c-from-the-metal/integer-promotions-and-implicit-conversions/demo.c`. It keeps the
signed/unsigned comparison warning local and intentional while building cleanly with
`-Wall -Wextra`.

```c
#include <stdint.h>
#include <stdio.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
static void compare_signed_and_unsigned(int left, unsigned int right) {
    printf("-1 < 1u              = %s\n", left < right ? "true" : "false");
}
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

int main(void) {
    uint8_t a = 250;
    uint8_t b = 10;
    int promoted_sum = a + b;
    uint8_t narrowed_sum = (uint8_t)(a + b);
    int negative = -1;
    unsigned int one = 1;
    unsigned int converted_negative = (unsigned int)negative;
    char plain = (char)0xFF;

    /* The uint8_t values are promoted to int before addition. */
    printf("sizeof(a + b)        = %zu bytes\n", sizeof(a + b));
    printf("promoted sum          = %d\n", promoted_sum);
    printf("narrowed uint8_t sum  = %u\n", (unsigned)narrowed_sum);

    /* In a mixed comparison, the negative int is converted to unsigned. */
    compare_signed_and_unsigned(negative, one);
    printf("(unsigned)-1         = %u\n", converted_negative);

    /* Plain char signedness is implementation-defined. */
    printf("(char)0xFF as int     = %d\n", (int)plain);

    return 0;
}
```

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Real output:

```
sizeof(a + b)        = 4 bytes
promoted sum          = 260
narrowed uint8_t sum  = 4
-1 < 1u              = false
(unsigned)-1         = 4294967295
(char)0xFF as int     = -1
```

The first line proves the `uint8_t + uint8_t` expression became a 4-byte `int` here. The
comparison line shows the signed value being converted to `unsigned int`. The final line is
implementation-defined: on this platform plain `char` is signed, so `(char)0xFF` prints as
`-1`.

## Failure modes & trade-offs

- **Mixed signedness comparisons.** `int i` compared with `size_t n` can convert `i` to a
  huge unsigned value when `i` is negative.
- **Silent narrowing.** Assigning a wide result to `uint8_t`, `int16_t`, or an enum-like
  field can discard information. Make intentional narrowing explicit and check ranges at
  boundaries.
- **Assuming small-type arithmetic.** `uint8_t` storage does not imply 8-bit intermediate
  arithmetic. Promotions happen first.
- **Plain `char` portability.** Code that treats plain `char` as signed on one target may
  break on a target where it is unsigned.
- **Warnings vs noise.** `-Wconversion` and `-Wsign-conversion` can be chatty, but they
  surface exactly the conversions this note is about. Use them in development even if your
  release flags are quieter.

## In practice

- **Choose signedness by meaning.** Use signed types for quantities that can go negative;
  use unsigned when you need modular arithmetic or bit patterns, not because "sizes are
  never negative."
- **Convert near the boundary.** Parse, range-check, and convert once. Do not let implicit
  conversions leak through the core of the program.
- **Make narrowing loud.** Use casts only after a check or next to a comment explaining the
  modulo behavior you want.
- **Keep loop indices compatible with their bounds.** If a bound is `size_t`, prefer a
  `size_t` index, or handle negative sentinel values before the comparison.
- **Use fixed-width types for representation, not magic.** `uint32_t` says storage width;
  the expression rules still apply.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|The C type system is weak]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: the contract]] · [[lowlevel/c-from-the-metal/more-ub-signed-overflow-aliasing-and-sequencing|More UB: signed overflow, aliasing, and sequencing]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Two's complement & integer representation]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words & addresses]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for integer promotions, usual arithmetic conversions, assignment conversions, and implementation-defined integer behavior. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Implicit conversions in C** — compact map of integer promotions, usual arithmetic conversions, boolean conversion, and assignment conversion. https://en.cppreference.com/w/c/language/conversion
- **cppreference — Arithmetic operators** — operator-specific conversion rules for arithmetic, comparison, shifts, and bitwise operations. https://en.cppreference.com/w/c/language/operator_arithmetic
- **cppreference — Integer types** — fixed-width integer types, limits, and the distinction between storage width and expression behavior. https://en.cppreference.com/w/c/types/integer
- **GCC warning options** — practical flags such as `-Wconversion`, `-Wsign-conversion`, and `-Wsign-compare` that expose risky conversions. https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
- **Robert Seacord — *Effective C*** — security-minded guidance on integer conversions, range checks, signedness, and avoiding conversion-related defects. https://nostarch.com/Effective_C
