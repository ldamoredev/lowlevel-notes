---
title: "x86-64 registers and the register file"
description: The x86-64 register file is the compiler's tiny, named workspace. Learn the general-purpose registers, sub-register aliases, ABI roles, and how to trace values through real compiler output.
tags: [assembly, x86-64, registers, abi, compiler-output]
order: 2
updated: 2026-06-30
---
# x86-64 registers and the register file

Assembly becomes readable when you stop seeing a wall of mnemonics and start tracking
where values live. On x86-64, the main working surface is the **register file**: a
small set of named CPU storage slots that instructions read and write directly. The
compiler is constantly deciding which live C values deserve those slots and which must
spill to memory. If you can recognize `rdi`, `rsi`, `rax`, `rsp`, and friends, compiler
output stops being noise and starts becoming a dataflow sketch.

> The reset: a C variable is not a place. A register is. When optimized code runs, the
> compiler may keep a value in a register, move it to another register, recompute it,
> or delete it entirely.

## How it really works

x86-64 exposes sixteen 64-bit **general-purpose registers (GPRs)**. They are "general"
because most integer, pointer, address, and control-flow work can use them, but they
are not all socially equal. The ISA gives some registers special behavior, and the ABI
gives many of them conventional jobs.

For Unix-like x86-64 targets, the System V AMD64 ABI gives you this practical reading
map:

| Register | What to recognize first |
|---|---|
| `rax` | integer/pointer return value; common scratch accumulator |
| `rdi`, `rsi` | 1st and 2nd integer/pointer arguments |
| `rdx`, `rcx` | 3rd and 4th integer/pointer arguments |
| `r8`, `r9` | 5th and 6th integer/pointer arguments |
| `r10`, `r11` | caller-saved scratch registers |
| `rbx`, `r12`–`r15` | callee-saved registers; a function must restore them if it uses them |
| `rbp` | frame pointer by convention; callee-saved |
| `rsp` | stack pointer; not a normal scratch register |

There are also registers you read constantly but do not treat like ordinary data
slots: `rip` is the instruction pointer, `rflags` holds condition bits, and
`xmm`/`ymm`/`zmm` registers carry floating-point and SIMD data. This note focuses on
the GPRs because they are the first thing you need when reading integer and pointer
code.

## Widths and aliases

Each x86-64 GPR has overlapping names at smaller widths. These are aliases for the
same physical register, not separate storage:

| 64-bit | 32-bit | 16-bit | 8-bit low |
|---|---|---|---|
| `rax` | `eax` | `ax` | `al` |
| `rbx` | `ebx` | `bx` | `bl` |
| `rcx` | `ecx` | `cx` | `cl` |
| `rdx` | `edx` | `dx` | `dl` |
| `rdi` | `edi` | `di` | `dil` |
| `rsi` | `esi` | `si` | `sil` |
| `r8` | `r8d` | `r8w` | `r8b` |

The sharp edge is write width. A 32-bit write like `mov eax, ...` or `xor eax, eax`
zero-extends into the full 64-bit register, so the upper half of `rax` becomes zero.
An 8-bit or 16-bit write like `mov al, ...` or `mov ax, ...` changes only that slice
and leaves the upper bits alone. This is why optimized x86-64 often uses `eax` even
when the surrounding code later reads `rax`: writing `eax` is a cheap way to produce a
known-clean 64-bit value.

`rsp` deserves its own warning. It is a GPR-shaped name, but it points to the top of
the stack. Treating it like scratch corrupts the call stack, return addresses, local
storage, and ABI alignment rules. When you see `add rsp, 48` or `sub rsp, 48`, read
that as stack-frame bookkeeping, not ordinary arithmetic.

## Trace a real function

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/x86-64-registers-and-the-register-file/`. The
function is designed to make the six integer argument registers visible:

```c
long mix_six(long a, long b, long c, long d, long e, long f) {
    long acc = a + b * 2;
    acc ^= c;
    acc += d * 4;
    acc -= e;
    return acc + f;
}
```

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
function body copied from the real output of:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_mix_six:                               ## @mix_six
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rdi + 2*rsi]
	xor	rax, rdx
	lea	rax, [rax + 4*rcx]
	sub	rax, r8
	add	rax, r9
	pop	rbp
	ret
	.cfi_endproc
```

Read it as register flow:

- `rdi` is `a`, `rsi` is `b`, `rdx` is `c`, `rcx` is `d`, `r8` is `e`, and `r9` is `f`.
- `lea rax, [rdi + 2*rsi]` computes `a + b * 2` and puts the running value in `rax`.
- `xor rax, rdx` applies `^ c`; `rdx` is an input, not a magical multiply/divide
  register in this context.
- `lea rax, [rax + 4*rcx]` adds `d * 4` while keeping the result in `rax`.
- `sub rax, r8` and `add rax, r9` consume the fifth and sixth argument registers.
- `ret` returns with the result already in `rax`.

That is the whole register-reading habit from
[[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Compiler Explorer as a daily tool]]: find the function, map the ABI registers, then
trace the value that becomes the return.

## Register pressure and spills

The register file is fast because it is small. When the compiler has more live values
than registers, it **spills** some values to memory, usually into the current stack
frame, then reloads them later. This is not "the compiler being dumb"; it is the
finite-register budget becoming visible.

You will see pressure when:

- many values must stay live across a long expression or loop,
- a function call forces caller-saved registers to be preserved somewhere,
- vector code competes for SIMD registers,
- debug-friendly `-O0` output keeps source-shaped variables in stack slots, or
- inline assembly tells the compiler that many registers are clobbered.

The decision rule is simple: when reading optimized assembly, register movement is
normal; repeated memory traffic in a hot loop is the thing to question.

## ARM64 appendix

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Relevant function body:

```asm
_mix_six:                               ; @mix_six
	.cfi_startproc
; %bb.0:
	add	x8, x0, x1, lsl #1
	eor	x8, x8, x2
	add	x8, x8, x3, lsl #2
	sub	x8, x8, x4
	add	x0, x8, x5
	ret
	.cfi_endproc
```

AArch64 has a larger and more regular GPR set: `x0`–`x30` are 64-bit registers, with
`w0`–`w30` as their 32-bit views. Integer arguments arrive in `x0`–`x7`, and return
values leave in `x0`. Here the compiler uses `x8` as a scratch accumulator and moves
the final result into `x0` with the last `add`.

The contrast is useful: the same C function that used `rdi`, `rsi`, `rdx`, `rcx`,
`r8`, `r9`, and `rax` on x86-64 uses `x0`–`x5` plus `x8` on ARM64. The high-level
meaning is stable; the register file and calling convention are target-specific.

## Failure modes & trade-offs

- **ABI matters.** System V AMD64 uses `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9` for the
  first six integer/pointer arguments. Windows x64 uses a different order and fewer
  argument registers (`rcx`, `rdx`, `r8`, `r9`). Same ISA family, different ABI.
- **Registers do not equal variables.** One register can hold different source values
  at different times, and one source value can move through several registers or never
  exist as a register at all.
- **Partial-register writes bite.** 32-bit writes zero-extend; 8/16-bit writes do not.
  Hand-written assembly and inline assembly need to be explicit about which width is
  being defined.
- **`rsp` is not spare storage.** If you corrupt the stack pointer, `ret`, local
  variables, saved registers, and alignment assumptions can all break at once.
- **Caller-saved vs callee-saved is a contract.** A function may freely clobber
  caller-saved registers, but it must restore callee-saved registers before returning.
  Inline assembly must tell the compiler what it clobbers.
- **`-O0` lies about pressure.** Debug builds often store every source-shaped variable
  on the stack. Use `-O2` when asking how optimized code uses the register file.

## In practice

- **Annotate argument registers first.** On System V x86-64, write `a/b/c/d/e/f` over
  `rdi/rsi/rdx/rcx/r8/r9` mentally before decoding each instruction.
- **Track one live value.** Follow the value that ends in `rax`; that often explains
  the whole function.
- **Treat `rbp`/`rsp` as frame machinery.** They describe where stack storage is, not
  the business computation.
- **Watch for spills in hot loops.** Loads/stores to `[rsp + ...]` or `[rbp - ...]`
  inside a loop can mean register pressure, ABI preservation, or debug-oriented code.
- **Keep the machine-model note nearby.** This note is the assembly-facing version of
  [[lowlevel/machine-model/registers-and-the-isa|registers & the ISA]]: the same finite
  storage, now with ABI roles and real compiler output attached.

**Connects to:** [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Why read assembly — Compiler Explorer as a daily tool]] · [[lowlevel/machine-model/registers-and-the-isa|Registers & the ISA]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|The CPU: fetch–decode–execute]] · [[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]] · [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is]]

## Sources

- **System V AMD64 ABI** — the x86-64 Unix calling convention: argument registers, return registers, caller-saved and callee-saved register rules. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 — authoritative x86-64 register model, sub-register behavior, `rip`, `rflags`, and SIMD register families. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Felix Cloutier — x86 and amd64 instruction reference** — quick instruction lookup for `lea`, `xor`, `sub`, `add`, and `ret`; verify against Intel SDM when precision matters. https://www.felixcloutier.com/x86/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 — machine-level representation of C programs and register-level dataflow. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — approachable x86-64 register and instruction reference with runnable examples. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Compiler Explorer** — fast feedback for checking how a real compiler allocates registers under specific flags and targets. https://godbolt.org/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — AArch64 register semantics plus Apple-platform ABI details for `clang -arch arm64` output. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
