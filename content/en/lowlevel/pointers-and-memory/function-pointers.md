---
title: "Function pointers"
description: Function pointers let C store and call code addresses with a checked signature. They power callbacks, dispatch tables, qsort comparators, and small runtime polymorphism without objects.
tags: [function-pointers, callbacks, dispatch, c, abi]
order: 8
updated: 2026-06-30
---
# Function pointers

A function pointer is a value that names callable code with a specific signature. It lets
C pass behavior around: comparators for `qsort`, callbacks with context, dispatch tables,
state machines, signal handlers, and small plugin-like interfaces. The signature is the
contract. Calling a function through an incompatible function pointer type is undefined
behavior because the caller and callee may disagree about arguments, return value, or the
calling convention.

> The reset: object pointers point at data; function pointers point at code. They often
> look like addresses on today's machines, but C gives them separate rules.

## The shape

Raw function pointer syntax is noisy:

```c
int (*op)(int, int);
```

Read it from the identifier out: `op` is a pointer to a function taking two `int`
arguments and returning `int`. A typedef makes APIs readable:

```c
typedef int (*binary_op)(int left, int right);
```

Then you can store functions, pass them, and call through them:

```c
binary_op op = add;
int result = op(2, 3);
```

The function designator `add` automatically converts to a pointer in most expression
contexts, much like arrays decay to pointers. `op(2, 3)` and `(*op)(2, 3)` are both valid;
the shorter call syntax is idiomatic.

## How it really works

The function pointer value identifies a function entry point according to the platform's
ABI. The compiler uses the pointed-to function type to generate the call sequence:
argument registers or stack slots, return register, and cleanup conventions. On the
System V AMD64 ABI target used by this atlas, integer arguments commonly flow through
registers such as `rdi`, `rsi`, `rdx`, and return through `rax`. A mismatched function
pointer type can make the caller put bits where the callee is not expecting them.

Function pointers are a low-level form of dynamic dispatch. A dispatch table is just an
array or struct of function pointers:

```c
struct Command {
    const char *name;
    binary_op run;
};
```

This is not object-oriented inheritance; there is no hidden receiver unless you pass one.
Most callback APIs therefore pair a function pointer with a `void *ctx` pointer so the
callback has state:

```c
typedef void (*visit_fn)(void *ctx, int value);
```

The function pointer supplies behavior; the context pointer supplies data. That pair is a
manual closure.

Do not treat function pointers as `void*`. The C standard guarantees conversions between
`void*` and object pointers, not between `void*` and function pointers. POSIX APIs such as
`dlsym` live at a platform boundary with their own rules; ordinary portable C should keep
code pointers and object pointers separate.

## Executable artifact: a tiny dispatch table

The demo lives in `examples/pointers-and-memory/function-pointers/demo.c`.

```c
// demo.c - shows function pointers with a command table, an `apply` function
// that receives callbacks, and function pointer comparison.
// Compiles cleanly and runs with:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>

typedef int (*binary_op)(int left, int right);

struct Command {
    const char *name;
    binary_op run;
};

static int add(int left, int right) {
    return left + right;
}

static int multiply(int left, int right) {
    return left * right;
}

static int apply(binary_op op, int left, int right) {
    return op(left, right);
}

int main(void) {
    struct Command commands[] = {
        {"add", add},
        {"mul", multiply},
    };

    for (size_t i = 0; i < sizeof commands / sizeof commands[0]; i++) {
        printf("%s(6, 7)              = %d\n",
               commands[i].name, commands[i].run(6, 7));
    }

    binary_op op = add;
    printf("apply add             = %d\n", apply(op, 2, 3));

    op = multiply;
    printf("apply multiply        = %d\n", apply(op, 2, 3));
    printf("same function pointer = %s\n",
           op == multiply ? "yes" : "no");

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
add(6, 7)              = 13
mul(6, 7)              = 42
apply add             = 5
apply multiply        = 6
same function pointer = yes
```

The table stores two names and two function pointers with the same signature. `apply`
does not know which operation it will call until runtime, but the signature still tells
the compiler how to perform the call.

## Failure modes & trade-offs

- **Wrong signature.** Casting a function to the wrong pointer type and calling it is
  undefined behavior. The machine-level call sequence may not match.
- **Null function pointer.** A function pointer can be null. Calling it is undefined
  behavior, just like dereferencing a null object pointer.
- **Object pointer confusion.** A function pointer is not portably storable in `void*`.
  Keep callback functions and callback contexts as two separate values.
- **Lifetime of pointed-to code.** Normal program functions live for the program lifetime.
  Dynamically loaded code can disappear if its library is unloaded while pointers remain.
- **Inlining and optimization trade-off.** Indirect calls are harder to inline and can be
  harder for branch prediction than direct calls. Use them when configurability matters.
- **Callback ownership ambiguity.** A function pointer plus `void *ctx` must define who
  owns the context and how long it stays alive.

## In practice

- **Use typedefs for callback signatures.** They make dispatch tables and API contracts
  readable.
- **Pair callbacks with context.** A callback without state is rare; pass `void *ctx`
  explicitly when the API needs a manual closure.
- **Keep signatures exact.** Avoid casts around function pointers; fix the declared type
  instead.
- **Prefer tables over switch ladders for stable command sets.** Function pointer tables
  make dispatch data-driven and easy to test.
- **Use direct calls in hot code unless dispatch is needed.** Dynamic dispatch has a cost;
  earn it with flexibility.

**Connects to:** [[lowlevel/pointers-and-memory/void-star-and-type-erasure|void* & type erasure]] · [[lowlevel/pointers-and-memory/pointers-to-pointers-double-indirection|Pointers to pointers]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — function designators, function pointer types, calls through function pointers, and undefined behavior for incompatible calls. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Pointer declaration** — function pointer syntax and examples. https://en.cppreference.com/w/c/language/pointer
- **cppreference — `qsort`** — standard comparator callback API using function pointers and `void*`. https://en.cppreference.com/w/c/algorithm/qsort
- **System V AMD64 ABI** — machine-level calling convention that explains why function signatures must match. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Jens Gustedt — *Modern C*** — callbacks, function pointer types, and C API design. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Beej's Guide to C — Function pointers** — approachable syntax and callback examples. https://beej.us/guide/bgc/html/split/pointers.html
