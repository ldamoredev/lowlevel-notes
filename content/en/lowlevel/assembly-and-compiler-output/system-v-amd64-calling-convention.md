---
title: "The System V AMD64 calling convention"
description: The System V AMD64 ABI defines how x86-64 functions pass arguments, return values, align the stack, and preserve registers. Learn the register map and stack argument layout from real compiler output.
tags: [assembly, x86-64, abi, calling-convention, system-v]
order: 8
updated: 2026-06-30
---
# The System V AMD64 calling convention

A calling convention is the contract that lets separately compiled functions call each
other. It says where arguments go, where return values come back, which registers a
callee must preserve, how the stack is aligned, and who cleans up stack arguments. The
System V AMD64 ABI is the dominant convention for x86-64 Unix-like targets. Without it,
`gcc`, `clang`, libc, debuggers, and your hand-written assembly would not agree on how a
function call works.

> The reset: a function call is not "pass variables." It is a register and stack
> protocol. The C parameter names are gone by the time the callee starts executing.

## How it really works

For integer and pointer arguments, the first six arguments arrive in this order:

| Argument | Register |
|---|---|
| 1 | `rdi` |
| 2 | `rsi` |
| 3 | `rdx` |
| 4 | `rcx` |
| 5 | `r8` |
| 6 | `r9` |

Additional integer/pointer arguments are passed on the stack. Integer return values
come back in `rax`. Floating-point and vector arguments have their own rules through
`xmm` registers and classification logic; this note stays on the integer path because
it is the first one you read constantly.

The register-saving split matters:

| Register class | Meaning |
|---|---|
| caller-saved | caller must save if it needs the value after a call |
| callee-saved | callee must restore before returning if it modifies it |

Common caller-saved integer registers include `rax`, `rdi`, `rsi`, `rdx`, `rcx`, `r8`,
`r9`, `r10`, and `r11`. Common callee-saved registers include `rbx`, `rbp`, and
`r12` through `r15`. `rsp` must be restored by the callee.

## A real eight-argument call

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/system-v-amd64-calling-convention/`.

```c
static NOINLINE long abi_mix(long a, long b, long c, long d,
                             long e, long f, long g, long h) {
    return a + 2 * b - c + 3 * d + e - 4 * f + g + h;
}
```

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
call site copied from the real output of:

```sh
gcc -S -O2 -fno-omit-frame-pointer -masm=intel demo.c -o demo.s
```

```asm
	mov	rdi, qword ptr [rbp - 64]
	mov	rsi, qword ptr [rbp - 56]
	mov	rdx, qword ptr [rbp - 48]
	mov	rcx, qword ptr [rbp - 40]
	mov	r8, qword ptr [rbp - 32]
	mov	r9, qword ptr [rbp - 24]
	push	qword ptr [rbp - 8]
	push	qword ptr [rbp - 16]
	call	_abi_mix
	add	rsp, 16
```

The first six arguments are loaded into `rdi`, `rsi`, `rdx`, `rcx`, `r8`, and `r9`.
Arguments seven and eight are pushed to the stack before `call`. The pushes appear in
reverse order because stack addresses grow downward: after both pushes, argument seven
is closest to the return address in the position the callee expects. After the call,
`add rsp, 16` removes those two 8-byte stack arguments from the caller's stack.

Inside the callee:

```asm
_abi_mix:                               ## @abi_mix
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
	pop	rbp
	ret
	.cfi_endproc
```

The callee reads `a` through `f` from registers. The seventh and eighth arguments are
at positive offsets from `rbp`: `[rbp + 16]` and `[rbp + 24]`. With this frame shape,
`[rbp]` holds the saved caller `rbp`, `[rbp + 8]` holds the return address, and the next
two slots hold stack-passed arguments.

The return value is left in `rax`. That is why the caller can immediately pass it to
`printf` by moving `rax` into the next call's second argument register:

```asm
	lea	rdi, [rip + L_.str]
	mov	rsi, rax
	xor	eax, eax
	call	_printf
```

The `xor eax, eax` before `printf` is also ABI-related: for variadic functions, System V
AMD64 uses `al` to communicate how many vector registers were used for floating-point
varargs. Here there are none, so the compiler sets it to zero.

## Stack alignment and ownership

The ABI also requires stack alignment at call boundaries. That is why you often see
apparently "extra" stack adjustment. The compiler is not only reserving locals; it is
maintaining the alignment that called code expects for spills, vector instructions, and
library routines.

Caller and callee responsibilities are split:

| Responsibility | Owner |
|---|---|
| place arguments | caller |
| push return address | `call` instruction |
| preserve callee-saved registers | callee |
| clean stack arguments in this ABI | caller |
| place integer return value | callee, in `rax` |

Once you know the contract, assembly listings become less mysterious. Register choices
are not arbitrary; they are the ABI.

## ARM64 appendix

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Relevant ARM64 callee:

```asm
_abi_mix:                               ; @abi_mix
	.cfi_startproc
; %bb.0:
	add	x8, x0, x1, lsl #1
	add	x9, x3, x3, lsl #1
	sub	x8, x8, x2
	add	x8, x8, x9
	add	x8, x8, x4
	sub	x8, x8, x5, lsl #2
	add	x8, x8, x6
	add	x0, x8, x7
	ret
	.cfi_endproc
```

Apple ARM64 passes the first eight integer/pointer arguments in `x0` through `x7`, and
returns integer values in `x0`. This example has exactly eight arguments, so no stack
argument is needed on ARM64. The contrast with System V AMD64 is useful: x86-64 had six
integer argument registers and used the stack for `g` and `h`; ARM64 kept all eight in
registers.

The call site shows that register setup directly:

```asm
	ldur	x0, [x29, #-8]
	ldur	x1, [x29, #-16]
	ldur	x2, [x29, #-24]
	ldur	x3, [x29, #-32]
	ldr	x4, [sp, #40]
	ldr	x5, [sp, #32]
	ldr	x6, [sp, #24]
	ldr	x7, [sp, #16]
	bl	_abi_mix
```

`bl` branches with link: it transfers control and writes the return address to `x30`.

## Failure modes & trade-offs

- **Using the wrong ABI.** Windows x64, System V AMD64, and Apple ARM64 do not use the
  same argument registers or stack rules. Hand assembly must target the right ABI.
- **Forgetting caller-saved registers.** If you need `rax`, `rdi`, `rsi`, `rdx`, `rcx`,
  `r8`, `r9`, `r10`, or `r11` after a call, save them yourself.
- **Forgetting callee-saved registers.** If your assembly function modifies `rbx`,
  `rbp`, or `r12` through `r15`, restore them before returning.
- **Misaligning the stack.** Library code may assume ABI alignment. A bad manual call
  sequence can crash far away from the mistake.
- **Ignoring variadic function rules.** Calls like `printf` have extra ABI details, such
  as the `al` vector-register count on System V AMD64.
- **Assuming parameter names survive.** Assembly has registers, stack slots, and debug
  metadata. The machine does not know C names like `a` or `g`.

## In practice

- **Start with the register map.** Before reading a function body, assign `rdi`, `rsi`,
  `rdx`, `rcx`, `r8`, and `r9` to source parameters.
- **Look above the return address for stack args.** With a frame pointer, stack-passed
  integer arguments often show up as `[rbp + 16]`, `[rbp + 24]`, and so on.
- **Treat calls as clobber points.** Caller-saved registers may change across a call.
- **Read `rax` as the return path.** Integer return values leave through `rax` on
  System V AMD64.
- **Keep this beside stack frames.** The calling convention explains why prologues,
  epilogues, stack alignment, and saved registers have the shapes they do.

**Connects to:** [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|x86-64 registers and the register file]] · [[lowlevel/assembly-and-compiler-output/stack-frames-function-prologue-and-epilogue|Stack frames: function prologue and epilogue]] · [[lowlevel/assembly-and-compiler-output/stack-at-the-assembly-level|The stack at the assembly level]] · [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|The core instruction set: mov, arithmetic, lea]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]]

## Sources

- **System V AMD64 ABI** — the authoritative calling convention for argument passing, return values, stack alignment, register classes, and varargs details. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 and Vol. 2 — instruction semantics for `call`, `ret`, stack operations, and integer register behavior. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 — procedure calls, register usage, stack frames, and machine-level C examples. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — readable examples of x86-64 function calls and stack-passed arguments. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Agner Fog — Calling conventions for different C++ compilers and operating systems** — practical cross-platform ABI comparison. https://www.agner.org/optimize/calling_conventions.pdf
- **Apple — Writing ARM64 code for Apple platforms** — Apple ARM64 calling-convention details for the appendix contrast. https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
