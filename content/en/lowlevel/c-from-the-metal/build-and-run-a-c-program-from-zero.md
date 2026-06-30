---
title: "Build and run a C program from zero (gcc/clang)"
description: A practical first build line for C: choose a compiler driver, select a standard, enable warnings, produce an executable, run it, and understand what each flag changes before the toolchain branch goes deeper.
tags: [c, build, gcc, clang, compiler, warnings]
order: 12
updated: 2026-06-29
---
# Build and run a C program from zero (gcc/clang)

Running C starts with a compiler driver such as `cc`, `gcc`, or `clang`. The driver reads
source files, runs the build pipeline, links the needed runtime and libraries, and writes
an executable. For a first serious command, be explicit: choose a C standard, choose an
optimization/debug posture, enable warnings, name the output, then run the binary. "It
compiled" means one stage succeeded; "it linked" and "it ran" are separate wins.

> The reset: `gcc demo.c` works, but it hides choices. A deliberate command says which C
> dialect, which warnings, which optimization level, and which output file you meant.

## A useful first command

For a small hosted program:

```bash
cc -std=c17 -O0 -g -Wall -Wextra -pedantic demo.c -o demo
./demo
```

What each piece means:

| Piece | Meaning |
|---|---|
| `cc` | generic C compiler driver; often clang on macOS, gcc or clang elsewhere |
| `-std=c17` | compile as C17, not whatever default the driver picked |
| `-O0` | no optimization; easier debugging and source-level stepping |
| `-g` | emit debug information for lldb/gdb |
| `-Wall -Wextra` | turn on useful warning groups |
| `-pedantic` | warn about non-standard extensions for the selected dialect |
| `demo.c` | input translation unit |
| `-o demo` | output executable name |

Use `gcc` or `clang` directly when you care which compiler is used. Use `cc` in scripts
when you want the host's default C compiler. Add `-Werror` only when the codebase is ready
for warnings to fail the build; it is a good CI policy and a sharp migration tool.

## How it really works

The driver still runs the same stages from
[[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]]:
preprocess, compile, assemble, link. The shortcuts are flags:

| Goal | Command shape |
|---|---|
| Preprocess only | `cc -E demo.c` |
| Compile to assembly | `cc -S demo.c -o demo.s` |
| Compile to object | `cc -c demo.c -o demo.o` |
| Link object to executable | `cc demo.o -o demo` |
| Check syntax/types only | `cc -fsyntax-only demo.c` |

For multiple source files, pass them all or compile separately:

```bash
cc -std=c17 -Wall -Wextra main.c counter.c -o app
cc -std=c17 -Wall -Wextra -c main.c -o main.o
cc -std=c17 -Wall -Wextra -c counter.c -o counter.o
cc main.o counter.o -o app
```

Headers are not compiled by themselves in the normal model. They are included into
translation units. Libraries are linked after objects that need them on many Unix linkers:
`cc main.o -lm -o app` for the math library when needed. Include paths use `-Ipath`;
library search paths use `-Lpath`; specific libraries use `-lname`.

Optimization flags change both performance and debuggability. `-O0 -g` is friendly while
learning. `-O2 -g` is common for optimized debug builds. Sanitizers such as
`-fsanitize=address,undefined` are test-build tools that catch many memory and UB bugs
early, but they are not a replacement for respecting the language contract.

## Executable artifact: one repeatable run script

The demo lives in `examples/c-from-the-metal/build-and-run-a-c-program-from-zero/`. The C
file parses one optional argument and prints the active standard macro.

```c
#include <stdio.h>
#include <stdlib.h>

static long sum_to(long count) {
    long total = 0;

    for (long i = 1; i <= count; i++) {
        total += i;
    }

    return total;
}

int main(int argc, char **argv) {
    long count = 3;

    if (argc > 1) {
        char *end = NULL;
        count = strtol(argv[1], &end, 10);
        if (*end != '\0' || count < 0) {
            fprintf(stderr, "usage: %s [non-negative-count]\n", argv[0]);
            return 2;
        }
    }

#ifdef __STDC_VERSION__
    printf("__STDC_VERSION__      = %ld\n", (long)__STDC_VERSION__);
#else
    printf("__STDC_VERSION__      = pre-C90\n");
#endif
    printf("requested count       = %ld\n", count);
    printf("sum 1..count          = %ld\n", sum_to(count));

    return 0;
}
```

The script makes the build repeatable:

```sh
#!/bin/sh
# run.sh — compile with explicit flags, run the program, then check without linking.
set -e
cd "$(dirname "$0")"
CC="${CC:-cc}"

echo "== compile =="
"$CC" -std=c17 -O0 -g -Wall -Wextra -pedantic demo.c -o demo

echo "== run =="
./demo 4

echo "== syntax-only check =="
"$CC" -std=c17 -Wall -Wextra -pedantic -fsyntax-only demo.c
echo "syntax ok"
```

Run it:

```bash
./run.sh
```

Real output:

```
== compile ==
== run ==
__STDC_VERSION__      = 201710
requested count       = 4
sum 1..count          = 10
== syntax-only check ==
syntax ok
```

`201710` is the C17 value of `__STDC_VERSION__`. The syntax-only pass is useful in editors
and CI when you want fast feedback before producing an object or executable.

## Failure modes & trade-offs

- **Using the compiler default dialect unknowingly.** Defaults change by compiler version.
  Pass `-std=c17`, `-std=c11`, or the dialect you mean.
- **Treating warnings as optional decoration.** C accepts many dangerous programs. Warnings
  are part of the feedback loop, especially for conversions, missing prototypes, and format
  string mistakes.
- **Confusing compile and link failures.** Syntax/type messages come from compilation.
  "Undefined reference" or "duplicate symbol" is linking. Different stage, different fix.
- **Forgetting libraries.** Some functions require extra link flags on some systems, such
  as `-lm` for math on many Unix-like platforms.
- **Debug vs release tension.** `-O0 -g` is easy to debug. `-O2` is closer to production
  behavior and may expose undefined behavior sooner. Test both when bugs are suspicious.

## In practice

- **Start every tiny experiment with a real warning line.** `cc -std=c17 -O0 -g -Wall
  -Wextra -pedantic demo.c -o demo` is a good default.
- **Use scripts once commands have more than one step.** Shell history is not a build
  system. A `run.sh` or Makefile captures intent.
- **Name outputs.** `-o demo` avoids mystery `a.out` files and makes scripts readable.
- **Keep generated files out of git.** Source, headers, scripts, and docs are versioned;
  object files and executables are build artifacts.
- **Graduate to Make when dependencies matter.** Once headers, multiple objects, or
  repeated builds appear, a Makefile is the next honest abstraction.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/the-preprocessor-macros-includes-conditional-compilation|The preprocessor]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declarations, and linkage]] · [[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]]

## Sources

- **GCC manual — Overall Options** — driver flags for stopping at preprocessing, assembly, object generation, or final linking. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
- **GCC manual — C dialect options** — `-std=...`, GNU dialects, and how GCC chooses language modes. https://gcc.gnu.org/onlinedocs/gcc/C-Dialect-Options.html
- **GCC manual — Warning Options** — practical warning flags including `-Wall`, `-Wextra`, `-Wpedantic`, and stricter conversion warnings. https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
- **Clang command guide** — clang driver options, compilation stages, diagnostics, and compatibility with GCC-style flags. https://clang.llvm.org/docs/CommandGuide/clang.html
- **cppreference — C standard revisions** — `__STDC_VERSION__` values and major language revision markers. https://en.cppreference.com/w/c
- **Jens Gustedt — *Modern C*** — practical hosted C build discipline, compiler invocation, and warning-oriented development. https://gustedt.gitlabpages.inria.fr/modern-c/
