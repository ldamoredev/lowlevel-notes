---
title: "More UB: signed overflow, aliasing, and sequencing"
description: Three C undefined-behavior traps that look harmless because the hardware has an obvious result: signed overflow, strict aliasing violations, and unsequenced side effects. What the standard says, what optimizers assume, and how to write the defined version.
tags: [c, undefined-behavior, overflow, aliasing, sequencing]
order: 4
updated: 2026-06-22
---
# More UB: signed overflow, aliasing, and sequencing

The previous note gave the big rule: undefined behavior is the program leaving the C
abstract machine. This note drills into three cases that fool even experienced engineers
because the hardware seems to have an obvious answer: signed overflow wraps in registers,
type punning "just reads the same bytes," and `i++` has a value you can imagine. In C,
those intuitions are not the contract. The compiler is allowed to optimize as if signed
overflow never happens, incompatible pointers do not alias, and side effects occur only in
sequenced ways.

> The reset: if the C standard says an operation is UB, the CPU's visible behavior is not
> your portability rule. The optimizer reasons from the source-language promise first.

## Three promises optimizers exploit

C gives the optimizer more freedom than "emit the obvious instruction for each source
line." Three of the most important promises are:

| Promise | Tempting bad shape | Defined shape |
|---|---|---|
| signed overflow does not happen | `int y = INT_MAX + 1;` | check, widen, or use unsigned modulo arithmetic |
| incompatible pointer types do not alias | `uint32_t bits = *(uint32_t *)&f;` | copy representation with `memcpy` |
| side effects are sequenced | `int y = i++ + ++i;` | split updates into separate statements |

These are not stylistic preferences. They are assumptions the compiler can use when it
folds expressions, vectorizes loops, caches values in registers, and deletes branches it
proves unreachable in any valid C execution.

## Signed overflow is not wraparound

Unsigned arithmetic in C is modular: `UINT_MAX + 1u == 0u`. Signed overflow is different.
If an `int` expression produces a value outside the representable range, the behavior is
undefined. On x86-64, the `add` or `imul` instruction may leave wrapped bits in a register,
but C did not promise that your signed expression maps to "do the instruction and keep the
low bits."

That freedom lets the optimizer assume facts like `x + 1 > x` for signed `int`. If signed
overflow were allowed to wrap, that would be false at `INT_MAX`. Because overflow is UB,
the compiler can treat the overflowing case as impossible and simplify code around it.
This is why "it worked at `-O0`" is weak evidence: low optimization often preserves more
source shape, while `-O2` uses the contract aggressively.

Decision rule:

- Use **signed** types for quantities that are conceptually negative or ordered, but guard
  arithmetic that may overflow.
- Use **unsigned** types only when modulo arithmetic is the actual domain model, not merely
  as a way to dodge UB.
- Use checked helpers like `__builtin_add_overflow` / `__builtin_mul_overflow`, wider
  intermediates, or explicit range checks when overflow is possible.

This connects directly to
[[lowlevel/machine-model/twos-complement-and-integer-representation|two's complement]]:
the bit pattern exists, but C's signed-overflow rule still controls the source program.

## Aliasing: bytes are not always types

Aliasing means two expressions refer to the same storage. C allows some aliasing and
forbids other aliasing so the compiler can avoid unnecessary reloads. The rough rule:
access an object through an lvalue of a compatible type, a qualified version of that type,
certain aggregate cases, or a character type. Accessing a `float` object through a
`uint32_t *` just because both are 4 bytes is not generally defined.

The common trap:

```c
float f = 1.0f;
uint32_t bits = *(uint32_t *)&f;  // strict-aliasing UB
```

This often "works" in a debug build because the bytes are indeed there. The problem is not
the memory; it is the promise you made to the compiler. If a `float *` and a `uint32_t *`
are assumed not to alias, the optimizer can keep the float in a register, reorder stores,
or reuse stale values. The defined way to copy object representation is `memcpy` into an
integer object of the same size. Modern compilers optimize that tiny `memcpy` away.

Character types are special: inspecting an object's representation through `unsigned char
*` is defined. That is why the first note's byte dump was legal, and why binary code often
uses bytes at boundaries while keeping typed access honest inside the program.

## Sequencing: side effects need order

C expressions do not always evaluate left to right. The standard defines places where
evaluations are sequenced, and places where the order is unspecified. If you modify the
same scalar object more than once between sequencing points, or modify it and also read it
for an unrelated value computation without sequencing, you can get UB.

Classic bad shape:

```c
int i = 3;
int y = i++ + ++i;  // unsequenced modifications of i
```

Do not try to memorize what this "prints on GCC." There is no value to memorize. The
program violated the sequencing rules. The fix is simple and boring: split the side
effects into statements so the order is explicit. In low-level C, boring sequence is a
feature. It gives the compiler a real contract and gives the reviewer something auditable.

Function-call argument order is another trap. In many calls, the order in which arguments
are evaluated is unspecified. If two arguments mutate or depend on the same object, pull
them into named temporaries first.

## Executable artifact: the defined versions

This program does not execute the bad expressions. It shows the three defined patterns:
checked signed multiplication, alias-safe type punning with `memcpy`, and sequenced side
effects.

```c
// demo.c — tres bordes de UB y sus versiones definidas.
// gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void checked_mul_int(int a, int b) {
    int out = 0;

    if (__builtin_mul_overflow(a, b, &out)) {
        printf("signed overflow avoided: %d * %d does not fit in int\n", a, b);
        return;
    }

    printf("checked multiply: %d * %d = %d\n", a, b, out);
}

static void show_float_bits(float value) {
    uint32_t bits = 0;

    memcpy(&bits, &value, sizeof bits);
    printf("alias-safe punning: %.2f -> 0x%08X\n", value, (unsigned)bits);
}

static void sequenced_update(int start) {
    int i = start;
    int left = i;
    i++;
    int right = i;
    i++;

    printf("sequenced effects: start=%d left=%d right=%d final=%d sum=%d\n",
           start, left, right, i, left + right);
}

int main(void) {
    checked_mul_int(1000, 20);
    checked_mul_int(INT_MAX, 2);

    show_float_bits(1.50f);

    sequenced_update(3);
    return 0;
}
```

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
```

Real output:

```
checked multiply: 1000 * 20 = 20000
signed overflow avoided: 2147483647 * 2 does not fit in int
alias-safe punning: 1.50 -> 0x3FC00000
sequenced effects: start=3 left=3 right=4 final=5 sum=7
```

Notice the pattern: the program keeps the dangerous operation outside the executed path.
It checks before overflowing, copies bytes instead of lying through an incompatible pointer,
and gives each side effect its own statement. That is not less "low-level." It is the
defined low-level spelling.

## Failure modes & trade-offs

- **Assuming debug behavior is the contract.** UB often appears stable under `-O0` because
  the compiler preserves source shape. The same source can change under `-O2`, LTO, or a
  different compiler.
- **Using unsigned as a reflex.** Unsigned avoids signed-overflow UB, but introduces
  modular arithmetic and signed/unsigned conversion traps. Use it intentionally.
- **Pointer casts as serialization.** Casting a byte buffer to a protocol `struct *` can
  violate alignment, effective type, padding, and endianness assumptions. Parse bytes
  explicitly at boundaries.
- **Clever expressions hiding side effects.** `++`, `--`, assignment inside expressions,
  and macro arguments can make sequencing hard to review. Split them when state changes.
- **Tool gaps.** UBSan catches many signed-overflow and sequencing issues, but strict
  aliasing bugs can be subtle and input-dependent. Code shape still matters.

## In practice

- **Make overflow policy explicit.** Choose checked, saturated, widened, or modulo
  arithmetic by domain. Do not let overflow policy be an accident of type choice.
- **Prefer `memcpy` for representation moves.** It communicates intent, obeys effective
  type rules, and optimizes well for fixed small sizes.
- **Keep `-fno-strict-aliasing` as a last resort.** It may make legacy code survive, but it
  gives up optimizer information globally. Fixing the aliasing shape is better when you
  own the code.
- **Keep side effects boring.** One mutation per statement is not childish; it is
  reviewable C.
- **Turn on focused warnings and sanitizers.** Add `-Wstrict-aliasing`, `-Wsequence-point`,
  `-Wconversion`, UBSan, and ASan to the test loop where they fit.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: the contract]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|The C type system is weak]] · [[lowlevel/c-from-the-metal/why-c-still-matters|Why C still matters]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Two's complement & integer representation]] · [[lowlevel/machine-model/endianness|Endianness]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for signed overflow, effective type, aliasing permissions, and sequencing rules. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Undefined behavior** — concise examples of signed overflow, strict aliasing, and unsequenced side effects as UB. https://en.cppreference.com/w/c/language/behavior
- **cppreference — Order of evaluation** — the practical reference for sequencing, unspecified order, and unsequenced modifications. https://en.cppreference.com/w/c/language/eval_order
- **cppreference — Object model and effective type** — the rules behind object representation, character inspection, and aliasing. https://en.cppreference.com/w/c/language/object
- **GCC integer overflow built-ins** — checked arithmetic helpers used by the demo and production C code that must handle overflow. https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html
- **Clang UndefinedBehaviorSanitizer docs** — sanitizer coverage for signed overflow, alignment, null, and several other UB families. https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
