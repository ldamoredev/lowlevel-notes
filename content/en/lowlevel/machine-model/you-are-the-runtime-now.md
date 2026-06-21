---
title: "You are the runtime now"
description: The managed runtime quietly did bounds checks, freed your memory, normalized your integers, and caught your type errors. Bare metal removes that floor — the CPU executes your bytes exactly, including the wrong thing. This is the reset for the whole atlas.
tags: [machine-model, undefined-behavior, mental-model, foundations]
order: 1
updated: 2026-06-21
featured: true
---
# You are the runtime now

For your whole career a layer of software stood between your code and the machine and
quietly cleaned up after you. It checked every array index, freed memory you forgot
about, kept integers from doing anything surprising, and turned your mistakes into
clean exceptions with stack traces. That layer — the JavaScript engine, the JVM, the
.NET CLR, the Python interpreter — is the **managed runtime**, and going low-level
means removing it. From here down there is no floor: the CPU executes the bytes your
compiler emitted, exactly, at a few nanoseconds each, including the bytes that
encode a mistake.

> The reset: nothing is watching your back anymore. A bug in managed code is a red
> test or a caught exception. A bug here can silently corrupt memory, return last
> week's stack garbage, or vanish entirely under the optimizer — and still "work on
> your machine."

## What the runtime was doing for you

Everything in this column was a *service*, paid for in CPU and memory, that you never
had to think about. Low-level programming hands each one back to you.

| The runtime did this for you | At bare metal, it's your job |
|---|---|
| Bounds-checked every array access | Nothing checks an index — out of range reads/writes whatever is there |
| Garbage-collected unused memory | You `free()` what you `malloc()`, exactly once, at the right time |
| Normalized integer math (or used bignums) | Fixed-width integers; signed overflow is **undefined**, not "wraps" |
| Zero-initialized variables and fields | A local variable holds whatever bytes were on the stack |
| Enforced types at runtime | A type is a *layout instruction*; reinterpret bytes and the machine obeys |
| Turned faults into exceptions | A bad pointer is a segfault — or worse, silent corruption with no fault at all |
| Made `null` a checked, throwing value | A null dereference is undefined behavior the compiler may *assume never happens* |

None of this is the CPU being hostile. The CPU has no concept of "array," "object,"
or "type." It has registers, addresses, and instructions. Those abstractions lived
entirely in the runtime you just removed.

## The new contract, made concrete

Here is the same tiny program twice. First in TypeScript, where the runtime keeps
every promise:

```ts
const a = [10, 20, 30];
console.log(a[5]);              // undefined — defined behavior, no crash, no garbage

let x = Number.MAX_SAFE_INTEGER;
console.log(x + 1);            // a wrong-but-defined number; never undefined behavior

let u: number;
console.log(u);               // the compiler refuses: "used before being assigned" (TS2454)
```

Out of bounds gives you `undefined`. Integer math that overflows precision gives a
*defined, if imprecise,* result. Reading an unset variable is caught before the code
even runs. Now the same shape in C, where none of those guarantees exist:

```c
#include <stdio.h>
#include <limits.h>

int main(void) {
    int a[3] = {10, 20, 30};

    printf("a[5]   = %d\n", a[5]);     // (1) out-of-bounds read: undefined behavior
    int x = INT_MAX;
    printf("x + 1  = %d\n", x + 1);    // (2) signed overflow: undefined behavior
    int u;
    printf("u      = %d\n", u);        // (3) uninitialized read: indeterminate value
    return 0;
}
```

Compile and run it the naive way and it *appears* to work — it prints three numbers
and exits 0. That is the trap. "It ran" is not "it is correct." Each of those three
lines is a bug the machine had no obligation to catch:

1. `a[5]` reads 8 bytes past a 3-element array — whatever happens to sit on the stack
   there. Today a stale value; tomorrow, after a refactor, a crash.
2. `INT_MAX + 1` is **undefined behavior**, not "wraps to `INT_MIN`." The standard
   says signed overflow can't happen, so the optimizer is *entitled to assume it
   never does* — and will delete the checks you wrote to guard against it.
3. `u` is read before it holds a value. It prints whatever bytes were left on the
   stack by an earlier call.

The point of the atlas is that you don't *guess* about these. You build the tools to
*see* them. Compile the C version with the sanitizers and the invisible becomes loud:

```bash
gcc -Wall -Wextra -fsanitize=address,undefined -g demo.c -o demo && ./demo
```

```text
demo.c:8:29: runtime error: signed integer overflow: 2147483647 + 1 cannot be
             represented in type 'int'
==12345==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x...
    READ of size 4 at 0x... thread T0
        #0 ... in main demo.c:6
```

Same program, same machine — the only thing that changed is that you asked the right
tool to watch. That move, from "it ran" to "I can see exactly what it did," is the
entire discipline of this atlas.

## Why "undefined" is the word that matters

Coming from a managed language, the instinct is to read "undefined behavior" as
"implementation-defined" or "whatever the hardware does." It is neither. **Undefined
behavior is a promise *you* make to the compiler that a thing will never happen.** In
exchange, the compiler optimizes as if it's true. Signed overflow won't happen, so a
loop bound can be proven finite. A pointer you dereferenced can't be null, so the
null check three lines later is dead code and gets deleted. When you break the
promise, the result isn't a wrong number — it's that the compiler's reasoning was
built on a false premise, and *anything* downstream of it is now meaningless. This is
why UB bugs are so slippery: they often work in `-O0`, break in `-O2`, and change
behavior when you add an unrelated `printf`.

## In practice

- **"Works on my machine" is the most dangerous sentence here.** Undefined behavior
  isn't random — it's the *absence of any guarantee*. It can stay invisible for months
  and then surface as data corruption in production after a compiler upgrade.
- **You are now responsible for four things the runtime owned:** memory lifetime (who
  frees, when), bounds (every index, every pointer), integer ranges (overflow,
  truncation, signedness), and initialization (no reading indeterminate bytes).
- **The fix is not fear — it's tooling and discipline.** Sanitizers, `-Wall -Wextra`,
  valgrind, and tests are how you keep moving fast in C without flying blind. That's
  not a postscript to low-level work; it's a [[lowlevel/craftsmanship-low-level/index|first-class part of it]].
- This branch is almost code-free precisely because the rest only makes sense once
  this reset has landed: there is no runtime down here. **You are the runtime now.**

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/craftsmanship-low-level/index|Craftsmanship Low-Level]]

## Sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 1–3 — how programs map to data, registers, and machine code; the foundation for "there is no runtime." csapp.cs.cmu.edu
- **Chris Lattner — *What Every C Programmer Should Know About Undefined Behavior*** (LLVM blog, 3 parts) — why the optimizer treats UB as a premise, with concrete deletions. blog.llvm.org/2011/05/what-every-c-programmer-should-know.html
- **John Regehr — *A Guide to Undefined Behavior in C and C++*** — the canonical tour of what UB is and why it bites. blog.regehr.org/archives/213
- **cppreference — *Undefined behavior*** — the precise list, straight from the standard's model. en.cppreference.com/w/c/language/behavior
- **Charles Petzold — *Code*** — builds the machine from the bottom so "the CPU just executes bytes" stops being abstract.
