---
title: "ARM64 appendix: AArch64 registers and calling convention"
description: AArch64 has x0-x30 registers, w-register 32-bit views, x0-x7 argument passing, x29 as frame pointer, x30 as link register, and sp as the stack pointer. Learn the Apple ARM64 call shape from real output.
tags: [assembly, arm64, aarch64, calling-convention, registers]
order: 12
updated: 2026-06-30
---
# ARM64 appendix: AArch64 registers and calling convention

AArch64 is the 64-bit ARM instruction set you will see on Apple Silicon and many modern
servers. It is still a register machine, but its vocabulary is cleaner than x86-64:
general-purpose registers are `x0` through `x30`, the 32-bit views are `w0` through
`w30`, arguments usually start in `x0`, and the return address lives in the link
register `x30` until it must be spilled. If x86-64 feels historically layered, ARM64
feels deliberately regular.

> The reset: on AArch64, `x0` and `w0` are two views of the same register. `x0` is
> 64-bit; `w0` is the low 32-bit view, and writes to `w0` zero the upper half of `x0`.

## How it really works

The registers you read first:

| Register | Role |
|---|---|
| `x0`-`x7` | first integer/pointer arguments and return value in `x0` |
| `w0`-`w7` | low 32-bit views of `x0`-`x7` |
| `x8`-`x15` | common temporaries and indirect-result/scratch roles depending on ABI |
| `x19`-`x28` | callee-saved general-purpose registers |
| `x29` | frame pointer (`fp`) by convention |
| `x30` | link register (`lr`), holds return address after `bl` |
| `sp` | stack pointer |

Function calls use `bl target`: branch with link. It jumps to `target` and writes the
return address to `x30`. A non-leaf function usually saves `x29` and `x30` in its stack
frame:

```asm
stp	x29, x30, [sp, #offset]
add	x29, sp, #offset
...
ldp	x29, x30, [sp, #offset]
ret
```

Unlike x86-64 `call`, `bl` does not push the return address to the stack by itself. The
callee spills `x30` only when it needs to preserve it across another call.

## A real AArch64 call with stack arguments

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/arm64-appendix-aarch64-registers-and-calling-convention/`.

```c
NOINLINE long arm64_mix(long a, long b, long c, long d, long e,
                        long f, long g, long h, long i, long j) {
    return a + 2 * b - c + 3 * d + e - 4 * f + g + h - i + j;
}
```

The function has ten integer arguments. Apple ARM64 passes the first eight in `x0`
through `x7`; the ninth and tenth are passed on the stack.

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Relevant callee:

```asm
_arm64_mix:                             ; @arm64_mix
	.cfi_startproc
; %bb.0:
	ldp	x9, x8, [sp]
	add	x10, x0, x1, lsl #1
	add	x11, x3, x3, lsl #1
	sub	x10, x10, x2
	add	x10, x10, x11
	add	x10, x10, x4
	sub	x10, x10, x5, lsl #2
	add	x10, x10, x6
	add	x10, x10, x7
	sub	x9, x10, x9
	add	x0, x9, x8
	ret
	.cfi_endproc
```

Map it:

- `a` through `h` arrive in `x0` through `x7`.
- `ldp x9, x8, [sp]` loads `i` and `j` from the first two stack argument slots.
- `x10` and `x11` are scratch registers chosen by the compiler.
- `add x10, x0, x1, lsl #1` computes `a + 2*b`.
- `sub x10, x10, x5, lsl #2` subtracts `4*f`.
- `add x0, x9, x8` writes the final return value to `x0`.
- `ret` returns to the address in `x30`.

The call site shows the caller side:

```asm
	ldur	x0, [x29, #-8]
	ldur	x1, [x29, #-16]
	ldur	x2, [x29, #-24]
	ldur	x3, [x29, #-32]
	ldur	x4, [x29, #-40]
	ldr	x5, [sp, #48]
	ldr	x6, [sp, #40]
	ldr	x7, [sp, #32]
	ldr	x8, [sp, #24]
	ldr	x9, [sp, #16]
	stp	x8, x9, [sp]
	bl	_arm64_mix
```

The caller loads arguments one through eight into `x0`-`x7`, places arguments nine and
ten at `[sp]` with `stp`, and calls with `bl`. After the call, the return value is in
`x0`.

## Stack frames and the link register

`main` is non-leaf because it calls `arm64_mix` and `printf`, so it saves frame pointer
and link register:

```asm
	sub	sp, sp, #112
	stp	x29, x30, [sp, #96]
	add	x29, sp, #96
	...
	ldp	x29, x30, [sp, #96]
	add	sp, sp, #112
	ret
```

The pair store/load instructions are idiomatic ARM64. `stp` stores a pair of registers;
`ldp` loads a pair. Saving `x30` is what lets `main` survive nested calls: each `bl`
overwrites `x30`, so non-leaf functions preserve the old link register somewhere safe.

## x86-64 comparison

The same C compiled for x86-64 System V uses a different contract:

```asm
_arm64_mix:                             ## @arm64_mix
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rsi, [rdi + 2*rsi]
	sub	rsi, rdx
	lea	rax, [rcx + 2*rcx]
	add	rax, rsi
	add	rax, r8
	shl	r9, 2
	sub	rax, r9
	add	rax, qword ptr [rbp + 16]
	add	rax, qword ptr [rbp + 24]
	sub	rax, qword ptr [rbp + 32]
	add	rax, qword ptr [rbp + 40]
	pop	rbp
	ret
	.cfi_endproc
```

x86-64 System V has six integer argument registers, so arguments seven through ten are
read from stack slots. ARM64 had eight argument registers and only used the stack for
arguments nine and ten. The C was identical; the ABI changed the machine boundary.

## Failure modes & trade-offs

- **Mixing `x` and `w` carelessly.** Writing `w0` zeroes the upper 32 bits of `x0`.
  That is useful, but it can surprise you if you expected a partial write.
- **Forgetting `x30` is volatile across calls.** Every `bl` overwrites the link register.
  Non-leaf functions must preserve it if they need to return correctly.
- **Treating `sp` like a general register.** AArch64 has restrictions around `sp` in
  some instruction forms and requires stack alignment.
- **Assuming x86 stack habits.** ARM64 does not push a return address with `bl`; it uses
  the link register.
- **Ignoring platform ABI differences.** Linux AArch64 and Apple ARM64 are close, but
  platform ABIs still define details such as red zones, reserved registers, and varargs.
- **Reading `csel` as a branch.** Conditional selects compute dataflow without changing
  control flow.

## In practice

- **Map `x0`-`x7` first.** For small functions, most of the argument story is in those
  eight registers.
- **Watch for stack args at `[sp]`.** Once arguments exceed the register window, the
  caller places the rest on the stack.
- **Track `x0` to read returns.** Integer/pointer return values leave in `x0`.
- **Look for `x29`/`x30` pairs.** `stp x29, x30` and `ldp x29, x30` are frame setup and
  teardown for non-leaf functions.
- **Use ARM64 output as a second opinion.** Comparing it with x86-64 often clarifies
  what came from C and what came from the target ABI.

**Connects to:** [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|x86-64 registers and the register file]] · [[lowlevel/assembly-and-compiler-output/system-v-amd64-calling-convention|The System V AMD64 calling convention]] · [[lowlevel/assembly-and-compiler-output/stack-frames-function-prologue-and-epilogue|Stack frames: function prologue and epilogue]] · [[lowlevel/assembly-and-compiler-output/how-c-constructs-compile-loops-structs-switch|How C constructs compile: loops, structs, switch]] · [[lowlevel/machine-model/registers-and-the-isa|Registers & the ISA]]

## Sources

- **Apple — Writing ARM64 code for Apple platforms** — Apple-platform AArch64 calling convention, register roles, stack alignment, and ABI notes. https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
- **Arm Architecture Reference Manual for A-profile architecture** — authoritative AArch64 instruction and register semantics. https://developer.arm.com/documentation/ddi0487/latest
- **Arm Procedure Call Standard for AArch64 (AAPCS64)** — procedure-call rules, register classes, argument passing, return values, and stack constraints. https://github.com/ARM-software/abi-aa
- **Stephen Smith — *Programming with 64-Bit ARM Assembly Language*** — practical AArch64 assembly examples and calling-convention explanations. https://link.springer.com/book/10.1007/978-1-4842-5881-1
- **System V AMD64 ABI** — contrast source for the x86-64 comparison in this note. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Compiler Explorer (Godbolt)** — fast cross-target comparison of the same C source under x86-64 and ARM64 compilers. https://godbolt.org/
