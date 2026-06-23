---
title: "How source becomes execution"
description: The path from a .c file to a running process — preprocess, compile, assemble, link to produce an executable on disk, then load and run to turn that file into a live process. The four build stages, what each tool emits, and how the OS loader brings the program to life.
tags: [machine-model, compilation, linking, loader, toolchain]
order: 11
updated: 2026-06-22
---
# How source becomes execution

A managed language blurred "run my code" into one button. Underneath, getting from text to
a live process is a **pipeline of distinct stages**, each with its own tool, input, and
inspectable output. First, four **build** stages turn source into an executable file on
disk: *preprocess → compile → assemble → link*. Then two **runtime** steps turn that file
into a running process: *load → run*. None of it is magic — you can stop the pipeline at any
stage and look at exactly what it produced. Understanding the stages is what lets you read a
build error, a linker error, and a crash as *different* problems at *different* points.

> The reset: `gcc hello.c` is not one action; it's four tools in a trench coat producing an
> ELF/Mach-O file, which the OS then maps into memory and jumps into. "It compiles" and "it
> links" and "it runs" are three separate victories.

## The four build stages

`gcc` (or `clang`) is a *driver* that runs four programs in sequence. Each flag below stops
the pipeline one stage early so you can see the intermediate:

| Stage | Tool | In → Out | What it does | Stop at |
|---|---|---|---|---|
| Preprocess | `cpp` | `.c` → `.i` | expand `#include`, `#define`, `#if` | `gcc -E` |
| Compile | `cc1` | `.i` → `.s` | translate C to assembly for the target ISA | `gcc -S` |
| Assemble | `as` | `.s` → `.o` | encode assembly into machine-code object | `gcc -c` |
| Link | `ld` | `.o` + libs → exe | resolve symbols, lay out segments | (final) |

Preprocessing is pure text substitution — a 6-line `hello.c` becomes ~560 lines once
`<stdio.h>` is pasted in. Compilation is the heavy stage, where C becomes ISA-specific
[[lowlevel/machine-model/registers-and-the-isa|assembly]]. Assembly is a near-mechanical
encoding into bytes. Linking stitches the pieces into one runnable file.

## Object files have holes; the linker fills them

The output of *assemble* is an **object file** (`.o`): real machine code, but with
**unresolved references** to anything defined elsewhere. Look at the symbols of a `hello.o`
that calls `printf`:

```
0000000000000000 T _answer     <- defined here (Text section)
0000000000000010 T _main       <- defined here
                 U _printf      <- Undefined: a hole to be filled at link time
```

`_answer` and `_main` are **defined** (`T` = text/code); `_printf` is **undefined** (`U`) —
the object file knows it's called but not where it lives. **Linking** resolves every such
hole against other object files and libraries (here, the C standard library), patches in the
addresses, lays out the final segments, and emits a single executable with no holes left.
The mechanics — symbol resolution, static vs dynamic linking, ELF sections, relocation —
are the whole subject of [[lowlevel/toolchain-and-linking/index|toolchain & linking]]; here
the point is just that a `.o` is *incomplete* and the linker completes it.

## From file to process: load → run

An executable on disk is inert. Running it is the OS's job:

- **`exec` and the loader.** When you launch the file, the kernel's loader maps its segments
  into a fresh [[lowlevel/machine-model/stack-vs-heap|address space]] — code (text) and
  globals (data/bss) from the file, plus a fresh stack and heap region.
- **The dynamic linker.** If the program uses shared libraries (`libc.so`), the dynamic
  linker (`ld.so`) loads them and wires up calls (via the PLT/GOT) so `printf` resolves to
  the real library code — lazily, on first call.
- **Startup and `main`.** The loader sets up the stack with `argv`/`environ`, then jumps to
  the C runtime entry point (`_start`), which initializes the runtime and calls `main`. When
  `main` returns, the runtime calls `exit` and the process tears down.

So "load → run" is: map the file into memory, resolve the remaining (dynamic) holes, build
the initial stack, and jump in. The address-space map you saw in the stack-vs-heap note is
exactly what the loader constructs.

## Walk it yourself

```sh
# pipeline.sh — watch one .c file pass through every stage. (gcc or clang)
cat > demo.c <<'EOF'
#include <stdio.h>
int answer(void) { return 42; }
int main(void) { printf("the answer is %d\n", answer()); return 0; }
EOF

gcc -E demo.c | wc -l                 # 1) preprocess: ~560 lines from 3
gcc -S -O2 -masm=intel demo.c         # 2) compile  -> demo.s (human-readable asm)
gcc -c demo.c -o demo.o               # 3) assemble -> demo.o (machine code)
nm demo.o                             #    inspect symbols: _printf shows as 'U'
gcc demo.o -o demo                    # 4) link     -> demo (executable, no holes)
file demo                             #    "ELF 64-bit ... executable" (Mach-O on macOS)
./demo                                # run: the answer is 42
```

Each command stops one stage later than the previous. The revealing moment is `nm demo.o`:
`_printf` is `U` (undefined) in the object file but the program still runs after linking,
because the linker filled that hole. (On Linux you'll see ELF and can dig further with
`readelf`/`objdump`; macOS emits Mach-O and uses `otool` — the *stages* are identical.)

## AOT native vs managed runtimes

This pipeline is **ahead-of-time (AOT)** compilation to native machine code: the work
happens once, at build time, and the CPU runs the result directly. Contrast the managed
floor you came from — Java/Kotlin compile to bytecode that a JVM interprets and **JIT**-
compiles at runtime; JavaScript is JIT-compiled from source on the fly. That's the
[[lowlevel/machine-model/you-are-the-runtime-now|"you are the runtime now"]] shift made
concrete: with C there's no VM between your bytes and the silicon, so the stages above are
the *entire* story of how your code reaches the CPU.

## Failure modes & trade-offs

- **Compile error vs link error are different stages.** A syntax/type error stops at
  *compile*; an *"undefined reference to `foo`"* is a *link* failure — the symbol was
  declared but never defined or its library wasn't linked (`-lm`, etc.). Same-looking
  "it won't build," different fix.
- **Declaration vs definition.** A header declaration satisfies the compiler; the linker
  still needs exactly one definition. Missing it → undefined reference; two of them →
  duplicate symbol.
- **Loader-time failures.** "error while loading shared libraries" / "image not found" is
  neither compile nor link — it's the *dynamic linker* at launch, unable to find a `.so`/
  `.dylib`. Different stage again.
- **Optimization changes the assembly, not the meaning.** `-O2` can reorder, inline, and
  delete code (we relied on this in earlier notes); the observable behavior must match the C
  abstract machine, but the emitted `.s` can look nothing like the source.

## In practice

- **Know which stage failed.** Read the error's vocabulary: parse/type → compile; "undefined
  reference" → link; "cannot open shared object" → load. That alone halves debugging time.
- **Use `-E`, `-S`, `-c` to look inside.** When a macro misbehaves, read `-E` output; when
  you doubt what the compiler emitted, read `-S`; this is everyday low-level practice.
- **Keep declarations (headers) and definitions (one `.o`) straight.** Most beginner link
  errors are a missing definition or an unlinked library, not a code bug.
- **This is the bridge to the toolchain branch.** Everything here — objects, symbols,
  linking, the loader — gets its full treatment in
  [[lowlevel/toolchain-and-linking/index|toolchain & linking]].

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/you-are-the-runtime-now|You are the runtime now]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/registers-and-the-isa|Registers & the ISA]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]]

## Sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 7 — linking, object files, symbol resolution, and loading; the spine for this note. https://csapp.cs.cmu.edu/
- **John R. Levine — *Linkers and Loaders*** — the definitive book on what the linker and loader actually do. https://linker.iecc.com/
- **GCC Manual — "Overall Options" (`-E`, `-S`, `-c`)** — how the driver runs the four stages and where to stop it. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
- **Jens Gustedt — *Modern C*** — translation units, declarations vs definitions, and linkage from the C side. https://gustedt.gitlabpages.inria.fr/modern-c/
- **man pages — `cpp`, `as`, `ld`, `ld.so`, `nm`** — the individual tools behind `gcc`, and how to inspect their output. https://man7.org/linux/man-pages/man1/ld.1.html
