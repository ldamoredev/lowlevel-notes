---
title: Start Here
description: The reading path through the Low-Level Atlas — the order that takes you from the machine model to an OS built from scratch without skipping the load-bearing steps.
tags: [orientation, path]
order: 1
updated: 2026-06-21
---
# Start Here

This atlas has one throughline: go from *"I can program"* to *"I understand the
machine."* You can read any note on its own, but the branches are ordered as a
descent and a climb. Here's the path.

## The path

1. **Reset your mental model** — [[lowlevel/machine-model/index|Machine Model]]. How the
   hardware actually works under the runtime. Almost no code; everything else hangs off it.
2. **Descend into C** — [[lowlevel/c-from-the-metal/index|C from the Metal]], then
   [[lowlevel/pointers-and-memory/index|Pointers & Memory]]. The language and the manual
   memory management that make you responsible for everything.
3. **See the metal** — [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler
   Output]] and [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]]. What the
   compiler emits, and the pipeline that builds and debugs it.
4. **(Optional) C++** — [[lowlevel/modern-cpp/index|Modern C++]]. RAII and the rest. Skip
   it on the first pass if you want; the OS is C-first.
5. **Climb back to systems** — [[lowlevel/systems-programming/index|Systems Programming]]
   and [[lowlevel/concurrency-and-performance/index|Concurrency & Performance]]. Talk to
   the kernel; make it fast.
6. **Build the machine** — [[lowlevel/os-from-scratch/index|OS from Scratch]]. The project
   that consumes all of the above.

Running through all of it: [[lowlevel/craftsmanship-low-level/index|Craftsmanship
Low-Level]] — the testing and review discipline that keeps bare-metal work honest.

## How to read it

- Notes are **atomic and source-backed**. Each branch index lists its planned notes and
  the canonical sources behind them.
- The OS target is **x86-64 via cross-compiler + QEMU** — host-agnostic, so it runs the
  same on Intel and Apple Silicon.
- Assembly is **x86-64 primary** with an **ARM64 appendix** per note.

See also: [[lowlevel/must-know|Must Know]] · [[lowlevel/index|Full Index]]
