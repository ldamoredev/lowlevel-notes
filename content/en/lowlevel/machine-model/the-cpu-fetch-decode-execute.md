---
title: "The CPU: fetch–decode–execute & the clock"
description: The CPU is a timed state machine: it fetches bytes at the instruction pointer, decodes them into work, executes them, writes results back, and advances or redirects control.
tags: [machine-model, cpu, clock, x86-64]
order: 2
updated: 2026-06-21
---
# The CPU: fetch–decode–execute & the clock

A CPU is not "running C." It is running a tiny loop in hardware: fetch the next
instruction bytes, decode what they mean, execute the operation, write back the
result, then decide which instruction comes next. The clock gives that loop a rhythm,
but it does not mean "one instruction per tick." Once you can point at `rip` and say
"the next bytes come from there," source code starts turning into a physical thing.

> The reset: execution is just state changing over time. Registers hold values, memory
> holds bytes, `rip` points at the next instruction, and the clock keeps the machine
> moving through that state.

## The loop the CPU never stops running

The classic model is **fetch -> decode -> execute -> write-back**. It is the smallest
useful story of a processor:

| Stage | What happens |
|---|---|
| Fetch | Read instruction bytes from memory at the address in the program counter |
| Decode | Turn those bytes into an operation: `add`, `mov`, `ret`, a branch, a load |
| Execute | Use the ALU, load/store unit, branch unit, or other hardware to do the work |
| Write-back | Commit the visible result to a register, memory, flags, or the next `rip` |

The important von Neumann idea is that **instructions and data live in the same
addressable memory**. A byte can be part of your string, part of your integer, or part
of an instruction stream; context decides. Your compiled program is not magic text.
It is bytes loaded into memory. The CPU fetches some of those bytes as instructions,
and those instructions may read or write other bytes as data.

That is why low-level programming keeps collapsing abstractions. A C expression
becomes instructions. An instruction becomes bytes. Those bytes sit at addresses.
The CPU only sees the last two layers: addresses and bytes.

## The instruction pointer is the bookmark

Every architecture has a register whose job is "where is the next instruction?" The
generic name is **program counter**; on x86-64 the architectural register is `rip`,
the **instruction pointer**.

For straight-line code, `rip` advances by the length of the instruction just decoded.
On x86-64 that length is variable: one instruction might be 1 byte, another 3, another
10 or more. The CPU does not add a fixed amount. It decodes enough bytes to know the
instruction length, then advances `rip` to the next instruction.

Control flow is just code that writes the bookmark somewhere else:

- A conditional jump changes `rip` only if its condition is true.
- An unconditional jump writes a new target into `rip`.
- A `call` stores a return address on the stack and then redirects `rip` to the callee.
- A `ret` pops that saved address and puts it back into `rip`.

This is the mechanical meaning of "the program branches." No flowchart is happening
inside the CPU. There is only a next address.

## The clock is a metronome, not a stopwatch

A 3 GHz CPU has a clock ticking about three billion times per second. One tick is a
**cycle**. That sounds like "three billion instructions per second," but that is the
wrong model.

Different instructions have different costs. A simple integer add can be cheap. A
load that misses cache may wait hundreds of cycles. Division is much slower than
addition. A branch whose direction was predicted correctly may be nearly invisible;
a mispredicted branch can flush work the CPU had already started. Latency is "how
long before this result is ready"; throughput is "how often can the machine start or
finish this kind of work." They are not the same number.

Modern CPUs also break instructions into internal work units, overlap many
instructions, and issue more than one operation per cycle when dependencies allow it.
So the clock is not a promise that one source line, one instruction, or one operation
finishes per tick. It is the rhythm against which all that hardware progress is
measured.

## Trace it by hand

Here is a tiny C program you can compile and run:

```c
#include <stdio.h>

__attribute__((noinline))
int add_then_double(int a, int b) {
    int sum = a + b;
    return sum * 2;
}

int main(void) {
    printf("%d\n", add_then_double(7, 5));
    return 0;
}
```

Build it locally:

```bash
cc -Wall -Wextra -O2 -fomit-frame-pointer demo.c -o demo && ./demo
```

```text
24
```

Now paste just the function into [Compiler Explorer](https://godbolt.org), choose an
x86-64 GCC or Clang compiler, use `-O2 -fomit-frame-pointer -masm=intel`, and hide
directives/comments. A typical x86-64 output is:

```asm
add_then_double:
    lea     eax, [rdi + rsi]
    add     eax, eax
    ret
```

Under the System V AMD64 calling convention, the first integer argument arrives in
`edi`, the second in `esi`, and the integer return value leaves in `eax`. The exact
addresses depend on the loader, but assume the function starts at `0x401120`:

| Step | `rip` before fetch | Instruction | What execute/write-back does | `rip` after |
|---|---|---|---|---|
| 1 | `0x401120` | `lea eax, [rdi + rsi]` | Computes `7 + 5`, writes `12` to `eax` | `0x401123` |
| 2 | `0x401123` | `add eax, eax` | Adds `eax` to itself, writes `24` to `eax`, updates flags | `0x401125` |
| 3 | `0x401125` | `ret` | Pops the caller's return address from `[rsp]` into `rip` | caller address |

Two details matter here. First, `lea` is not loading memory in this form; it is using
the address-generation hardware as arithmetic. Second, writing `eax` in x86-64 also
zeroes the upper half of `rax`, so the full 64-bit return register is clean even
though the C return type is `int`.

That table is the mental move: a function call is not an abstract visit to a named
block. It is `rip` landing on bytes, those bytes mutating registers, and `ret`
restoring `rip` to the saved continuation.

## Modern CPUs preserve the model, not the mechanism

Real high-performance CPUs do not literally finish one instruction before fetching
the next. They pipeline fetch/decode/execute stages so many instructions are in
flight at once. They are superscalar, meaning they can issue multiple operations in
one cycle. They execute out of order when independent work can run before older
blocked work. They predict branches so fetch can continue before the branch is
resolved.

That does not make fetch-decode-execute useless. It means the model is
**architectural**, not microscopic. The CPU works hard to preserve the illusion that
your single thread executed in program order, with registers and memory ending up as
if each instruction completed one after another. When you read assembly, debug a
crash, reason about a branch, or understand what `ret` does, that architectural model
is the one you need first.

The deeper machinery matters later for performance. Pipelining, cache misses, branch
prediction, and out-of-order execution explain why two instruction streams with the
same result can have very different cost. But you cannot reason about the tricks
until the simple model is solid.

## ARM64 appendix

On AArch64, the same function has the same shape but different registers and
fixed-width instructions. First integer argument: `w0`. Second: `w1`. Integer return:
`w0`.

```asm
add_then_double:
    add     w8, w1, w0
    lsl     w0, w8, #1
    ret
```

Every AArch64 instruction here is 4 bytes wide, so the program counter advances in
neat 4-byte steps for straight-line code. The architectural story is still the same:
fetch at the program counter, decode, execute, write back, advance or branch.

## In practice

- **Do not read GHz as speed.** Clock rate is one input. Instruction mix, dependency
  chains, memory latency, branch prediction, and compiler output decide what useful
  work happens per cycle.
- **Do not assume a source line is an instruction.** The optimizer may erase it,
  combine it, duplicate it, or replace it with an instruction like `lea` whose name
  does not look like the C operation.
- **When control flow feels mysterious, find `rip`.** In a debugger, the current
  instruction pointer tells you where execution really is. The call stack is a story
  reconstructed from saved return addresses and frame/unwind metadata.
- **Use the simple model first, then refine.** Fetch-decode-execute is the right first
  abstraction. Pipelining and out-of-order execution explain performance, not the
  basic meaning of your program.

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/you-are-the-runtime-now|You are the runtime now]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]]

## Sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3–4 — machine-level C, instruction sequencing, and the processor architecture model. https://csapp.cs.cmu.edu/
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1–2 — the x86-64 authority for `rip`, instruction encoding, and instruction semantics. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Compiler Explorer** — the fastest way to verify how a concrete C fragment becomes x86-64 or AArch64 assembly under real compilers. https://godbolt.org/
- **Charles Petzold — *Code: The Hidden Language of Computer Hardware and Software*** — builds the intuition that instructions are bytes interpreted by hardware.
- **Ben Eater — *Build an 8-bit computer from scratch*** — a visible fetch/decode/execute CPU built from simple parts; ideal for making the control loop concrete. https://eater.net/8bit
- **Agner Fog — optimization manuals** — advanced reference for instruction latency, throughput, pipelining, and real microarchitectural cost. https://www.agner.org/optimize/
