---
title: "Custom allocators: arena, pool & bump"
description: General-purpose `malloc` handles unknown lifetimes and sizes. Custom allocators win when your program knows the lifetime shape: allocate many things together, reuse fixed-size slots, or reset a whole region at once.
tags: [allocators, arena, pool, bump, memory-management, performance]
order: 15
updated: 2026-06-30
---
# Custom allocators: arena, pool & bump

General-purpose `malloc` is a compromise: arbitrary sizes, arbitrary lifetimes, arbitrary
free order, and many callers sharing one heap. A custom allocator narrows the problem.
If your objects die together, use an arena. If your objects all have one size, use a pool.
If allocation is just "move a cursor forward," use a bump allocator. The win is not magic;
it is making lifetime and size constraints explicit.

> The reset: an allocator is a policy about lifetime, layout, and reuse. Performance is a
> consequence of picking the right policy for the data.

## How it really works

A **bump allocator** owns a contiguous buffer and a cursor. Allocation aligns the cursor,
returns the current address, then advances the cursor. Individual frees do not exist.
Resetting the allocator moves the cursor back, invalidating everything allocated from it.
That makes allocation cheap and deallocation nearly free when all objects share a phase
lifetime.

An **arena** is usually a bump allocator packaged as a lifetime region. The region might
use one fixed buffer, chain multiple blocks from `malloc`, or reserve virtual memory and
commit pages on demand. The important design is not the backing store. It is that the user
says, "all of these objects live until the arena resets or is destroyed."

A **pool allocator** owns slots of one fixed size. Free slots are commonly linked together
inside the slots themselves:

```c
struct PoolNode {
    struct PoolNode *next;
};
```

When a slot is free, its bytes store the next pointer. When it is allocated, the user's
object occupies those bytes. Allocation pops one node from the free list. Free pushes it
back. This gives predictable `O(1)` operations and avoids external fragmentation for that
object size.

All custom allocators must still respect C's object rules. Returned addresses need proper
alignment for the requested type. Storage must be large enough. Resetting an arena ends
the practical lifetime of everything inside it, even if stale pointers still contain old
addresses. A pool double-free can corrupt the free list just as surely as a double-free
can corrupt `malloc` metadata.

## Executable artifact: tiny arena and fixed-size pool

The demo lives in `examples/pointers-and-memory/custom-allocators-arena-pool-and-bump/demo.c`.

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Real output:

```
arena used            = 24 bytes
arena values          = 10 20 30 9.5
arena after reset     = 0 bytes
pool values           = 1 2 3
pool exhausted        = yes
pool reused slot      = 22
```

The arena allocates three `int` values and one `double`, paying padding to keep the
`double` aligned. Resetting it invalidates the allocations in one operation. The pool has
three fixed-size slots; the fourth allocation fails, freeing one slot makes it reusable,
and the allocator never asks the general heap for another block.

## Failure modes & trade-offs

- **Dangling after arena reset.** Reset is a mass free. Every pointer into the arena is
  stale afterward.
- **Wrong lifetime grouping.** Arenas leak by design until reset. If one object must live
  much longer than the phase, it belongs elsewhere.
- **Alignment bugs.** A cursor that ignores `_Alignof(T)` can return addresses that are
  invalid or slow to dereference.
- **Pool double-free.** Pushing the same slot twice can create cycles or duplicate
  allocation of one slot.
- **Fixed-size waste.** Pools are fast for one size class, but large slots waste memory
  when most objects are small.
- **Thread sharing.** A simple arena or pool is usually not thread-safe. Adding locks can
  erase the win or change contention patterns.
- **Cleanup for contained resources.** Resetting raw memory does not close file
  descriptors, release nested heap allocations, or run custom destructors.

## In practice

- **Use arenas for phase lifetimes.** Parsing a file, building one request, compiling one
  unit, or simulating one frame are natural shapes.
- **Use pools for fixed-size churn.** Packets, AST nodes of one class, jobs, particles, and
  small command objects often fit.
- **Keep the general heap as the baseline.** Replace `malloc` only where lifetime or
  profiling shows a real reason.
- **Make reset points explicit.** The call site should make it obvious when all arena
  pointers become invalid.
- **Store capacity and used bytes.** Custom allocators need bounds checks as much as any
  other buffer code.
- **Instrument first versions.** Add counters, high-water marks, and asserts before making
  the allocator clever.

**Connects to:** [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|The heap: `malloc`/`free` and the allocator underneath]] · [[lowlevel/pointers-and-memory/memory-leaks-and-ownership-discipline|Memory leaks & ownership discipline]] · [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alignment & padding]] · [[lowlevel/pointers-and-memory/data-layout-and-cache-friendliness|Data layout & cache-friendliness]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **Ryan Fleury — *Untangling Lifetimes: The Arena Allocator*** — clear explanation of arena allocation as lifetime design, not just speed trick. https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator
- **Doug Lea — *A Memory Allocator*** — allocator design pressures behind `dlmalloc`: fragmentation, bins, locality, and trade-offs. https://gee.cs.oswego.edu/dl/html/malloc.html
- **Dan Luu — *Malloc tutorial*** — small allocator walkthrough that makes heap metadata and free lists concrete. https://danluu.com/malloc-tutorial/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — dynamic memory allocation and implicit/explicit free-list allocators. https://csapp.cs.cmu.edu/
- **cppreference — `aligned_alloc`** — C allocation API reference for alignment-sensitive storage. https://en.cppreference.com/w/c/memory/aligned_alloc
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — locality, cache behavior, and why allocator layout affects performance. https://www.akkadia.org/drepper/cpumemory.pdf
