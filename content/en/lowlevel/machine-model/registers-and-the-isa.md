---
title: "Registers & the ISA"
description: Registers are the CPU's fastest, smallest storage; the ISA is the contract that defines them and the instructions that touch them. The x86-64 register map, sub-register aliasing, and why the compiler fights over so few names.
tags: [machine-model, registers, isa, x86-64, arm64]
order: 3
updated: 2026-06-21
---
# Registers & the ISA

We already watched the CPU fetch bytes at `rip` and mutate state in
[[lowlevel/machine-model/the-cpu-fetch-decode-execute|the fetch–decode–execute loop]].
This note zooms into *where that state lives* and *who defines the rules*. Registers
are the handful of named, fixed-width slots inside the CPU — the fastest storage in
the machine by a wide margin. The **instruction set architecture (ISA)** is the
contract that says which registers exist, what instructions can touch them, and how
those instructions are encoded. C variables are a fiction; registers and the ISA are
the real, finite surface your code runs on.

> The reset: there is no infinite pool of variables. There are ~16 general-purpose
> registers, and almost everything the CPU does is "load into a register, operate on
> registers, store back." The compiler spends real effort deciding what gets to live
> in one.

## Why registers are special

A register is on-die storage the ALU can read and write in essentially zero extra
time — it *is* where computation happens. Everything else is farther away and slower.
Rough latencies on a modern machine:

| Storage | Approx. access latency | Capacity |
|---|---|---|
| Register | ~0 cycles (it's the operand) | ~16 GPRs × 8 bytes |
| L1 cache | ~4 cycles | tens of KB |
| L2 cache | ~12 cycles | hundreds of KB |
| RAM | ~100–300 cycles | GBs |

That gap is the whole reason registers matter. An `add rax, rbx` is free compared to
a value that has to come from RAM. The CPU cannot operate directly on most memory the
way a high-level language pretends — it brings values *into* registers, works there,
and writes results *back*. "Make it fast" very often means "keep the hot values in
registers and out of memory."

## The x86-64 register map

x86-64 exposes **16 general-purpose registers (GPRs)**, each 64 bits wide:

| Register | Conventional role (per the ABI / some instructions) |
|---|---|
| `rax` | Return value; implicit operand for `mul`/`div` |
| `rbx` | Callee-saved scratch |
| `rcx` | 4th argument; implicit shift/`rep` count |
| `rdx` | 3rd argument; high half of `mul`/`div` |
| `rsi`, `rdi` | 2nd and 1st arguments; string-op source/dest |
| `rbp` | Frame pointer (by convention) |
| `rsp` | **Stack pointer** (not a free register) |
| `r8`–`r15` | Added by x86-64; `r8`/`r9` are the 5th/6th arguments |

They are called "general purpose," but the ISA and the ABI pin conventional jobs onto
most of them — `rsp` is always the stack pointer, `rax` carries return values, shifts
read their count from `cl`. Beyond the GPRs there's `rip` (the instruction pointer,
not directly writable as data), `rflags` (status bits like zero, carry, sign,
overflow), and the **vector registers** `xmm0–15` / `ymm` / `zmm` (128/256/512-bit)
that SIMD code uses.

## Sub-register aliasing (and a sharp edge)

Each GPR is one physical register you can address at four widths. They are **not
separate registers** — they overlap:

| 64-bit | 32-bit | 16-bit | 8-bit (low) |
|---|---|---|---|
| `rax` | `eax` | `ax` | `al` |
| `rdi` | `edi` | `di` | `dil` |
| `r8` | `r8d` | `r8w` | `r8b` |

Writing `eax` and writing `al` are not symmetric, and this is a classic surprise:

```asm
mov eax, 1      ; writes the low 32 bits AND zero-extends: rax = 0x0000000000000001
mov al, 1       ; writes only the low 8 bits: the upper 56 bits of rax are UNCHANGED
```

Writing a **32-bit** sub-register zero-extends into the full 64-bit register (we saw
this in note #2: an `int` return in `eax` leaves `rax` clean). Writing an **8- or
16-bit** sub-register leaves the upper bits intact — so a stale high byte can leak
through if you assumed otherwise. The hardware does this for compatibility reasons,
and it's a real source of subtle bugs in hand-written assembly.

## The ISA is a contract, not a chip

The **instruction set architecture** is the specification a CPU promises to honor. It
defines:

- the **register set** (the names above) and their widths,
- the **instructions**, their **semantics**, and their **byte encoding**,
- **addressing modes** (how operands name memory),
- the **memory model** (what ordering other cores can observe), and
- privilege levels, interrupts, and exceptions.

The ISA is the boundary between two worlds. **Compilers target it; CPUs implement
it.** Crucially, many different *microarchitectures* implement the same ISA: Intel and
AMD both implement **x86-64**; Apple's M-series, Qualcomm, and Ampere all implement
**ARM64 (AArch64)**. Code built for an ISA runs on any conforming chip, even though
the silicon inside is wildly different. That separation is exactly why the OS in this
atlas can target x86-64 and run unchanged on an Intel Mac and (via QEMU) on Apple
Silicon.

Two big ISA families, two philosophies:

| | x86-64 | ARM64 (AArch64) |
|---|---|---|
| Style | CISC | RISC |
| Instruction length | Variable (1–15 bytes) | Fixed (4 bytes) |
| GPRs | 16 | 31 (`x0`–`x30`) + `sp` |
| Memory operands | Many instructions touch memory | Load/store only |

The CISC/RISC line is blurrier than it used to be — modern x86 chips decode those
complex instructions into RISC-like internal *micro-ops* — but the framing still
explains why ARM64 disassembly looks so regular and x86-64 so dense.

## See the registers get used

A function with six arguments shows the System V argument registers and the return
register at a glance. Paste this into [Compiler Explorer](https://godbolt.org) with an
x86-64 compiler at `-O2 -masm=intel`:

```c
long sum6(long a, long b, long c, long d, long e, long f) {
    return a + b + c + d + e + f;
}
```

```asm
sum6:
    lea     rax, [rdi + rsi]   ; rax = a + b
    add     rax, rdx           ; + c
    add     rax, rcx           ; + d
    add     rax, r8            ; + e
    add     rax, r9            ; + f
    ret                        ; return value already in rax
```

Every argument arrived in a register the ABI fixed in advance — `rdi, rsi, rdx, rcx,
r8, r9` — and the result accumulates in `rax` because that's where a return value
lives. (The 7th integer argument, if there were one, would come in on the stack, not a
register — there simply aren't enough.) The *why* of this ordering is the
[[lowlevel/assembly-and-compiler-output/index|calling convention]]; the point here is
that the compiler is choosing among a small, fixed set of names.

## ARM64 appendix

AArch64 has 31 general-purpose registers `x0`–`x30` (64-bit), each with a 32-bit view
`w0`–`w30`, plus a separate stack pointer `sp` and a quirky **zero register**
(`xzr`/`wzr`) that reads as 0 and discards writes. `x30` is the link register (return
address); the program counter is not a GPR. Same `sum6`:

```asm
sum6:
    add     x8, x0, x1
    add     x8, x8, x2
    add     x8, x8, x3
    add     x8, x8, x4
    add     x0, x8, x5
    ret
```

Arguments come in `x0`–`x7` (up to eight in registers, versus six on x86-64), and the
return value leaves in `x0`. Writing `w0` zero-extends into `x0`, mirroring the
`eax`→`rax` rule.

## In practice

- **There are far fewer registers than variables.** When a function needs more live
  values than registers, the compiler **spills** some to the stack and reloads them —
  "register pressure." Tight loops are faster partly because their hot values fit in
  registers.
- **Sub-register writes are not uniform.** 32-bit writes zero-extend; 8/16-bit writes
  preserve the upper bits. In hand-written or inline assembly, this leaks stale data if
  you forget it.
- **Don't memorize the map — recognize it.** You don't need every conventional role by
  heart, but seeing `rdi`/`rsi`/`rax` in a disassembly and knowing "args and return"
  is what makes reading compiler output fast.
- **One ISA, many chips.** Targeting an ISA (x86-64) rather than a specific CPU is what
  makes a binary portable across vendors and generations — and what lets the OS project
  stay host-agnostic.

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|The CPU: fetch–decode–execute]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 — machine-level representation of programs; registers, operands, and how C maps onto them. https://csapp.cs.cmu.edu/
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 — the authoritative x86-64 register model, sub-register behavior, and encoding. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Arm Architecture Reference Manual for A-profile (AArch64)** — the authoritative ARM64 register set, `wzr`/`xzr`, and instruction semantics. https://developer.arm.com/documentation/ddi0487/latest
- **System V AMD64 ABI** — which registers carry arguments, returns, and which are callee-saved. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Compiler Explorer** — verify in seconds which registers a real compiler assigns to a given C function. https://godbolt.org/
- **Agner Fog — optimization manuals** — register usage, the cost of spills, and microarchitectural detail. https://www.agner.org/optimize/
