---
title: Low-Level Index
description: Every branch of the Low-Level Atlas at a glance, grouped by phase — from the machine model down to the metal and back up to an OS built from scratch.
tags: [index, map]
order: 0
updated: 2026-06-21
---
# Low-Level Index

The whole atlas on one page. Ten branches across seven phases, descending from the
machine model to the metal and climbing back up to a working operating system.

## 01 · Foundations

- [[lowlevel/machine-model/index|Machine Model]] — CPU, memory hierarchy, data representation; "now you are the runtime."

## 02 · The C Layer

- [[lowlevel/c-from-the-metal/index|C from the Metal]] — the weak type system, undefined behavior, a thin layer over the machine.
- [[lowlevel/pointers-and-memory/index|Pointers & Memory]] — stack vs heap, malloc/free, the classic bugs, custom allocators.

## 03 · Down to the Metal

- [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] — read x86-64 (ARM64 appendix) and what your compiler emits.
- [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] — preprocess→compile→assemble→link, ELF, debuggers, sanitizers.

## 04 · C++ (optional)

- [[lowlevel/modern-cpp/index|Modern C++]] — RAII, smart pointers, move semantics, the STL. Optional; the OS is C-first.

## 05 · Systems

- [[lowlevel/systems-programming/index|Systems Programming]] — syscalls, processes, mmap, signals, the POSIX layer.
- [[lowlevel/concurrency-and-performance/index|Concurrency & Performance]] — atomics, memory ordering, lock-free, SIMD, benchmarking.

## 06 · The Project

- [[lowlevel/os-from-scratch/index|OS from Scratch]] — bootloader to shell on x86-64 under QEMU.

## ★ · Always Active

- [[lowlevel/craftsmanship-low-level/index|Craftsmanship Low-Level]] — TDD, fuzzing, contracts, and review for memory-unsafe code.

## Reference

- [[lowlevel/reference-registry|Reference Registry]] — canonical books, specs, and tools cited across the atlas.
