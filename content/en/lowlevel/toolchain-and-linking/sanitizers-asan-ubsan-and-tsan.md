---
title: "Sanitizers: ASan, UBSan, and TSan"
description: Sanitizers are compiler instrumentation modes: ASan catches many memory bugs, UBSan catches undefined behavior, and TSan catches data races with runtime checks.
tags: [toolchain, sanitizers, asan, ubsan, tsan, debugging]
order: 12
updated: 2026-06-22
---
# Sanitizers: ASan, UBSan, and TSan

Sanitizers are compiler-inserted runtime checks. Instead of waiting for memory corruption,
undefined behavior, or data races to surface as strange symptoms, you build a special binary
that watches the program while it runs. AddressSanitizer (ASan) focuses on memory safety,
UndefinedBehaviorSanitizer (UBSan) focuses on language-rule violations, and ThreadSanitizer
(TSan) focuses on data races. They are not proofs of correctness, but they make many
low-level bugs loud and local.

> The reset: a sanitizer is not an external debugger. The compiler instruments your program,
> links a sanitizer runtime, and the runtime reports violations while the program executes.

## How it really works

Sanitizers add checks around operations the compiler can see. ASan surrounds allocations
with red zones, tracks poisoned shadow memory, and reports out-of-bounds, use-after-free,
and some stack lifetime bugs. UBSan inserts checks for operations such as signed overflow,
invalid shifts, null/dangling assumptions, alignment violations, and bad casts depending on
enabled options. TSan instruments memory accesses and synchronization operations to detect
conflicting unsynchronized accesses from different threads.

| Sanitizer | Finds | Typical flag |
|---|---|---|
| ASan | heap/stack/global out-of-bounds, use-after-free, double-free patterns | `-fsanitize=address` |
| UBSan | signed overflow, invalid shifts, alignment, null, vptr/cast issues by language | `-fsanitize=undefined` |
| TSan | data races between threads | `-fsanitize=thread -pthread` |

The cost is deliberate. ASan often uses about 2x memory and meaningful CPU overhead. TSan is
heavier because it tracks cross-thread access history. UBSan can be light in recover mode or
hard-fail with trap options. Sanitizer builds are for development, CI, fuzzing, and
debugging; they are usually not your production release binary unless you choose that trade.

## Executable artifact: make bugs loud

The example lives in `examples/toolchain-and-linking/sanitizers-asan-ubsan-and-tsan/`. It
has one small C file per sanitizer. The script captures expected sanitizer failures and
keeps going so all three reports are visible.

ASan demo:

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *values = malloc(3 * sizeof(int));
    if (values == NULL) {
        return 1;
    }

    values[0] = 10;
    values[1] = 20;
    values[2] = 30;
    printf("asan before bug: %d\n", values[1]);
    fflush(stdout);

    values[3] = 40;

    free(values);
    return 0;
}
```

UBSan demo:

```c
#include <limits.h>
#include <stdio.h>

int main(void) {
    int value = INT_MAX;
    int overflowed = value + 1;
    printf("ubsan result: %d\n", overflowed);
    return 0;
}
```

Run it:

```bash
cd examples/toolchain-and-linking/sanitizers-asan-ubsan-and-tsan
./run.sh
```

Real output from this machine:

```text
== ASan: heap buffer overflow ==
asan exit status: 1
asan before bug: 20
==PID==ERROR: AddressSanitizer: heap-buffer-overflow on address 0xADDR at pc 0xADDR bp 0xADDR sp 0xADDR
asan_demo:x86_64+0xADDR) in main+0xADDR
== UBSan: signed integer overflow ==
ubsan exit status: 0
ubsan_demo.c:6:28: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
ubsan result: -2147483648
== TSan: data race ==
tsan exit status: 66
WARNING: ThreadSanitizer: data race (pid=PID)
tsan_demo:x86_64+0xADDR) in worker+0xADDR
tsan counter: 2000
```

The script normalizes volatile addresses and PIDs, but the reports are real sanitizer
output from this machine. ASan stops on the heap write one element past the allocation.
UBSan reports signed integer overflow but continues in recover mode, so the wrapped result
still prints. TSan reports a race on the unsynchronized global counter and exits with the
configured sanitizer exit code.

## Failure modes & trade-offs

- **Instrumentation sees executed paths.** If the bad path never runs, the sanitizer cannot
  report it.
- **Not every bug is covered.** ASan does not prove all memory is safe; UBSan does not cover
  every possible undefined behavior; TSan reports data races, not all concurrency bugs.
- **Overhead is real.** Sanitized binaries are slower and larger, especially TSan builds.
- **Optimization can help or hurt diagnostics.** Use a debuggable optimization level such
  as `-O1 -g` or `-O0 -g` depending on sanitizer and reproduction.
- **Sanitizers can conflict.** ASan and TSan are generally not combined in one binary. Build
  separate configurations.
- **Runtime availability is platform-specific.** Apple clang, upstream Clang, and GCC have
  different sanitizer support matrices.

## In practice

- **Run ASan early and often.** It is the fastest way to catch many C memory bugs before
  they become mysterious crashes.
- **Keep UBSan in CI.** Signed overflow and invalid shifts often point at wrong assumptions
  even when the program "seems fine."
- **Use TSan on threaded tests.** It is heavy, but data races are too subtle to debug only by
  inspection.
- **Compile everything relevant with the sanitizer.** Instrumented code calling large
  uninstrumented regions can miss context.
- **Pair sanitizers with debuggers.** Let the sanitizer find the violation, then use LLDB or
  GDB when you need to inspect surrounding state.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/toolchain-and-linking/gdb-lldb-essentials-for-low-level-debugging|gdb / lldb essentials]] · [[lowlevel/pointers-and-memory/buffer-overflow-and-out-of-bounds-access|Buffer overflow and out-of-bounds access]] · [[lowlevel/pointers-and-memory/use-after-free-and-double-free|Use-after-free and double-free]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior]]

## Sources

- **Clang AddressSanitizer documentation** — ASan build flags, runtime behavior, and supported bug classes. https://clang.llvm.org/docs/AddressSanitizer.html
- **Clang UndefinedBehaviorSanitizer documentation** — UBSan checks, recover/trap modes, and flags. https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
- **Clang ThreadSanitizer documentation** — TSan usage, overhead, supported platforms, and race reports. https://clang.llvm.org/docs/ThreadSanitizer.html
- **GCC Instrumentation Options** — GCC sanitizer flags and related runtime options. https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html
- **Google Sanitizers Wiki** — practical notes on sanitizer runtimes and diagnostics. https://github.com/google/sanitizers/wiki
