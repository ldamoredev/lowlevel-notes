---
title: "Cache lines & locality"
description: The cache moves memory in 64-byte lines, not bytes — so a cache miss costs ~100× a hit, and the same loop can run 25× slower just by changing the order you touch memory. What a cache line is, why a miss is so expensive, and a C benchmark that proves it.
tags: [machine-model, cache, cache-line, locality, performance]
order: 5
updated: 2026-06-22
---
# Cache lines & locality

The [[lowlevel/machine-model/the-memory-hierarchy|memory hierarchy]] told you *that*
caches sit between the CPU and RAM. This note is about the single mechanical fact that
makes them work — and makes them bite. The cache never moves one byte. It moves a fixed
block called a **cache line** (64 bytes on x86-64), always. Read one `int` and you pull
its 63 neighbors along for free; touch memory in an order that ignores those lines and
you pay full RAM latency on nearly every access. Because a miss costs roughly **100× a
hit**, the *order* in which you walk memory can change a program's speed by 10–25×
without changing a single operation it performs.

> The reset: you cannot ask the cache to hold "this variable." You get lines. Fast code
> is code whose access pattern uses every byte of each line it pays for, before that
> line is evicted. That's the whole game.

## The cache line is the unit of transfer

When the CPU needs an address that isn't in cache, it does **not** fetch that byte. It
fetches the entire aligned 64-byte line containing it from the next level down, and
installs that line in cache. Consequences:

- **Neighbors are free.** After reading `a[0]`, the bytes for `a[1]..a[15]` (an
  `int[16]` line) are already cached. Sequential access gets 15 hits for the price of
  one miss.
- **A line is the granularity of everything.** Eviction, coherence between cores, and
  "is this cached?" all happen per line, never per byte.
- **Misaligned/scattered access wastes the line.** If you use only 4 of the 64 bytes
  before the line is evicted, you paid a full miss to transport 60 bytes you threw away.

A cache is **set-associative**: each line maps to a set of a few slots; the hardware
keeps a **tag** per slot to know which memory address currently lives there, and evicts
(usually) the least-recently-used line when a set fills. You don't manage any of this —
but knowing the line is 64 bytes is enough to reason about almost every cache effect.
(Line size is an ISA/microarchitecture detail: 64 bytes on x86-64 and on most ARM, but
**128 bytes on Apple Silicon** — check with `sysctl hw.cachelinesize` on macOS or
`getconf LEVEL1_DCACHE_LINESIZE` on Linux.)

## Why a miss costs ~100× a hit

The number comes straight from the hierarchy's latencies:

| Outcome | Where the data is | Approx. latency |
|---|---|---|
| L1 hit | already in L1 | ~4 cycles (~1 ns) |
| L2 hit | one level down | ~12 cycles (~4 ns) |
| LLC (L3) hit | shared cache | ~40 cycles (~15 ns) |
| **Miss → RAM** | main memory | **~200–300 cycles (~100 ns)** |

An L1 hit feeds the pipeline in a cycle or two; a full miss stalls for ~100 ns — on a
3 GHz core that's **~300 instructions' worth of time** the CPU could have been
executing. Out-of-order execution and prefetching hide *some* of this when the access
pattern is predictable (sequential streaming), which is exactly why the prefetcher
made the random-vs-sequential gap so large in the previous note. But a dependent miss
the CPU can't predict — a pointer chase, a random index — exposes the full ~100×.

## Same work, 25× slower: row vs column

Nothing demonstrates cache lines better than traversing a matrix two ways. Both loops
read every element exactly once and sum them — identical operation count. Only the
*order* differs. Build and run:

```c
// cache_lines.c — identical work, opposite traversal orders.
// gcc -O2 -Wall -Wextra cache_lines.c -o cache_lines && ./cache_lines
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(void) {
    const int N = 8192;                       // 8192 x 8192 ints = 256 MB
    int *m = malloc((size_t)N * N * sizeof(int));
    if (!m) { perror("malloc"); return 1; }
    for (long i = 0; i < (long)N * N; i++) m[i] = 1;

    volatile long sink = 0;
    double t0 = now_ms();
    long s = 0;
    for (int i = 0; i < N; i++)               // row-major: stride 1, walks each line
        for (int j = 0; j < N; j++) s += m[(long)i * N + j];
    double t1 = now_ms(); sink = s;

    s = 0;
    for (int j = 0; j < N; j++)               // column-major: stride N, new line each step
        for (int i = 0; i < N; i++) s += m[(long)i * N + j];
    double t2 = now_ms(); sink = s;

    printf("row-major  (stride 1) : %8.1f ms\n", t1 - t0);
    printf("col-major  (stride N) : %8.1f ms\n", t2 - t1);
    printf("slowdown              : %8.1fx\n", (t2 - t1) / (t1 - t0));
    return sink == -1;
}
```

A warm run on an Intel Core i7 laptop: **row-major ~35 ms, column-major ~870 ms —
about 25× slower**, same additions. Row-major marches along memory, so each 64-byte
line is loaded once and all 16 `int`s in it are used before moving on. Column-major
jumps `N` ints (32 KB) every step, so each access lands on a *fresh* line, uses 4 of
its 64 bytes, and is evicted long before you come back to it — essentially a miss per
element. Transpose your loops (or your data) and you reclaim the order of magnitude.

## Locality is a property you design for

You can't add locality after the fact; it's baked into your data layout and traversal:

- **Array of structs (AoS) vs struct of arrays (SoA).** Looping over `particle[i].x`
  for a million particles wastes most of each line on the `y`, `z`, `mass`… you didn't
  read. Splitting hot fields into their own arrays (SoA) packs them line-dense. This is
  the heart of *data-oriented design*.
- **Hot/cold splitting.** Keep frequently-touched fields together and push rarely-used
  fields to a separate "cold" struct, so the hot path fills lines with useful data.
- **Pack and align.** A struct's layout, padding, and alignment decide how many fit per
  line — covered when we reach [[lowlevel/pointers-and-memory/index|pointers & memory]].

## Failure modes & trade-offs

- **Big-O can lie.** A linked list and an array both "iterate in O(n)," but the array is
  often 10×+ faster because it streams contiguous lines while the list chases pointers to
  cold, scattered nodes — a miss each hop.
- **Large strides defeat the line.** Any stride ≥ the line size (16 `int`s) means one
  useful element per line. Column-major access, hash buckets, and naive matrix transpose
  all hit this.
- **False sharing (a multicore trap).** Two cores writing *different* variables that
  happen to share one cache line ping-pong the line between their caches, serializing
  what looked parallel. The fix is padding hot per-thread data to its own line —
  revisited in concurrency.
- **It's invisible in the source.** Two loops with identical C can differ 25× at
  runtime. Only a profiler's cache-miss counters (or a benchmark like the one above)
  reveals it.

## In practice

- **Walk memory the way it's laid out.** For C's row-major arrays, make the *last* index
  the inner loop. This one rule fixes a huge fraction of accidental slowdowns.
- **Prefer contiguous, line-dense data.** Arrays of small structs walked in order beat
  pointer-linked graphs whenever you can arrange it.
- **Measure misses, not just time.** `perf stat -e cache-misses,cache-references ./prog`
  on Linux, or Instruments/`Cachegrind`, tells you whether you're line-bound.
- **Think in 64-byte chunks.** When you read a field, you paid for its whole line —
  design so the rest of that line is data you also want.

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/the-memory-hierarchy|The memory hierarchy]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|The CPU: fetch–decode–execute]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 6 — cache organization, lines, set-associativity, and locality; the spine for this note. https://csapp.cs.cmu.edu/
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — cache lines, associativity, and access patterns measured in depth. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- **Igor Ostrovsky — *Gallery of Processor Cache Effects*** — short, runnable experiments that expose line size, associativity, and false sharing. https://igoro.com/archive/gallery-of-processor-cache-effects/
- **Mike Acton — *Data-Oriented Design and C++* (CppCon 2014)** — why data layout, not abstraction, drives real performance. https://www.youtube.com/watch?v=rX0ItVEVjHc
- **Intel 64 and IA-32 Architectures Optimization Reference Manual** — authoritative cache-line size, prefetch behavior, and false-sharing guidance on x86-64. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
