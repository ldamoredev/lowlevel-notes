---
title: "The preprocessor: macros, includes, conditional compilation"
description: The C preprocessor runs before the compiler: it includes files, expands macros, and removes or keeps code with conditional compilation. It is powerful because it rewrites tokens, and dangerous for the same reason.
tags: [c, preprocessor, macros, includes, conditional-compilation]
order: 5
updated: 2026-06-22
---
# The preprocessor: macros, includes, conditional compilation

The C preprocessor is a token-rewriting phase that runs before the compiler proper. It
does not know types, scopes, lifetimes, or expressions the way the C compiler does; it
knows directives like `#include`, `#define`, `#if`, and textual tokens. That makes it
essential for C's build model: headers are pasted into translation units, constants and
small generic idioms are expressed as macros, and platform or feature-specific code is
selected before parsing. The cost is that mistakes happen before the type checker can
help you.

> The reset: macros are not functions, includes are not imports, and `#if` is not a
> runtime branch. The preprocessor edits the source stream the compiler will later see.

## The phase before C sees C

Translation starts with preprocessing. Comments disappear, backslash-newline splices are
handled, directives run, macros expand, and `#include` pulls other files into the current
translation unit. Only after that does the compiler parse declarations, types, statements,
and expressions.

| Preprocessor feature | What it does | What it does not do |
|---|---|---|
| `#include` | pastes another file's tokens here | create a module boundary |
| object-like macro | replaces a name with tokens | create a typed constant |
| function-like macro | replaces a call-shaped token sequence | evaluate arguments once like a function |
| `#if` / `#ifdef` | keeps or removes code before compilation | run a runtime condition |
| `#` / `##` | stringifies or pastes tokens | inspect values or types |

This explains why C headers matter so much. A header is not "loaded" at runtime. It is
inserted into each source file that includes it, usually guarded by `#ifndef` or
`#pragma once` so declarations are not repeated inside one translation unit. Later notes
on declarations, definitions, and linkage will build on this: the preprocessor assembles
the text; the compiler and linker enforce the C rules.

## How it really works

A macro is a token replacement rule. `#define BUFFER_CAP 4` says that later tokens named
`BUFFER_CAP` expand to the token `4`. A function-like macro such as
`#define MAX(a, b) ((a) > (b) ? (a) : (b))` substitutes the argument tokens into the
replacement list. It does not call a function, type-check arguments, or guarantee each
argument is evaluated once.

That is why defensive macro style exists:

- **Parenthesize parameters and the whole expression.** `SQUARE(x)` should be
  `((x) * (x))`, not `x * x`, or precedence will bite callers.
- **Never pass side effects to macros that may use an argument more than once.** `MAX(i++,
  j++)` can increment more than you meant.
- **Prefer `static inline` functions when types are known.** They obey C scoping and
  evaluation rules, and modern compilers inline them.
- **Use macros for things C cannot express directly.** Conditional compilation, compile-time
  feature flags, `sizeof`-based generic idioms, stringification, token pasting, and
  declarations that must vary by platform.

Conditional compilation is equally literal. If `ENABLE_TRACE` is `0`, a `#if
ENABLE_TRACE` block is not parsed as C at all. That is useful for kernels, embedded
targets, and portability layers where a symbol, register, syscall, or header may not exist
on every target. It is also dangerous: code hidden behind a rarely used `#if` can silently
rot until that build configuration is tested.

## Executable artifact: same source, different tokens

This demo uses `#include`, defaulted macros, stringification, a size macro, and conditional
compilation. The base build compiles trace code out. A second build passes `-D` definitions
on the command line, changing the token stream before the compiler sees it.

```c
// demo.c — preprocessor: includes, macros y compilacion condicional.
// gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>

#ifndef BUFFER_CAP
#define BUFFER_CAP 4
#endif

#ifndef ENABLE_TRACE
#define ENABLE_TRACE 0
#endif

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#if ENABLE_TRACE
#define TRACE(message) printf("trace: %s\n", (message))
#else
#define TRACE(message) ((void)0)
#endif

static int clamp_to_cap(int value) {
    return value > BUFFER_CAP ? BUFFER_CAP : value;
}

int main(void) {
    int samples[BUFFER_CAP] = {0};

    for (size_t i = 0; i < ARRAY_LEN(samples); i++) {
        samples[i] = (int)(i + 1) * 10;
    }

    printf("BUFFER_CAP value       = %d\n", BUFFER_CAP);
    printf("BUFFER_CAP as text     = %s\n", STRINGIFY(BUFFER_CAP));
    printf("ARRAY_LEN(samples)     = %zu\n", ARRAY_LEN(samples));
    printf("MAX(3, 7)              = %d\n", MAX(3, 7));
    printf("clamp_to_cap(9)        = %d\n", clamp_to_cap(9));

    TRACE("compiled only when ENABLE_TRACE is nonzero");

#if ENABLE_TRACE
    printf("trace mode             = compiled in\n");
#else
    printf("trace mode             = compiled out\n");
#endif

    return 0;
}
```

Compile and run the default:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
```

Real output:

```
BUFFER_CAP value       = 4
BUFFER_CAP as text     = 4
ARRAY_LEN(samples)     = 4
MAX(3, 7)              = 7
clamp_to_cap(9)        = 4
trace mode             = compiled out
```

Now compile with definitions supplied by the build command:

```bash
gcc -O0 -Wall -Wextra -DENABLE_TRACE=1 -DBUFFER_CAP=6 demo.c -o demo && ./demo
```

Real output:

```
BUFFER_CAP value       = 6
BUFFER_CAP as text     = 6
ARRAY_LEN(samples)     = 6
MAX(3, 7)              = 7
clamp_to_cap(9)        = 6
trace: compiled only when ENABLE_TRACE is nonzero
trace mode             = compiled in
```

The C code did not read an environment variable. The build command changed the program
before compilation. `BUFFER_CAP` became different tokens, `TRACE` became either a `printf`
or `((void)0)`, and one side of the `#if` never reached the parser.

## Failure modes & trade-offs

- **Double evaluation.** `MAX(i++, j++)` can increment the chosen argument twice and the
  other once. Function-like macros substitute tokens; they do not evaluate like functions.
- **Precedence bugs.** `#define SQUARE(x) x * x` makes `SQUARE(1 + 2)` expand to `1 + 2 *
  1 + 2`. Parenthesize parameters and the whole replacement expression.
- **Scope surprises.** Macros ignore C scope after definition. A macro name can rewrite
  later code until `#undef` or end of translation unit.
- **Stale conditional branches.** Code behind `#if TARGET_X` may not even parse on other
  builds. Every supported configuration needs a build.
- **Header duplication.** Headers are pasted text. Without include guards, repeated
  declarations may be fine, but repeated definitions can break a translation unit.
- **Debugging the wrong source.** Compiler errors after macro expansion can point into code
  you did not write literally. Use `gcc -E` or `clang -E` to inspect preprocessed output.

## In practice

- **Use include guards in every header.** The classic shape is `#ifndef NAME_H`, `#define
  NAME_H`, declarations, `#endif`.
- **Prefer `enum`, `const` objects, or `static inline` functions when they fit.** Reach for
  macros when you need preprocessing, not habit.
- **Keep macro arguments boring.** Pass variables and values, not `i++`, function calls
  with side effects, or expressions whose evaluation count matters.
- **Name feature flags centrally.** Build systems should own `-D` flags; source should
  document defaults with `#ifndef`.
- **Inspect expansion when confused.** `gcc -E demo.c` shows the actual token stream going
  to the compiler. That is often the shortest path out of macro fog.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/why-c-still-matters|Why C still matters]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|The C type system is weak]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: the contract]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for preprocessing phases, directives, macro replacement, conditional inclusion, and translation units. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Preprocessor** — compact reference for directives, macro replacement, conditional inclusion, stringification, and token pasting. https://en.cppreference.com/w/c/preprocessor
- **GCC — Options Controlling the Preprocessor** — practical reference for `-D`, `-U`, `-I`, `-E`, include paths, and preprocessing diagnostics. https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html
- **GCC — The C Preprocessor manual** — detailed behavior of macro expansion, include guards, conditionals, and common pitfalls. https://gcc.gnu.org/onlinedocs/cpp/
- **Jens Gustedt — *Modern C*** — modern guidance on macros, `_Generic`, inline functions, headers, and avoiding preprocessor abuse. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Beej's Guide to C — The C Preprocessor** — friendly examples of `#include`, `#define`, `#ifdef`, and the build-command flags that drive them. https://beej.us/guide/bgc/html/split/the-c-preprocessor.html
