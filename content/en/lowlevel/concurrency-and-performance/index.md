---
title: Concurrency & Performance
description: Where low-level pays off. Threads, atomics, memory ordering and models, lock-free, false sharing, cache-aware code, SIMD, serious benchmarking, and data-oriented design — the difference between "it works" and "it flies."
tags: [concurrency, atomics, memory-model, performance, simd]
order: 0
updated: 2026-06-21
---
# Concurrency & Performance

This is where understanding the machine turns into speed. Concurrency forces you to
confront the **memory model** — the rules for what one thread can observe of another
— and performance forces you to respect the **cache hierarchy**, branch prediction,
and the vector units sitting idle in every core. We cover atomics and memory
ordering, lock-free programming and its traps, false sharing, cache-aware and
data-oriented design, SIMD, and how to **benchmark without fooling yourself**.

> "It works" and "it flies" are different engineering problems. The second one is
> mostly about memory access patterns and what the hardware can do in parallel —
> not about clever code.

## Planned notes

- Concurrency vs parallelism
- Threads and shared mutable state
- Data races and what "undefined" means here
- Mutexes, condition variables, and their real cost
- Atomics and the C/C++ memory model
- Memory ordering: acquire, release, seq-cst
- Lock-free programming (and why it's so hard)
- False sharing and cache coherence
- Cache-aware and cache-oblivious code
- SIMD and vectorization
- Data-oriented design
- Benchmarking seriously: pitfalls and methodology
- Profiling for performance (perf, flamegraphs)

## Core sources

- **Anthony Williams — *C++ Concurrency in Action*** — threads, atomics, the memory model, done right.
- **Jeff Preshing — blog** — the clearest writing on memory ordering and lock-free. preshing.com
- **Herlihy & Shavit — *The Art of Multiprocessor Programming*** — the theory of concurrent objects.
- **Agner Fog — optimization manuals** (agner.org/optimize) and **Brendan Gregg — *Systems Performance*** (brendangregg.com).
- **Mike Acton — *Data-Oriented Design* (CppCon 2014)** — the talk that reframes performance. youtube.com/watch?v=rX0ItVEVjHc

**Connects to:** [[lowlevel/systems-programming/index|Systems Programming]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/machine-model/index|Machine Model]]
