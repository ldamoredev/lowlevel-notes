---
title: Craftsmanship Low-Level
description: My signature, made transversal. How TDD, clean code, and XP translate to C and C++ — testing frameworks, fuzzing, contracts and asserts, defensive programming, and reviewing code that can corrupt memory. SerenityOS as a disciplined-OS case study.
tags: [craftsmanship, tdd, testing, fuzzing, always-on]
order: 0
updated: 2026-06-21
---
# Craftsmanship Low-Level

Software craftsmanship — TDD, clean code, XP — was built in managed languages where
a failing test is a red bar, not a corrupted heap. This always-on branch carries
that discipline down to the metal: **how do you do TDD when a bug can scribble over
memory? What does a unit test look like in C? How do you review code where a missing
`free` is a leak and a stray pointer is a crash?** These are playbooks, not theory —
the habits that keep bare-metal work honest.

> The discipline doesn't get weaker as the language gets sharper — it gets more
> important. Tests, contracts, and sanitizers are how you stay fast in C without
> being reckless.

## Planned playbooks

- TDD in C/C++: the red-green-refactor loop, adapted
- Testing frameworks: Unity (C), Catch2 and GoogleTest (C++)
- Test doubles and seams in C
- Contracts: `assert`, preconditions, and invariants
- Defensive programming (without paranoia)
- Fuzzing with libFuzzer and AFL++
- Sanitizers as part of the test loop
- Code review for memory-unsafe code
- Refactoring legacy C safely (Feathers' techniques)
- Clean code at the bare metal
- Build hygiene: warnings-as-errors and CI for C/C++
- SerenityOS as a disciplined-OS case study

## Core sources

- **James Grenning — *Test-Driven Development for Embedded C*** — the spine; TDD where it's hardest.
- **GoogleTest**, **Catch2**, and **Unity** docs — the C/C++ testing frameworks.
- **Michael Feathers — *Working Effectively with Legacy Code*** — seams and safe refactoring.
- **libFuzzer** (llvm.org/docs/LibFuzzer.html) and **AFL++** (aflplus.plus) — coverage-guided fuzzing.
- **SerenityOS** (github.com/SerenityOS/serenity) — a from-scratch OS built with real engineering discipline.

**Connects to:** [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/modern-cpp/index|Modern C++]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]
