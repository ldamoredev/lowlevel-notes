---
title: "Undefined behavior: the contract you didn't know you signed"
description: Undefined behavior is not a quirky runtime result; it is a broken contract with the C abstract machine. Once your program invokes UB, the compiler may assume that path never happens and optimize around it.
tags: [c, undefined-behavior, compiler, optimization, safety]
order: 3
updated: 2026-06-22
---
# Undefined behavior: the contract you didn't know you signed

Undefined behavior (UB) is what happens when a C program steps outside the rules of the C
abstract machine. It is not "the CPU will do something weird" and it is not "the result is
unpredictable but local." It means the standard gives the implementation no requirements,
so the compiler may assume that path never happens and optimize using that assumption. UB
is the hidden contract behind C's speed: you get low-level control only while you keep the
promises the optimizer relies on.

> The reset: UB is not a bad value. UB is the program leaving the language. After that,
> reasoning from source code, assembly, or one local run can all mislead you.

## UB is a contract with the abstract machine

C is specified in terms of an abstract machine: objects, lifetimes, effective types,
sequencing, integer rules, pointer rules, and library preconditions. Your source code is
not a list of CPU instructions. It is a claim that, for every execution the compiler has
to care about, those abstract-machine rules are respected.

That claim buys performance. The compiler can assume:

| Rule you promise | Optimization the compiler can make |
|---|---|
| signed `int` overflow never happens | simplify comparisons and loop exits |
| array access stays within the object | remove impossible bounds cases |
| non-null pointers are not dereferenced when null | delete redundant null checks |
| object lifetimes are respected | reuse stack slots and registers aggressively |
| effective type / aliasing rules are respected | keep values cached instead of reloading memory |

This is why UB is different from **implementation-defined behavior** and **unspecified
behavior**. Implementation-defined behavior has a documented choice, such as the signedness
of plain `char` on a target. Unspecified behavior chooses among valid possibilities, such
as argument evaluation order in many calls. UB has no requirements at all: the compiler is
allowed to behave as if your program did not do that.

## How it really works

When the optimizer sees code, it builds a model of what must be true if the program has no
UB. Those truths become facts. If `p` is dereferenced, then along that path `p` is assumed
non-null. If `i + 1 > i` for signed `int`, then signed overflow is assumed absent. If a
loop index is used to access `a[i]`, accesses outside the array are assumed impossible in
valid executions.

At machine level, the CPU might have a concrete instruction for the bad operation. Signed
overflow on x86-64 produces wrapped bits. Reading one past an array may load whatever byte
is next. Type-punning through the wrong pointer may appear to work. But the compiler did
not promise to preserve that accident. It may reorder, delete, fold, vectorize, or cache
based on the stronger source-level contract.

Common UB families:

- **Lifetime UB.** Use-after-free, returning a pointer to a local, reading an object before
  its lifetime starts, or using storage after its lifetime ends.
- **Bounds UB.** Accessing outside an array object, even by one element when you dereference
  it. Forming one-past is allowed; reading one-past is not.
- **Integer UB.** Signed integer overflow, division overflow (`INT_MIN / -1`), and division
  by zero.
- **Pointer UB.** Dereferencing null, misaligned access, invalid pointer arithmetic, or
  comparing unrelated pointers in ways the standard does not define.
- **Type UB.** Violating effective type / strict aliasing rules by accessing an object
  through an incompatible pointer type.
- **Sequencing UB.** Modifying a scalar more than once without sequencing, such as old C
  interview traps like `i = i++ + ++i`.

The [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|weak type system]] catches some
shape errors before this point. UB is what happens when the program still type-checks but
the contract underneath it is false.

## Executable artifact: avoid the bad path

This demo deliberately **does not execute UB**. It shows three classic edges and the
defined version: checked signed addition, bounds checks before indexing, and `memcpy` for
type-punning bytes. Compile it normally first; sanitizers come after.

```c
// demo.c — UB as a contract: don't execute "the bad case"; check it first.
// gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void checked_add_int(int a, int b) {
    int out = 0;

    if (__builtin_add_overflow(a, b, &out)) {
        printf("checked add: %d + %d would overflow a signed int (UB avoided)\n", a, b);
        return;
    }

    printf("checked add: %d + %d = %d\n", a, b, out);
}

static void read_index(const int *items, size_t count, size_t index) {
    if (index >= count) {
        printf("bounds check: index %zu is outside 0..%zu (UB avoided)\n",
               index, count - 1);
        return;
    }

    printf("bounds check: items[%zu] = %d\n", index, items[index]);
}

static void show_float_bits(float value) {
    uint32_t bits = 0;

    memcpy(&bits, &value, sizeof bits);
    printf("memcpy punning: %.1f has bits 0x%08X\n", value, (unsigned)bits);
}

int main(void) {
    int items[] = {10, 20, 30};

    checked_add_int(40, 2);
    checked_add_int(INT_MAX, 1);

    read_index(items, sizeof items / sizeof items[0], 1);
    read_index(items, sizeof items / sizeof items[0], 3);

    show_float_bits(1.0f);
    return 0;
}
```

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
```

Real output:

```
checked add: 40 + 2 = 42
checked add: 2147483647 + 1 would overflow a signed int (UB avoided)
bounds check: items[1] = 20
bounds check: index 3 is outside 0..2 (UB avoided)
memcpy punning: 1.0 has bits 0x3F800000
```

The important part is what the program refuses to do. It does not compute `INT_MAX + 1`
as a signed expression and hope for wraparound. It does not read `items[3]`. It does not
reinterpret a `float *` as a `uint32_t *`; it copies the object representation through
`memcpy`, which C defines. In real code, pair this style with UBSan/ASan in tests:

```bash
gcc -O1 -g -fsanitize=undefined,address demo.c -o demo-asan-ubsan
```

Sanitizers do not change the contract. They make many contract violations loud before
the optimizer turns them into something harder to debug.

## Failure modes & trade-offs

- **"It worked on my machine."** A local run only proves one compiler, one optimization
  level, one input, one layout, and one moment. UB may disappear at `-O0` and break at
  `-O2`, or only under link-time optimization.
- **Accidental hardware reasoning.** x86-64 wraps signed overflow in hardware, but C signed
  overflow is still UB. The source-level rule wins over the instruction-level accident.
- **Warnings are incomplete.** `-Wall -Wextra` catches many suspicious patterns, not all
  UB. Some UB depends on runtime values, aliasing across functions, or ownership history.
- **Sanitizers are tests, not proof.** UBSan and ASan are excellent, but they cover executed
  paths. Untested paths can still contain UB.
- **Avoiding UB can cost ceremony.** Bounds checks, checked arithmetic, `memcpy` for
  representation, and ownership discipline add code. That ceremony is cheaper than
  debugging an optimizer-enabled ghost.

## In practice

- **Treat UB as a design bug, not a runtime event.** The right fix is to make the illegal
  state unrepresentable or guarded before it happens.
- **Use unsigned only when you want modulo arithmetic.** Do not use it merely to silence
  signed overflow; it changes comparisons and conversions too.
- **Keep pointer provenance simple.** Allocate, pass, and free through obvious ownership
  paths. Avoid clever arithmetic outside the object you actually own.
- **Use `memcpy` for object-representation copies.** Compilers recognize and optimize small
  `memcpy` calls; you get defined behavior without giving up generated code quality.
- **Turn on the tools early.** Warnings, UBSan, ASan, fuzzing, and code review are the
  practical counterweight to C's thin contract.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/why-c-still-matters|Why C still matters]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|The C type system is weak]] · [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Two's complement & integer representation]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for undefined behavior, the abstract machine, object lifetimes, effective type, sequencing, and integer rules. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Undefined behavior** — compact catalog of undefined, unspecified, and implementation-defined behavior with optimization examples. https://en.cppreference.com/w/c/language/behavior
- **cppreference — Object model and effective type** — the rules behind object representation, aliasing, alignment, and why character-byte inspection and `memcpy` are special. https://en.cppreference.com/w/c/language/object
- **Jens Gustedt — *Modern C*** — practical framing of C's abstract machine, undefined behavior, integer rules, object lifetimes, and portable idioms. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Clang UndefinedBehaviorSanitizer docs** — how UBSan instruments many UB classes in test builds and what it can catch. https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
- **GCC integer overflow built-ins** — checked arithmetic primitives such as `__builtin_add_overflow`, useful when overflow must be handled explicitly. https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html
