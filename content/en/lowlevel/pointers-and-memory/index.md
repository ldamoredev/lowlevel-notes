---
title: Pointers & Memory
description: The heart of low-level programming. Stack vs heap, malloc/free, pointer arithmetic, alignment and padding, the classic memory bugs, and custom allocators — managing memory with intention.
tags: [pointers, memory, allocators, heap, c-layer]
order: 0
updated: 2026-06-30
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

- [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is — an address with a type]]
- [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Pointer arithmetic and the type's stride]]
- [[lowlevel/pointers-and-memory/the-stack-frames-locals-and-automatic-storage|The stack: frames, locals, and automatic storage]]
- [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|The heap: `malloc`/`free` and the allocator underneath]]
- [[lowlevel/pointers-and-memory/pointers-to-pointers-double-indirection|Pointers to pointers (double indirection)]]
- [[lowlevel/pointers-and-memory/void-star-and-type-erasure|`void*` and type erasure]]
- [[lowlevel/pointers-and-memory/const-correctness-and-what-const-really-promises|const-correctness and what `const` really promises]]
- [[lowlevel/pointers-and-memory/function-pointers|Function pointers]]
- [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alignment and padding]]
- [[lowlevel/pointers-and-memory/data-layout-and-cache-friendliness|Data layout and cache-friendliness]]
- [[lowlevel/pointers-and-memory/strict-aliasing-and-type-punning|Strict aliasing and type punning]]
- [[lowlevel/pointers-and-memory/use-after-free-and-double-free|Classic bug: use-after-free and double-free]]
- [[lowlevel/pointers-and-memory/buffer-overflow-and-out-of-bounds-access|Classic bug: buffer overflow and out-of-bounds access]]
- [[lowlevel/pointers-and-memory/memory-leaks-and-ownership-discipline|Classic bug: memory leaks and ownership discipline]]
- [[lowlevel/pointers-and-memory/custom-allocators-arena-pool-and-bump|Custom allocators: arena, pool, and bump]]

## Core sources

- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — the memory paper. https://www.akkadia.org/drepper/cpumemory.pdf
- **CS:APP — dynamic memory allocation chapters** — how `malloc` actually works. https://csapp.cs.cmu.edu/
- **Richard Reese — *Understanding and Using C Pointers*** (O'Reilly) — pointers end to end. https://www.oreilly.com/library/view/understanding-and-using/9781449344535/
- **Ryan Fleury — *Untangling Lifetimes: The Arena Allocator*** — arena allocation as a design tool. https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator
- **Doug Lea — *A Memory Allocator (dlmalloc)*** and **Dan Luu** malloc writeups — real allocator internals. https://gee.cs.oswego.edu/dl/html/malloc.html · https://danluu.com/malloc-tutorial/

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]
