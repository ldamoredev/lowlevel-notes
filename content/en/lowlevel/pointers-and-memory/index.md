---
title: Pointers & Memory
description: The heart of low-level programming. Stack vs heap, malloc/free, pointer arithmetic, alignment and padding, the classic memory bugs, and custom allocators — managing memory with intention.
tags: [pointers, memory, allocators, heap, c-layer]
order: 0
updated: 2026-06-21
---
# Pointers & Memory

This is the branch where low-level either clicks or doesn't. A pointer is just an
address with a type; from that one idea comes everything that makes C fast and
everything that makes it dangerous. We go deep on **the stack and the heap,
`malloc`/`free`, pointer arithmetic, struct layout, the classic bugs
(use-after-free, double-free, leaks, overflows), and writing your own allocators**
so you stop treating memory as infinite and start managing it with intention.

> Memory is not a detail the language hides from you here — it's the material you
> work in. Owning it means knowing who allocates, who frees, and what every byte of
> a struct is doing.

## Planned notes

- What a pointer really is — an address with a type
- Pointer arithmetic and the type's stride
- The stack: frames, locals, and automatic storage
- The heap: `malloc`/`free` and the allocator underneath
- Pointers to pointers (double indirection)
- `void*` and type erasure
- const-correctness and what `const` really promises
- Function pointers
- Struct layout: alignment and padding
- Data layout and cache-friendliness
- Strict aliasing and type punning
- Classic bug: use-after-free and double-free
- Classic bug: buffer overflow and out-of-bounds access
- Classic bug: memory leaks and ownership discipline
- Custom allocators: arena, pool, and bump

## Core sources

- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — the memory paper. akkadia.org/drepper/cpumemory.pdf
- **CS:APP — dynamic memory allocation chapters** — how `malloc` actually works.
- **Richard Reese — *Understanding and Using C Pointers*** (O'Reilly) — pointers end to end.
- **Ryan Fleury — *Untangling Lifetimes: The Arena Allocator*** — arena allocation as a design tool. rfleury.com
- **Doug Lea — *A Memory Allocator (dlmalloc)*** and **Dan Luu** malloc writeups — real allocator internals.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]
