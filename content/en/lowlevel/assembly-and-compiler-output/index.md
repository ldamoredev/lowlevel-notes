---
title: Assembly & Compiler Output
description: Read and understand x86-64 (with an ARM64 appendix). Registers, instructions, stack frames, calling conventions, and Compiler Explorer as the daily tool for seeing exactly what your C and C++ emit.
tags: [assembly, x86-64, arm64, compiler, metal]
order: 0
updated: 2026-06-21
---
# Assembly & Compiler Output

You don't need to *write* much assembly to benefit enormously from *reading* it. The
goal of this branch is fluency: open [Compiler Explorer](https://godbolt.org), paste
your C or C++, and understand the instructions that come out. We focus on **x86-64**
(it matches the OS target, the literature, and Godbolt's defaults), cover registers,
the core instruction set, stack frames and the System V calling convention, and add
an **ARM64 appendix** per topic — because that's your daily hardware going forward.

> The compiler is not a black box. Once you can read its output, "is this fast?" and
> "what is this code actually doing?" stop being guesses.

## Planned notes

- [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Why read assembly — Compiler Explorer as a daily tool]]
- [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|x86-64 registers and the register file]]
- [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|The core instruction set: `mov`, arithmetic, `lea`]]
- [[lowlevel/assembly-and-compiler-output/addressing-modes-and-memory-operands|Addressing modes and memory operands]]
- Control flow: jumps, conditions, and the flags register
- The stack at the assembly level (`push`/`pop`, `rsp`/`rbp`)
- Stack frames: function prologue and epilogue
- The System V AMD64 calling convention
- How C constructs compile: loops, structs, `switch`
- Optimization: what `-O2` does to your code
- Inline assembly (and when to reach for it)
- ARM64 appendix: AArch64 registers and calling convention

## Core sources

- **Compiler Explorer (godbolt.org)** — the daily tool; see what any compiler emits. godbolt.org
- **Jonathan Bartlett — *Programming from the Ground Up*** (free) — assembly from first principles.
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** (free PDF).
- **Felix Cloutier — x86 and amd64 instruction reference** — the quick lookup. felix.fr/x86 (and Intel SDM for the authority).
- **ARM64:** Stephen Smith — *Programming with 64-Bit ARM Assembly*; Apple's ARM64 / Apple Silicon assembly docs.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/machine-model/index|Machine Model]]
