---
title: Machine Model
description: How a computer actually works under the runtime — CPU, registers, the memory hierarchy, data representation, and how source code becomes execution. The branch that resets "now you are the runtime."
tags: [machine-model, cpu, memory, foundations]
order: 0
updated: 2026-06-21
---
# Machine Model

For years a runtime caught your mistakes: a garbage collector freed your memory, a
VM normalized your integers, exceptions unwound your stack. This branch removes that
floor. Before any C, assembly, or kernel, you rebuild the mental model of the bare
machine — **the CPU, its registers, the memory hierarchy, and how bits become
behavior** — so that everything later has something concrete to attach to.

> The reset: there is no runtime watching your back anymore. The machine does exactly
> what the bytes say, including the wrong thing, at the speed of a few nanoseconds per
> cache miss. This branch is almost code-free — it's the substrate.

## Planned notes

- [[lowlevel/machine-model/you-are-the-runtime-now|You are the runtime now]] — what the managed floor was hiding
- [[lowlevel/machine-model/the-cpu-fetch-decode-execute|The CPU: fetch–decode–execute and the clock]]
- [[lowlevel/machine-model/registers-and-the-isa|Registers and the instruction set architecture (ISA)]]
- The memory hierarchy: registers → L1/L2/L3 → RAM → disk
- Cache lines, locality, and why a miss costs ~100× a hit
- Stack vs heap: the two memory regions you'll live in
- Bits, bytes, words, and addresses
- Two's complement and integer representation
- Endianness: byte order and why it bites you
- IEEE 754 floating point (and why `0.1 + 0.2 != 0.3`)
- How source becomes execution: compile → load → run
- The process address space and virtual memory (first look)

## Core sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — the spine of this branch. csapp.cs.cmu.edu
- **Charles Petzold — *Code: The Hidden Language of Computer Hardware and Software*** — builds the machine from relays up; pure intuition.
- **Ben Eater — *Build an 8-bit computer from scratch*** — a CPU on breadboards; the deepest mental model. eater.net/8bit
- **Agner Fog — microarchitecture manuals** — advanced detail on how real CPUs execute. agner.org/optimize

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]]
