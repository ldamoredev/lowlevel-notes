---
title: "Data layout & cache-friendliness"
description: Fast memory code starts with layout. Cache lines reward contiguous, predictable access and punish pointer chasing, wide strides, cold fields mixed with hot fields, and layouts that fetch bytes you never use.
tags: [data-layout, cache, locality, soa, aos, performance]
order: 10
updated: 2026-06-30
---
# Data layout & cache-friendliness

The CPU does not fetch individual fields from RAM one at a time. It moves cache lines:
small contiguous chunks, commonly 64 bytes, that become fast if you reuse them and costly
if you waste them. Data layout is the choice of which bytes sit next to which other bytes.
A layout that matches the loop's access pattern can be faster than clever code over a
hostile layout. This is where pointers and memory turn into performance.

> The reset: locality is a property of the access pattern plus the layout. Ask "when I
> touch this byte, which neighboring bytes did I also pay to bring into cache?"

## Cache-friendly means predictable and dense

The usual wins:

| Pattern | Why it helps |
|---|---|
| contiguous arrays | hardware prefetchers and cache lines work with the loop |
| small element stride | more useful elements per cache line |
| hot/cold split | hot loops stop dragging rarely used fields through cache |
| structure of arrays (SoA) | loops over one field read one dense array |
| fewer pointer hops | fewer unpredictable loads and cache misses |

The usual losses are the opposite: linked structures scattered across the heap, arrays of
large structs when the loop reads one field, mixed hot and cold fields, and indirect
access patterns that defeat prefetching.

## How it really works

Suppose each particle has positions, velocities, and an `alive` flag:

```c
struct Particle {
    float x, y, z;
    float vx, vy, vz;
    int alive;
};
```

An array of these structs is an **array of structs** (AoS). It is convenient when code uses
one whole particle at a time. But a loop that only sums live `x` positions steps by
`sizeof(struct Particle)` to read a single `float` and one flag. Most bytes fetched in
each cache line are not useful to that loop.

A **structure of arrays** (SoA) stores each field in a separate array:

```c
struct Particles {
    float *x, *y, *z;
    float *vx, *vy, *vz;
    int *alive;
};
```

Now a loop over `x[i]` and `alive[i]` walks two dense arrays. The stride for `x` is
`sizeof(float)`, not `sizeof(struct Particle)`. That improves spatial locality and often
makes vectorization easier. The trade-off is API complexity: creating, resizing, and
passing the data now involves several arrays that must stay in sync.

There is no universal winner. AoS is often better for code that consumes complete records:
parsing one packet, updating one entity with all fields, or handing an object to an API.
SoA is often better for numeric loops that touch the same field across many elements.
Hybrid layouts, such as arrays of small chunks or hot/cold splits, are common in real
systems.

Pointer-heavy structures pay another cost. A linked list node may contain only a few
useful bytes, but each `next` pointer can jump to a different cache line or page. The CPU
cannot know the next address until the current node loads, so memory-level parallelism and
prefetching suffer. Arrays make the next address obvious.

## Executable artifact: AoS stride vs SoA stride

The demo lives in `examples/pointers-and-memory/data-layout-and-cache-friendliness/demo.c`.

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Real output:

```
AoS particle size     = 28 bytes
AoS x stride          = 28 bytes
SoA x stride          = 4 bytes
sum alive x AoS       = 8.0
sum alive x SoA       = 8.0
```

Both layouts compute the same result. The access pattern is not the same. In AoS, adjacent
`x` fields are 28 bytes apart because each particle carries all fields together. In SoA,
adjacent `x` values are 4 bytes apart, so a cache line holds many `x` values.

## Failure modes & trade-offs

- **Optimizing layout before knowing the loop.** Layout is only "friendly" relative to an
  access pattern. Start with the hot loop, not a slogan.
- **Pointer chasing.** Lists, trees, and graphs can be semantically right and still cache
  hostile. Use them when insertion/deletion/topology matters more than scanning speed.
- **Hot and cold fields together.** Rarely used strings, debug data, or ownership metadata
  inside a hot struct can pollute every cache line.
- **SoA complexity.** Multiple arrays must be allocated, resized, and kept at the same
  length. Bugs move from "bad stride" to "arrays out of sync."
- **False sharing later.** In concurrent code, adjacent hot fields used by different
  threads can fight over the same cache line.
- **Benchmark traps.** Tiny demos fit in cache and lie. Real decisions need realistic
  sizes, access patterns, compiler flags, and measurement.

## In practice

- **Design data around the dominant loop.** Write down which fields the loop reads and
  writes, then put those bytes close together.
- **Prefer arrays for scanning.** If you iterate over everything often, contiguous storage
  is the default.
- **Split hot and cold data.** Keep the small fields needed every frame/request/packet away
  from rarely used metadata.
- **Choose AoS for whole-record code, SoA for field-wise numeric loops.** Use hybrids when
  each side has a real workload.
- **Measure cache behavior, not just wall time.** Use profilers and hardware counters when
  available: cache misses, branch misses, cycles, and bandwidth tell the story.
- **Keep correctness first.** A clear AoS layout that is fast enough beats a fragile SoA
  layout with unclear ownership.

**Connects to:** [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alignment & padding]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Pointer arithmetic & stride]] · [[lowlevel/machine-model/cache-lines-and-locality|Cache lines & locality]] · [[lowlevel/machine-model/the-memory-hierarchy|The memory hierarchy]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — cache hierarchy, cache lines, prefetching, locality, and memory access costs. https://www.akkadia.org/drepper/cpumemory.pdf
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — locality, cache behavior, and memory hierarchy as seen by C programs. https://csapp.cs.cmu.edu/
- **Agner Fog — optimization manuals** — practical CPU-level details on caches, memory access, and vectorization. https://www.agner.org/optimize/
- **Mike Acton — *Data-Oriented Design and C++*** — canonical talk on designing layouts around access patterns. https://www.youtube.com/watch?v=rX0ItVEVjHc
- **cppreference — Structure declaration** — the C layout rules that determine AoS element size and field offsets. https://en.cppreference.com/w/c/language/struct
