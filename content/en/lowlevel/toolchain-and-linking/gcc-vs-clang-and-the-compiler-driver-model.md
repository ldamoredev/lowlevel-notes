---
title: "gcc vs clang and the compiler driver model"
description: gcc and clang are driver commands: they parse your flags, choose frontend/backend tools, add runtime objects and libraries, and orchestrate compile, assemble, and link phases.
tags: [toolchain, gcc, clang, compiler-driver, frontend, backend]
order: 8
updated: 2026-06-22
---
# gcc vs clang and the compiler driver model

`gcc` and `clang` are the commands you type, but the command is a **driver**. It decides
which phases are needed, selects target defaults, invokes compiler frontends, chooses an
assembler/linker path, and adds runtime objects and libraries for hosted C. GCC and Clang
have different internals and ecosystems, but from the shell they expose the same mental
model: one driver command orchestrates many lower-level tools. When you pass `-v` or `-###`,
the driver stops being quiet.

> The reset: do not picture `gcc main.c` as one compiler blob. Picture a driver reading
> inputs and flags, then producing a concrete pipeline for your target.

## How it really works

GCC is both a compiler collection and a driver interface. For C, the GCC driver normally
invokes preprocessing/compiler components such as `cc1`, an assembler such as `as`, and a
link step through `collect2`/`ld`. Clang is both a GCC-compatible driver and an LLVM-based
compiler frontend; its internal compile step often appears as `clang -cc1`, with LLVM IR and
LLVM code generation behind it. Clang commonly uses an integrated assembler, though it can
also use external tools.

The driver chooses phases from the inputs and options:

| Command shape | Driver work |
|---|---|
| `gcc -E main.c` | preprocess only |
| `gcc -S main.c` | compile to assembly |
| `gcc -c main.c` | compile and assemble to `.o` |
| `gcc main.o helper.o -o app` | link existing objects |
| `clang -target x86_64-unknown-linux-gnu -c x.c` | compile for a specific target triple |

Flags such as `-I`, `-D`, `-O2`, `-g`, `-Wall`, `-Wextra`, `-std=c17`, `-fPIC`, `-shared`,
`-L`, `-l`, and `-Wl,option` belong to different parts of the pipeline. The driver routes
them. `-I` and `-D` affect preprocessing. `-O2`, `-g`, and `-std=` affect compilation.
`-L`, `-l`, and many `-Wl,` flags affect linking. This is why the same command line can mix
language options, codegen options, and linker options without you invoking every tool by
hand.

## Why the driver adds things

A hosted C program needs more than your object files. It needs startup code that enters the
C runtime, arranges `argc`/`argv`/environment state, calls initializers, calls `main`, and
exits correctly. It also needs libc and compiler runtime helpers for operations the target
does not inline directly. The driver knows which startup objects and libraries match the
target. Direct `ld` commands often fail because they skip that knowledge.

This matters for the OS project later. Hosted user-space code should usually let the driver
add defaults. Freestanding kernels do the opposite: use a cross-compiler and flags such as
`-ffreestanding`, `-nostdlib`, and a linker script because the hosted runtime is the wrong
runtime.

## Executable artifact: compare drivers and inspect `-###`

The example lives in
`examples/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model/`. It builds the
same program through `gcc` and `clang`, then asks Clang to print a compile-only phase trace.
On this macOS host, `/usr/bin/gcc` is Apple clang in GCC-compatible-driver clothing, which is
itself a useful platform fact.

```c
#include <stdio.h>

static int twice(int value) {
    return value * 2;
}

int main(void) {
    printf("driver model: %d\n", twice(4));
    return 0;
}
```

Run it:

```bash
cd examples/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model
./run.sh
```

Real output from this machine:

```text
== build with gcc driver ==
driver model: 8
== build with clang driver ==
driver model: 8
== gcc driver identity on this host ==
Apple clang version 15.0.0 (clang-1500.3.9.4)
Target: x86_64-apple-darwin24.6.0
== clang driver phase trace (-###, compile only) ==
"-cc1"
"-triple" "x86_64-apple-macosx14.5.0"
"-emit-obj"
```

`-###` prints the commands the driver would run without actually running them. The excerpt
shows the hidden `-cc1` frontend invocation, the target triple, and the request to emit an
object file. On Linux with real GCC installed, `gcc -v` would show GCC's own `cc1`, `as`,
and linker path instead; the driver model is the same even when the implementation differs.

## Failure modes & trade-offs

- **Assuming `gcc` means GNU GCC everywhere.** On macOS, `/usr/bin/gcc` is commonly Apple
  clang. Always check `--version` when behavior matters.
- **Sending a flag to the wrong phase.** Linker flags often need `-Wl,`; preprocessor flags
  do nothing at link time; `-lfoo` does nothing during `-c`.
- **Calling `ld` directly.** You skip startup objects, default libraries, library search
  decisions, and compiler runtime helpers unless you add them yourself.
- **Relying on perfect GCC/Clang compatibility.** Clang accepts many GCC flags, but warning
  behavior, extensions, sanitizer support, inline assembly details, and diagnostics can
  differ.
- **Cross-target confusion.** The target triple controls ABI, object format, relocation
  types, default include paths, and library expectations.
- **Freestanding vs hosted mismatch.** A kernel build should not accidentally pull in the
  host C runtime; a normal app should not accidentally omit it.

## In practice

- **Use `-v` when defaults matter.** It shows include paths, tool paths, target choices, and
  linker invocation details.
- **Use `-###` with Clang for a clean driver trace.** It is especially useful when teaching
  or debugging phase routing.
- **Prefer the driver for ordinary links.** Let `gcc`/`clang` call the linker unless you are
  deliberately writing a low-level link command.
- **Write portable warnings first.** `-Wall -Wextra -Wpedantic` and a chosen `-std=` help
  make GCC/Clang differences visible early.
- **Be explicit for OS work.** Cross-compiler prefix, target triple, `-ffreestanding`,
  `-nostdlib`, and a linker script are not decoration; they define the environment.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link|The pipeline: preprocess → compile → assemble → link]] · [[lowlevel/toolchain-and-linking/static-linking-and-archives|Static linking and archives]] · [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Why read assembly]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]

## Sources

- **GCC Manual — Overall Options** — `-E`, `-S`, `-c`, `-v`, and driver phase control. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
- **GCC Internals — `collect2`** — why GCC may wrap or augment the final linker invocation. https://gcc.gnu.org/onlinedocs/gccint/Collect2.html
- **Clang command guide** — Clang driver options, target selection, `-###`, and GCC-compatible behavior. https://clang.llvm.org/docs/CommandGuide/clang.html
- **LLVM documentation — Clang design** — frontend/driver/LLVM architecture background. https://clang.llvm.org/docs/DriverInternals.html
- **GNU `ld` manual** — the linker the driver eventually configures on GNU-style systems. https://sourceware.org/binutils/docs/ld/
- **OSDev Wiki — GCC Cross-Compiler** — why freestanding OS work uses an explicit cross-toolchain instead of host defaults. https://wiki.osdev.org/GCC_Cross-Compiler
