---
title: Reference Registry
description: The canonical books, specifications, and tools cited across the Low-Level Atlas, normalized in one place. The starting points, not gospel — verify before relying.
tags: [reference, registry, sources]
order: 3
updated: 2026-06-21
---
# Reference Registry

A normalized registry of the load-bearing references behind the atlas. Per-branch
"Core sources" point here for the canonical items. Prefer primary/canonical sources;
verify a link before relying on it.

## Books (canonical)

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective*** (CS:APP)
- **Jens Gustedt — *Modern C*** (free) · **Kernighan & Ritchie — *The C Programming Language*** (K&R)
- **Peter van der Linden — *Expert C Programming: Deep C Secrets*** · **Robert Seacord — *Effective C***
- **John Levine — *Linkers and Loaders*** · **Michael Kerrisk — *The Linux Programming Interface***
- **Anthony Williams — *C++ Concurrency in Action*** · **Bjarne Stroustrup — *A Tour of C++***
- **James Grenning — *Test-Driven Development for Embedded C*** · **Michael Feathers — *Working Effectively with Legacy Code***
- **OSTEP — *Operating Systems: Three Easy Pieces*** (free) · **xv6 book** (MIT)

## Specifications & references

- **Intel 64 and IA-32 Architectures Software Developer's Manual (SDM)** — x86-64 authority.
- **System V AMD64 ABI** — the x86-64 calling convention.
- **ELF specification** — object/executable format.
- **Felix Cloutier — x86/amd64 instruction reference** (felix.fr/x86) · **cppreference** (C/C++).
- **POSIX / man7.org** — syscalls and the UNIX API.

## Tools

- **Compiler Explorer / godbolt.org** — see compiler output live.
- **QEMU** — emulator for the OS project · **`x86_64-elf` GCC cross-compiler** (OSDev guide).
- **gdb / lldb** — debuggers · **valgrind** — memory/cache analysis · **perf** — profiling.
- **Sanitizers** — ASan / UBSan / TSan (Clang/GCC) · **libFuzzer**, **AFL++** — fuzzing.
- **Unity / Catch2 / GoogleTest** — C/C++ testing frameworks.

## Hubs

- **OSDev Wiki** (wiki.osdev.org) — the OS-development hub.
- **Agner Fog** (agner.org/optimize) — microarchitecture & optimization manuals.
- **Jeff Preshing** (preshing.com) — memory ordering & lock-free.
- ⚠️ **James Molloy kernel tutorial** — has [known bugs](https://wiki.osdev.org/James_Molloy%27s_Known_Bugs); cite only with the warning.

See also: [[lowlevel/index|Full Index]]
