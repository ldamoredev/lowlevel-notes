---
title: Toolchain & Linking
description: The full pipeline — preprocess, compile, assemble, link. Object files, ELF, static vs dynamic linking, symbols, gcc vs clang, make/cmake, gdb/lldb, sanitizers, and valgrind. The branch of build and debugging discipline.
tags: [toolchain, linker, elf, debugging, sanitizers, metal]
order: 0
updated: 2026-06-21
---
# Toolchain & Linking

`gcc main.c` hides four programs and a small mountain of conventions. This branch
opens them up: **preprocess → compile → assemble → link**, what an object file
contains, how the linker resolves symbols, the ELF format, static vs dynamic
linking, and the loader that maps it all into a running process. Then the discipline
half: **gdb/lldb, sanitizers (ASan/UBSan/TSan), and valgrind** — the tools that turn
"it crashed" into a precise diagnosis.

> Linking is where separate compilation, libraries, and the loader meet. Understand
> it and "undefined reference" / "symbol not found" stop being mysteries.

## Planned notes

- [[lowlevel/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link|The pipeline: preprocess → compile → assemble → link]]
- [[lowlevel/toolchain-and-linking/object-files-and-whats-inside-them|Object files and what's inside them]]
- [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|The ELF format (sections, segments, headers)]]
- [[lowlevel/toolchain-and-linking/symbols-definition-reference-and-resolution|Symbols: definition, reference, and resolution]]
- Static linking and archives (`.a`)
- Dynamic linking and shared libraries (`.so`)
- The dynamic loader, relocation, PLT and GOT
- gcc vs clang and the compiler driver model
- `make` and the build dependency graph
- `cmake` (when make isn't enough)
- gdb / lldb essentials for low-level debugging
- Sanitizers: ASan, UBSan, and TSan
- valgrind and profilers (perf, cachegrind)

## Core sources

- **John Levine — *Linkers and Loaders*** — the spine of this branch.
- **Ian Lance Taylor — *Linkers* (20-part series)** — how a linker really works, from the gold author. lwn.net/Articles/276782
- **Ulrich Drepper — *How To Write Shared Libraries*** — dynamic linking in depth. akkadia.org/drepper/dsohowto.pdf
- **David Drysdale — *Beginner's Guide to Linkers*** — the gentle on-ramp. lurklurk.org/linkers/linkers.html
- **ELF specification**; **Clang sanitizer docs** (clang.llvm.org/docs); **GDB manual** (sourceware.org/gdb).

**Connects to:** [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]
