---
title: "The memory hierarchy"
description: Memory is not one flat thing — it's a pyramid of stores trading size for speed, from registers to L1/L2/L3 cache to RAM to disk. Why the gap exists, the latency numbers that matter, and a C benchmark you can run to watch the cliff.
tags: [machine-model, memory, cache, latency, performance]
order: 4
updated: 2026-06-22
---
# The memory hierarchy

When you write `x + y`, a managed language lets you imagine one uniform "memory" the
CPU reaches into at a constant cost. That picture is a lie the hardware works very hard
to maintain. Real storage is a **pyramid**: a few registers that are effectively free,
then progressively larger and slower stores — L1, L2, L3 cache, main RAM, and finally
disk. Each level down is roughly an order of magnitude bigger and an order of magnitude
slower. The whole machine is engineered around one bet: that the data you touch next is
probably near the data you just touched, so a small fast store can stand in for a huge
slow one most of the time.

> The reset: "reading a variable" is not one operation with one cost. The same line of
> C can take 4 cycles or 300 depending on *where the value currently lives*, and you
> usually can't see that in the source. Performance work is largely the art of keeping
> data high in this pyramid.

## Why a hierarchy exists at all

Fast memory is expensive and physically small; large memory is cheap and far away.
You cannot have one store that is simultaneously huge, fast, and affordable — the
[speed of light and transistor economics forbid it](https://www.intel.com). So
hardware fakes it by stacking several stores and **automatically moving hot data
upward**. Registers are decided by the compiler; the caches are managed transparently
by the CPU; RAM↔disk movement is managed by the OS (paging). You mostly don't control
the caches directly — you influence them by *how you access memory*.

| Level | Typical latency | Typical size | Managed by |
|---|---|---|---|
| Register | ~0 cycles (it *is* the operand) | ~16 GPRs × 8 B | Compiler |
| L1 cache | ~4 cycles (~1 ns) | 32–64 KB per core | Hardware |
| L2 cache | ~12 cycles (~4 ns) | 256 KB–2 MB per core | Hardware |
| L3 cache | ~40 cycles (~15 ns) | 8–64 MB shared | Hardware |
| RAM (DRAM) | ~100–300 cycles (~60–100 ns) | 8–128 GB | OS (virtual memory) |
| SSD | ~100 µs | 0.25–4 TB | OS (filesystem) |
| Spinning disk | ~10 ms | TBs | OS (filesystem) |

The numbers vary by chip, but the **ratios** are the lesson. An L1 hit to a RAM access
is roughly a **1:100** difference; RAM to SSD is another ~1000×; SSD to a hard disk
another ~100×. If a register access were one second, an L1 hit would be a few seconds,
RAM would be a couple of minutes, an SSD read would be a day, and a hard-disk seek
would be months. That mental scale is why "it's just memory" is never just memory.

## Locality: the bet that makes it work

Caches pay off only because real programs exhibit **locality of reference**:

- **Temporal locality** — if you touched an address, you'll likely touch it again soon
  (a loop counter, a hot struct field). Keep it cached and reuse wins.
- **Spatial locality** — if you touched an address, you'll likely touch its neighbors
  soon (the next array element). So caches don't move single bytes; they move a whole
  **cache line** (64 bytes on x86-64; 128 on Apple Silicon) at once.

This is why *how* you walk memory dominates performance. Striding through an array in
order uses every byte of each fetched line before moving on — near-perfect spatial
locality. Jumping around randomly throws most of each line away and pays a fresh
high-latency miss almost every access. Same data, same total bytes, wildly different
speed. (The mechanics of cache lines and the cost of a miss get their own note next.)

## Watch the cliff: a runnable benchmark

This program times a fixed number of accesses against a working set that grows from a
few KB to tens of MB. While the set fits in L1, accesses are fast; as it spills into
L2, L3, then RAM, the per-access time steps up at each boundary. Build and run:

```c
// hierarchy.c — observe per-access latency grow as the working set leaves each cache.
// gcc -O2 -Wall -Wextra hierarchy.c -o hierarchy && ./hierarchy
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    const long ACCESSES = 256L * 1024 * 1024;   // fixed work per size
    volatile int sink = 0;
    printf("%10s  %12s\n", "size(KB)", "ns/access");

    for (long kb = 4; kb <= 64L * 1024; kb *= 2) {
        long n = kb * 1024 / (long)sizeof(int);
        int *a = malloc((size_t)n * sizeof(int));
        if (!a) { perror("malloc"); return 1; }

        // Build ONE big random cycle (Sattolo's algorithm). A *random* order is what
        // defeats the hardware prefetcher, so each load genuinely waits on the last.
        for (long i = 0; i < n; i++) a[i] = (int)i;
        for (long i = n - 1; i > 0; i--) {
            long j = rand() % i;
            int t = a[i]; a[i] = a[j]; a[j] = t;
        }

        int idx = 0;
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (long i = 0; i < ACCESSES; i++) idx = a[idx];   // dependent chase
        clock_gettime(CLOCK_MONOTONIC, &t1);

        double ns = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
        printf("%10ld  %12.2f\n", kb, ns / (double)ACCESSES);
        sink = idx;          // force the chase to be observed (defeats dead-code elim)
        free(a);
    }
    return sink == -1;
}
```

While the array fits in L1/L2 you'll see a roughly flat ~2 ns; as it spills into L3 the
time climbs steadily, and once the working set no longer fits in cache and every chase
waits on RAM it rises toward ~100+ ns — a real run on an Intel Core i7 laptop went
`2.3 → 4.6 → 12.6 → 33.7 → 99.5 → 164.5 ns` from 32 KB up to 64 MB. You just *measured*
the hierarchy — no profiler required. Two details make it work, and both are easy to get
wrong: the **random** Sattolo cycle defeats the prefetcher (a sequential `(i+1)%n` ring
stays ~2 ns even at 64 MB because the CPU predicts it), and the dependent `idx = a[idx]`
chain (with the `volatile` sink) stops `-O2` from overlapping the loads or deleting the
loop. Drop either and the curve flattens and hides the effect.

## Failure modes & trade-offs

- **The flat-memory assumption.** Algorithms compared only by operation count can lose
  badly in practice: a linked list and a `std::vector`/array may have the same Big-O,
  but the array wins enormously because it streams contiguous cache lines while the list
  chases pointers all over RAM.
- **Working set, not total size.** What matters is how much memory is *hot* in a tight
  window, not how much you allocated. A 1 GB array touched sequentially can be cache-
  friendly; a 1 MB structure touched randomly may not be.
- **You can't cache your way out of bad access patterns.** Caches are automatic but not
  magic. Random access, pointer chasing, and large strides defeat them; the fix is data
  layout, not a bigger cache.
- **Disk is a different universe.** Once the OS has to page to SSD or disk, you've left
  the nanosecond world for micro/milliseconds. Thrashing (constant paging) makes a
  machine feel frozen even with the CPU mostly idle.

## In practice

- **Think in terms of "where does this live right now."** Reading the same value in a
  loop is cheap; touching a fresh cache line every iteration is not.
- **Contiguous and sequential beats clever and scattered.** Arrays of structs walked in
  order are the default fast path; this is the seed of *data-oriented design*.
- **Measure, don't guess.** The benchmark above, `perf stat` (cache-miss counters), and
  Cachegrind tell you where you are in the pyramid. Latency intuition is often wrong by
  10–100×.
- **This is the substrate for the whole atlas.** The stack is fast partly because it's
  always hot in cache; `malloc` can be slow partly because fresh heap memory is cold.
  Keep this pyramid in mind when we reach allocators and performance.

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/registers-and-the-isa|Registers & the ISA]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|The CPU: fetch–decode–execute]] · [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 6 — the memory hierarchy, locality, and cache organization; the spine for this note. https://csapp.cs.cmu.edu/
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — the definitive deep dive on DRAM, caches, and access patterns, with measurements. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- **Latency Numbers Every Programmer Should Know** (Jeff Dean / Peter Norvig, "by year" version) — the canonical order-of-magnitude latency table. https://colin-scott.github.io/personal_website/research/interactive_latency.html
- **Agner Fog — *The microarchitecture of Intel, AMD and VIA CPUs*** — real cache sizes, latencies, and how the hierarchy behaves per microarchitecture. https://www.agner.org/optimize/
- **Intel 64 and IA-32 Architectures Optimization Reference Manual** — authoritative cache behavior, line sizes, and prefetch on x86-64. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
