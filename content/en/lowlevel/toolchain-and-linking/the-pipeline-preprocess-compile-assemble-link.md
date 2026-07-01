---
title: "The pipeline: preprocess → compile → assemble → link"
description: "gcc main.c is a driver command, not one magic compiler action: it orchestrates preprocessing, compilation, assembly, and linking, and each stage leaves an inspectable artifact."
tags: [toolchain, gcc, clang, preprocessor, assembler, linker]
order: 1
updated: 2026-06-22
---
# The pipeline: preprocess → compile → assemble → link

You already saw the panorama in
[[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]]:
source becomes an executable through *preprocess → compile → assemble → link*, then the OS
loads and runs it. This note zooms into the build half. The mental model is that
`gcc main.c` is a **driver** command: it chooses and invokes the real tools, passes the
right target defaults, and hides a set of intermediate files you can inspect. Once you can
name the stage, the artifact, and the tool, build failures stop sounding like one generic
"compiler problem."

> The reset: a driver is orchestration. `gcc`/`clang` read your command line, decide which
> phases are needed, then run preprocessing, compilation, assembly, and linking with the
> right defaults for your host or cross target.

## How it really works

The four stages are separate even when the driver streams temporary files between them.
When you ask it to stop early, the hidden artifacts become ordinary files:

| Stage | Usual tool | In → Out | What changes | Stop at |
|---|---|---|---|---|
| Preprocess | `cpp` / driver frontend | `.c` → `.i` | expands `#include`, `#define`, `#if` | `gcc -E` |
| Compile | `cc1` / `clang -cc1` | `.i` → `.s` | lowers C into target assembly | `gcc -S` |
| Assemble | `as` / integrated assembler | `.s` → `.o` | encodes instructions and emits symbols/relocations | `gcc -c` |
| Link | `collect2` → `ld` / linker | `.o` + startup objects + libs → executable | resolves external symbols and lays out the output file | final command |

Preprocessing is still text-level work. It does not know whether `printf("%d", x)` is type
correct; it expands headers, macros, and conditional compilation into the translation unit
that the compiler will parse.

Compilation is where C becomes architecture-specific assembly. The compiler frontend and
optimizer check syntax and types, apply the language rules, choose instructions and
registers, and emit a `.s` file for the target ISA. `-O0` and `-O2` can produce very
different assembly from the same C, but both are still outputs of the compile stage.

Assembly is a near-mechanical encoding step with bookkeeping. The assembler turns
mnemonics such as `call _printf` into machine-code bytes inside a relocatable object file,
then records symbols: definitions this object provides and references that still need a
definition from somewhere else.

Linking combines objects and libraries into the executable file. The linker sees symbol
tables, not C source. It matches undefined references against definitions, reports missing
or duplicate definitions, lays out the final output file, and includes the startup/runtime
pieces the driver requested. Object file internals, ELF, static/dynamic linking, relocation,
and the loader are the next notes in this branch; here the important part is where the link
stage begins and what it is responsible for.

## The driver model

`gcc` is the program you call; it is not the whole compiler pipeline by itself. With GCC,
the driver commonly invokes `cpp`/preprocessing logic, `cc1` for C compilation, `as` for
assembly, then `collect2`, which wraps or invokes `ld` for the final link. With Clang, the
frontend is usually visible as `clang -cc1`, and the assembler may be integrated instead of
a separate `as` process. The stage model is the same.

Use `-v` when you want to see the driver stop hiding the machinery:

```bash
gcc -v demo.c -o demo
gcc -v -c demo.c -o demo.o
```

The output is intentionally host-specific: include search paths, target triple, assembler
path, linker path, startup objects such as `crt1.o`/`Scrt1.o`, and default libraries such as
`libc` or compiler runtime libraries. That is why direct `ld` commands often fail with
strange missing-entry or missing-library errors: the driver normally adds the C runtime
entry objects and default libraries for you. Later, when building a freestanding kernel,
you will deliberately turn some of that off with flags such as `-ffreestanding`,
`-nostdlib`, and a cross-compiler; for hosted user-space C, let the driver do the boring
work.

File suffixes also guide the driver. A `.c` input starts before preprocessing. A `.i` input
is already preprocessed. A `.s` input can go straight to the assembler. A `.o` input skips
to the link step. This is why `gcc main.o helper.o -o app` is still a valid link command
even though no C compilation happens.

## Executable artifact: stop at every stage

The example lives in
`examples/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link/`. Its source
is deliberately small: two functions defined in the object file and one call to libc.

```c
#include <stdio.h>

int build_number(void) {
    return 4;
}

const char *build_label(void) {
    return "pipeline";
}

int main(void) {
    printf("%s stages: %d\n", build_label(), build_number());
    return 0;
}
```

First, verify the normal "one command" build:

```bash
cd examples/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link
gcc -O0 -Wall -Wextra demo.c -o demo_direct
./demo_direct
```

Real output:

```
pipeline stages: 4
```

Now run the scripted pipeline:

```bash
./run.sh
```

Real output from this machine:

```
== 1) preprocess (-E): .c -> .i ==
     578 demo.i
== 2) compile (-S): .i -> .s ==
wrote demo.s
== 3) assemble (-c): .s -> .o ==
0000000000000010 T _build_label
0000000000000000 T _build_number
0000000000000020 T _main
                 U _printf
== 4) link: .o + libc -> executable ==
demo: Mach-O 64-bit executable x86_64
== run ==
pipeline stages: 4
```

The `nm demo.o` section is the key inspection point. `T _build_label`, `T _build_number`,
and `T _main` are text/code symbols defined by this object file. `U _printf` is an
undefined external reference: the object file records the call, but it does not contain
libc's implementation. On Linux/ELF you will usually see symbol names without the leading
underscore and `file demo` will say ELF; on macOS this run produced Mach-O and `nm` prints
the leading underscore. The stages are identical.

Use the platform's object tools at the right depth. `nm` is enough for this note because it
shows defined vs undefined symbols. On Linux, `readelf` and `objdump` are the daily ELF
tools; on macOS, `otool` and `nm` are the Mach-O tools. The next object-file and ELF notes
will use those more deeply instead of pretending all hosts emit the same binary format.

This connects directly to the C-side model in
[[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declarations vs definitions, and linkage]]:
a declaration lets one translation unit compile, but the object file still carries `U`
symbols until another object file or library provides definitions.

## Failure modes & trade-offs

- **Preprocess failures are about text input.** Missing headers, bad include paths, broken
  macro expansions, and `#if` branches that expose invalid tokens fail before the compiler
  can type-check C.
- **Compile failures are about C syntax, types, and language rules.** Parse errors,
  incompatible types, invalid lvalues, and many diagnostics about undefined behavior belong
  here. No object file is produced.
- **Assemble failures are about target assembly.** Bad mnemonics, unsupported instructions,
  wrong target mode, or malformed inline assembly can fail after C compilation but before
  an object file exists.
- **Link failures are about whole-program symbols and libraries.** "Undefined reference,"
  "duplicate symbol," and "multiple definition" mean object files were produced, but the
  linker could not build one coherent executable.
- **Loader failures happen after a successful link.** "Cannot open shared object file" on
  Linux or "Library not loaded" / "image not found" on macOS is the dynamic loader at launch
  time, not the compiler or static linker.
- **The driver trades control for sane defaults.** For normal hosted programs, `gcc demo.c`
  adding startup objects and libc is exactly what you want. For kernels, bootloaders, and
  freestanding code, those defaults become the wrong defaults, so you use a cross-toolchain
  and explicit flags.

## In practice

- **Start by naming the failed stage.** Macro/include noise points to preprocess; syntax and
  type messages point to compile; "undefined reference" points to link; missing shared
  libraries point to load.
- **Use `-E`, `-S`, and `-c` as probes.** `-E` answers "what source did the compiler really
  see?", `-S` answers "what assembly did it emit?", and `-c` answers "what symbols did this
  translation unit provide or require?"
- **Use `nm` before guessing.** If a function is `U` in one object and never `T` in any
  linked object or library, the linker error is not mysterious; the definition is absent
  from the link.
- **Use `gcc -v` when defaults matter.** It shows include paths, tool paths, target
  decisions, startup objects, and libraries. That is often the shortest path from "why did
  this link?" to the actual command line the driver generated.
- **Keep this note at the pipeline boundary.** Object layout, section headers, symbol
  resolution, static archives, shared libraries, PLT/GOT, relocation, and the dynamic
  loader all build on this map, but each deserves its own sharper note.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declarations vs definitions, and linkage]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]]

## Sources

- **GCC Manual — "Overall Options" (`-E`, `-S`, `-c`)** — the driver flags that stop after preprocessing, compilation, or assembly. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
- **GCC Internals — `collect2`** — why GCC may wrap the system linker instead of invoking `ld` as a bare final step. https://gcc.gnu.org/onlinedocs/gccint/Collect2.html
- **GNU binutils — `nm` manual** — the meaning of symbol classes such as `T` and `U` when inspecting object files. https://sourceware.org/binutils/docs/binutils/nm.html
- **GNU binutils — `as` and `ld` manuals** — the assembler and linker tools behind the driver on GNU-style toolchains. https://sourceware.org/binutils/docs/as/ · https://sourceware.org/binutils/docs/ld/
- **David Drysdale — *Beginner's Guide to Linkers*** — a concrete explanation of object files, unresolved symbols, and why linking is separate from compilation. https://www.lurklurk.org/linkers/linkers.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 7 — object files, linking, symbol resolution, and loading from the systems-programming side. https://csapp.cs.cmu.edu/
