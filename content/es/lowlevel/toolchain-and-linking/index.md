---
title: Toolchain y Linking
description: El pipeline completo — preprocess, compile, assemble, link. Object files, ELF, linking estático vs dinámico, símbolos, gcc vs clang, make/cmake, gdb/lldb, sanitizers y valgrind. La rama de disciplina de build y debugging.
tags: [toolchain, linker, elf, debugging, sanitizers, metal]
order: 0
updated: 2026-06-21
---
# Toolchain y Linking

`gcc main.c` esconde cuatro programas y una montaña chica de convenciones. Esta rama los
abre: **preprocess → compile → assemble → link**, qué contiene un object file, cómo el
linker resuelve símbolos, el formato ELF, linking estático vs dinámico y el loader que
mapea todo eso en un proceso corriendo. Después viene la mitad de disciplina:
**gdb/lldb, sanitizers (ASan/UBSan/TSan) y valgrind** — las herramientas que convierten
"crasheó" en un diagnóstico preciso.

> Linking es donde se encuentran separate compilation, bibliotecas y el loader. Entendelo
> y "undefined reference" / "symbol not found" dejan de ser misterios.

## Notas planificadas

- [[lowlevel/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link|El pipeline: preprocess → compile → assemble → link]]
- [[lowlevel/toolchain-and-linking/object-files-and-whats-inside-them|Object files y qué tienen adentro]]
- [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|El formato ELF (sections, segments, headers)]]
- [[lowlevel/toolchain-and-linking/symbols-definition-reference-and-resolution|Símbolos: definición, referencia y resolución]]
- [[lowlevel/toolchain-and-linking/static-linking-and-archives|Static linking y archives (`.a`)]]
- [[lowlevel/toolchain-and-linking/dynamic-linking-and-shared-libraries|Dynamic linking y shared libraries (`.so`)]]
- [[lowlevel/toolchain-and-linking/the-dynamic-loader-relocation-plt-and-got|El dynamic loader, relocation, PLT y GOT]]
- [[lowlevel/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model|gcc vs clang y el modelo de compiler driver]]
- `make` y el grafo de dependencias de build
- `cmake` (cuando make no alcanza)
- gdb / lldb esenciales para debugging de bajo nivel
- Sanitizers: ASan, UBSan y TSan
- valgrind y profilers (perf, cachegrind)

## Fuentes núcleo

- **John Levine — *Linkers and Loaders*** — la columna de esta rama.
- **Ian Lance Taylor — *Linkers* (serie de 20 partes)** — cómo funciona realmente un linker, desde el autor de gold. lwn.net/Articles/276782
- **Ulrich Drepper — *How To Write Shared Libraries*** — dynamic linking en profundidad. akkadia.org/drepper/dsohowto.pdf
- **David Drysdale — *Beginner's Guide to Linkers*** — la rampa de entrada amable. lurklurk.org/linkers/linkers.html
- **ELF specification**; **docs de sanitizers de Clang** (clang.llvm.org/docs); **manual de GDB** (sourceware.org/gdb).

**Conecta con:** [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/os-from-scratch/index|OS desde Cero]]
