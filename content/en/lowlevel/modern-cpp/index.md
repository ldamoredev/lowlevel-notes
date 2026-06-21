---
title: Modern C++
description: The jump from C to C++ — RAII, smart pointers, move semantics, templates, the STL, and zero-cost abstractions. Optional in this atlas because the OS is C-first; SerenityOS is the OS-in-C++ counterexample worth a note.
tags: [cpp, raii, templates, stl, optional]
order: 0
updated: 2026-06-21
---
# Modern C++

> **Optional branch.** The OS-from-scratch project is C-first, so this branch is
> marked optional and deferred: the outline and sources are planned here, but its
> atomic notes are expected *after* the C layer is solid. Expand it when C++ earns
> its place in the work — or treat it as a reference detour.

C++ is not "C with classes" anymore. The reason to learn it at this level is a
single idea — **RAII**: tying resource lifetime to object lifetime so the compiler
frees what you acquired. From there: smart pointers, move semantics, templates, the
STL, and "zero-cost abstractions" that compile down to what you'd have written by
hand in C. This branch frames **which subset of modern C++ is worth using** and where
the abstractions leak.

## Planned notes

- C vs C++: what you actually gain (and pay)
- RAII: the idea that changes everything
- Smart pointers: `unique_ptr`, `shared_ptr`, ownership
- Move semantics and value categories
- References, `const`, and overloading
- Templates and generic programming
- The STL: containers, iterators, algorithms
- Zero-cost abstractions (and where they leak)
- Modern C++ (11 → 23) highlights worth adopting
- Error handling: exceptions vs `expected`
- Which subset of C++ to use (and which to avoid)
- SerenityOS: an operating system written in C++

## Core sources

- **Bjarne Stroustrup — *A Tour of C++*** — the concise, authoritative spine.
- **Scott Meyers — *Effective Modern C++*** — the C++11/14 idioms that matter.
- **C++ Core Guidelines** (Stroustrup & Sutter) — what "good modern C++" means. isocpp.github.io/CppCoreGuidelines
- **Klaus Iglberger — *C++ Software Design*** — design with modern C++.
- **learncpp.com** (free, thorough) and **CppCon talks** (youtube.com/@CppCon).

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/craftsmanship-low-level/index|Craftsmanship Low-Level]]
