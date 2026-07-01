---
title: "valgrind and profilers (perf, cachegrind)"
description: Valgrind, cachegrind, and perf answer different questions than sanitizers: memory correctness, simulated cache/call costs, and hardware-level performance counters.
tags: [toolchain, valgrind, perf, cachegrind, profiling, debugging]
order: 13
updated: 2026-06-22
---
# valgrind and profilers (perf, cachegrind)

Sanitizers instrument your build. Valgrind and profilers observe execution from different
angles. Memcheck runs your program on Valgrind's synthetic CPU and tracks memory validity.
Cachegrind simulates cache and branch behavior to explain locality costs. Linux `perf`
samples real hardware/software events from the kernel side. The practical rule is to ask
the right question: "is this memory access valid?", "where does the program spend cycles?",
or "which cache/branch behavior dominates?"

> The reset: correctness tools and profiling tools are not interchangeable. Memcheck finds
> many memory bugs slowly; `perf` measures real execution cheaply; cachegrind gives a
> deterministic simulation that is useful but not the hardware itself.

## How it really works

Valgrind translates machine code to an intermediate representation and runs it under a
tool. Memcheck tracks definedness, addressability, allocations, frees, and leaks. That is
why it can catch uninitialized reads and invalid frees in binaries you did not compile with
ASan. The cost is high: programs can run tens of times slower.

Cachegrind is another Valgrind tool. It records instruction counts and simulates cache and
branch behavior. It is excellent for comparing algorithmic locality and call hot spots in a
repeatable way, but it is a model. Real machines have prefetchers, shared caches, TLBs,
frequency changes, and OS noise.

`perf` is Linux-specific and uses kernel performance events. It can sample CPU cycles,
instructions, cache misses, branches, page faults, context switches, and call stacks. It
measures the real binary on the real machine with far less overhead than Valgrind, but it
requires Linux permissions and careful interpretation.

| Tool | Best question | Cost |
|---|---|---|
| Valgrind Memcheck | "Did I read invalid/uninitialized memory or leak?" | very high |
| Cachegrind | "Where are instruction/cache/branch costs in a repeatable model?" | high |
| Linux `perf stat` | "What hardware/software event counts did this run produce?" | low |
| Linux `perf record/report` | "Where did sampled time go?" | low to medium |
| Sanitizers | "Can compiler instrumentation catch this bug during tests?" | medium |

## Executable artifact: workload plus local tool availability

The example lives in `examples/toolchain-and-linking/valgrind-and-profilers-perf-cachegrind/`.
This macOS environment does not have Valgrind, Cachegrind, or Linux `perf`, so the script
builds and runs a deterministic workload, then reports local tool availability instead of
inventing output.

```c
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t mix(uint64_t value) {
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return value;
}

int main(void) {
    enum { count = 4096 };
    uint64_t *values = malloc(count * sizeof(*values));
    if (values == NULL) {
        return 1;
    }

    for (int i = 0; i < count; i++) {
        values[i] = mix((uint64_t)i + 1U);
    }

    uint64_t checksum = 0;
    for (int pass = 0; pass < 8; pass++) {
        for (int i = 0; i < count; i++) {
            checksum ^= mix(values[i] + (uint64_t)pass);
        }
    }

    printf("profile checksum: 0x%016llx\n", (unsigned long long)checksum);
    free(values);
    return 0;
}
```

Run it:

```bash
cd examples/toolchain-and-linking/valgrind-and-profilers-perf-cachegrind
./run.sh
```

Real output from this machine:

```text
== build workload ==
profile checksum: 0xa57dea46fbef3b53
== tool availability on this machine ==
valgrind: not installed on this machine
perf: not installed on this machine
cachegrind: not available without valgrind
```

On Linux with the tools installed, the commands to run against the same workload are:

```bash
valgrind --tool=memcheck --leak-check=full ./demo
valgrind --tool=cachegrind ./demo
cg_annotate cachegrind.out.*
perf stat ./demo
perf record -g ./demo
perf report
```

On macOS, the rough equivalents are Instruments, `sample`, `leaks`, `vmmap`, `dtrace`/DTrace
where permitted, and Xcode's profiling UI. They are not drop-in replacements for Linux
`perf`, and Valgrind support on modern macOS is limited.

## Valgrind vs sanitizers

ASan usually runs much faster than Memcheck and catches many memory bugs in test loops.
Memcheck can inspect uninstrumented binaries and detects uninitialized-value uses that ASan
does not target in the same way. ASan changes compilation; Memcheck changes execution.
Neither replaces ownership discipline or tests.

TSan is the sanitizer for data races. Valgrind has Helgrind/DRD for threading analysis, but
TSan is usually the first choice when the compiler/runtime support is available. For
performance, use profilers, not memory checkers.

## Failure modes & trade-offs

- **Profiling debug builds lies.** `-O0` changes the program shape. Profile optimized builds
  with symbols, such as `-O2 -g`.
- **Microbenchmarks are fragile.** CPU frequency, warmup, cache state, input size, and OS
  noise can dominate small runs.
- **Sampling needs enough time.** A program that runs for 2 ms may not produce useful `perf
  record` samples.
- **Valgrind changes execution.** It is excellent for memory diagnostics, but too slow and
  synthetic for final performance conclusions.
- **Cachegrind is a model.** Treat it as a comparative locality tool, not a perfect model of
  your CPU.
- **Permissions matter.** Linux `perf` may require `perf_event_paranoid` changes or root for
  some events.

## In practice

- **Use Memcheck when ASan is unavailable or you need uninitialized-read/leak detail.**
- **Use ASan/UBSan first in fast CI loops.** They find many correctness bugs sooner.
- **Use `perf stat` before `perf record`.** Event counts tell you whether the program is
  instruction-heavy, branchy, cache-missy, or syscall/context-switch heavy.
- **Use Cachegrind for locality experiments.** It is useful when comparing two data layouts
  or loop structures.
- **Always keep symbols.** Optimized-with-debug-info builds make profiler output readable
  without destroying the optimized shape.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/toolchain-and-linking/sanitizers-asan-ubsan-and-tsan|Sanitizers: ASan, UBSan, and TSan]] · [[lowlevel/toolchain-and-linking/gdb-lldb-essentials-for-low-level-debugging|gdb / lldb essentials]] · [[lowlevel/pointers-and-memory/data-layout-and-cache-friendliness|Data layout and cache-friendliness]] · [[lowlevel/machine-model/cache-lines-and-locality|Cache lines and locality]]

## Sources

- **Valgrind User Manual** — Valgrind architecture, Memcheck behavior, and tool usage. https://valgrind.org/docs/manual/manual.html
- **Valgrind Memcheck Manual** — invalid accesses, uninitialized values, leaks, and diagnostics. https://valgrind.org/docs/manual/mc-manual.html
- **Valgrind Cachegrind Manual** — cache/branch simulation, `cg_annotate`, and interpretation. https://valgrind.org/docs/manual/cg-manual.html
- **Linux `perf` wiki** — `perf stat`, `perf record`, event model, and workflow overview. https://perfwiki.github.io/main/
- **Brendan Gregg — Linux perf examples** — practical `perf` command patterns and interpretation. https://www.brendangregg.com/perf.html
