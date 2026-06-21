---
title: C from the Metal
description: C for someone who already programs. The dangerous parts — a weak type system, undefined behavior, the preprocessor, array-to-pointer decay — and why "a thin layer over the machine" makes you responsible for everything.
tags: [c, undefined-behavior, types, c-layer]
order: 0
updated: 2026-06-21
---
# C from the Metal

You already know how to program. C doesn't teach you that — it removes the safety
net and hands you the machine. This branch is about the parts of C that surprise an
experienced engineer coming from a managed language: **a type system that barely
checks anything, undefined behavior that the compiler exploits, a textual
preprocessor, arrays that quietly become pointers, and a standard library that
assumes you know what you're doing.**

> C is a thin, leaky abstraction over the hardware. That's its power and its danger:
> there is almost nothing between your source and the instructions, so every
> guarantee you used to lean on is now your job.

## Planned notes

- Why C still matters — a thin layer over the machine
- The C type system is weak (and what that costs you)
- Undefined behavior: the contract you didn't know you signed
- More UB: signed overflow, aliasing, and sequencing
- The preprocessor: macros, includes, conditional compilation
- Translation units, declarations vs definitions, and linkage
- Arrays and array-to-pointer decay
- Structs, unions, and bitfields
- Strings are just `char*` (and the consequences)
- Integer promotions and implicit conversions
- The minimal standard library (libc essentials)
- Build and run a C program from zero (gcc/clang)
- Reading the C standard and cppreference

## Core sources

- **Jens Gustedt — *Modern C*** (free PDF) — the spine; teaches C as it should be written today. gustedt.gitlabpages.inria.fr/modern-c
- **Kernighan & Ritchie — *The C Programming Language* (K&R)** — the canonical, terse original.
- **Robert Seacord — *Effective C*** — modern, security-minded C.
- **Peter van der Linden — *Expert C Programming: Deep C Secrets*** — the gold mine of quirks and UB.
- **Beej's Guide to C** — friendly and free. beej.us/guide/bgc · **cppreference (C)** — the reference. en.cppreference.com/w/c

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]]
