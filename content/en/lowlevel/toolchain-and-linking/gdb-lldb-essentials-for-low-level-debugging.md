---
title: "gdb / lldb essentials for low-level debugging"
description: Low-level debugging starts with symbols and a debugger: compile with -g, set breakpoints, step, inspect frames, variables, memory, registers, and postmortem core dumps.
tags: [toolchain, debugging, gdb, lldb, registers, core-dumps]
order: 11
updated: 2026-06-22
---
# gdb / lldb essentials for low-level debugging

A debugger is a controlled microscope for a running program. Compile with debug
information, stop execution at a chosen point, inspect the current stack frame, step one
source line or instruction at a time, and read memory/registers when source-level state is
not enough. `gdb` and `lldb` have different command syntax, but the low-level workflow is
the same: stop, orient, inspect, step, and test the hypothesis.

> The reset: `-g` does not make code "debug mode." It adds metadata that lets the debugger
> map machine addresses back to source files, functions, lines, variables, and types.

## How it really works

Debuggers combine several layers of information. The executable contains machine code and,
when built with `-g`, debug sections or platform-equivalent metadata. The OS exposes control
over another process through debugging APIs such as `ptrace` on Unix-like systems or
debugserver/dyld integration on macOS. The debugger uses both: OS control to stop and read
the process, debug info to explain what those bytes and registers mean.

The core commands map cleanly between GDB and LLDB:

| Task | GDB | LLDB |
|---|---|---|
| Start program | `run` | `run` |
| Break at function | `break accumulate` | `breakpoint set --name accumulate` |
| Backtrace | `bt` | `bt` |
| Print variable | `print value` | `frame variable value` or `expression value` |
| Step over | `next` | `thread step-over` |
| Step into | `step` | `thread step-in` |
| Examine memory | `x/16xb ptr` | `memory read --size 1 --count 16 ptr` |
| Registers | `info registers` | `register read` |

Breakpoints replace "add printf and recompile" with a stop point. Backtraces show how you
got there. Frame variables show local state with type information. Memory reads and register
reads are where the abstraction drops: pointers become addresses, stack frames become bytes,
and calling convention details from the assembly branch become visible.

## Core dumps and postmortem debugging

A core dump is a snapshot of a crashed process. Instead of reproducing the crash live, you
load the executable plus the core file and inspect the last state:

```bash
gdb ./demo core
lldb --core core ./demo
```

Core dumps depend on OS policy. Linux often needs `ulimit -c unlimited` and may route dumps
through `systemd-coredump` or `/proc/sys/kernel/core_pattern`. macOS has its own crash
reporting and core-dump controls. The concept is stable: debug info plus a memory/register
snapshot lets you ask "where did it die and what was the state?"

## Executable artifact: breakpoint, step, variables, registers

The example lives in
`examples/toolchain-and-linking/gdb-lldb-essentials-for-low-level-debugging/`. It compiles
with `-g`, runs once normally, then runs LLDB in batch mode. The script redacts volatile
addresses and process ids to keep the output comparable while still using real debugger
output.

```c
#include <stdio.h>

static int accumulate(int value) {
    int doubled = value * 2;
    int biased = doubled + 5;
    return biased;
}

int main(void) {
    int result = accumulate(7);
    printf("debug result: %d\n", result);
    return 0;
}
```

Run it:

```bash
cd examples/toolchain-and-linking/gdb-lldb-essentials-for-low-level-debugging
./run.sh
```

Real output from this machine:

```text
== compile with debug info ==
debug result: 19
== lldb batch session ==
Breakpoint 1: 2 locations.
    frame #0: 0xADDR demo`accumulate(value=7) at demo.c:4:19
(int) value = 7
    frame #0: 0xADDR demo`accumulate(value=7) at demo.c:5:18
(int) doubled = 14
    frame #0: 0xADDR demo`accumulate(value=7) at demo.c:6:12
(int) biased = 19
  * frame #0: 0xADDR demo`accumulate(value=7) at demo.c:6:12
     rip = 0xADDR  demo`accumulate + 25 at demo.c:6:12
     rsp = 0xADDR
     rbp = 0xADDR
Process PID exited with status = 0 (0xADDR)
```

The breakpoint stops at `accumulate`. The first frame variable shows the argument. Two
step-over commands execute the assignments, so `doubled` and `biased` become inspectable.
The backtrace line shows the current frame. The register read shows `rip`, `rsp`, and `rbp`,
which tie directly to x86-64 control flow and stack-frame mechanics.

## Failure modes & trade-offs

- **No debug info.** Without `-g`, you can still debug assembly and symbols, but source
  lines, locals, and types may be missing or degraded.
- **Optimization changes reality.** With `-O2`, variables can be optimized away, inlined, or
  live only in registers. Debugging optimized code is real but less direct.
- **Debugger permissions.** macOS and hardened Linux setups can block launching or attaching
  unless debugger permissions are granted.
- **Stale source vs binary.** If the executable was not rebuilt from the source you are
  viewing, line numbers and variables can mislead you.
- **Core dumps may be disabled.** Postmortem debugging requires OS limits and crash-dump
  routing to allow core capture.
- **Heisenbugs exist.** Timing, threads, signals, and data races can change behavior when a
  debugger stops the process.

## In practice

- **Compile a debug build first.** `-O0 -g -Wall -Wextra` is the simple starting point.
- **Set breakpoints on narrow hypotheses.** Stop near the suspicious transition, not at the
  beginning of the whole program.
- **Use backtrace before stepping.** Knowing how you got there often matters more than the
  current line.
- **Read memory when pointers are suspicious.** Source variables can hide aliasing,
  lifetime, and layout bugs.
- **Read registers when ABI or assembly matters.** Calls, returns, stack corruption, and
  crash addresses are register-level facts.
- **Use sanitizers before heroic debugging.** ASan/UBSan often point directly at memory and
  UB bugs that would take longer to chase manually.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/assembly-and-compiler-output/stack-frames-function-prologue-and-epilogue|Stack frames]] · [[lowlevel/assembly-and-compiler-output/system-v-amd64-calling-convention|System V AMD64 calling convention]] · [[lowlevel/pointers-and-memory/use-after-free-and-double-free|Use-after-free and double-free]] · [[lowlevel/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model|gcc vs clang and the compiler driver model]]

## Sources

- **GDB Manual** — breakpoints, stepping, stack frames, memory examination, registers, and core files. https://sourceware.org/gdb/current/onlinedocs/gdb.html/
- **LLDB Tutorial** — official command examples for breakpoints, stepping, frames, variables, and expressions. https://lldb.llvm.org/use/tutorial.html
- **LLDB GDB command map** — practical mapping between common GDB and LLDB commands. https://lldb.llvm.org/use/map.html
- **GCC Debugging Options** — `-g` and debug-info generation from the compiler driver. https://gcc.gnu.org/onlinedocs/gcc/Debugging-Options.html
- **Apple Debugging and LLDB docs** — platform-specific LLDB/debugserver behavior on macOS. https://developer.apple.com/documentation/xcode/debugging
