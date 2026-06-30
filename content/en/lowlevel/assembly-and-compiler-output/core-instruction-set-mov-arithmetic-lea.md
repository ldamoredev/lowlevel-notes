---
title: "The core instruction set: `mov`, arithmetic, `lea`"
description: The first x86-64 instructions to read fluently are data movement, integer arithmetic, and lea. Learn how real compiler output uses mov, add/sub, imul, and lea to express C expressions.
tags: [assembly, x86-64, instructions, mov, lea]
order: 3
updated: 2026-06-30
---
# The core instruction set: `mov`, arithmetic, `lea`

Most optimized integer code is built from a small vocabulary repeated at high speed:
copy a value, compute on registers, maybe touch memory, return. You do not need the
whole x86-64 manual in your head to start reading compiler output. If `mov`,
`add`/`sub`, `imul`, `xor`, and `lea` feel familiar, a surprising amount of C becomes
legible. The trick is to read instructions as **state changes**: which register or
memory location is written, and which operands were read.

> The reset: an instruction is not a line of C. It is one state transition in the CPU:
> read operands, write a destination, advance `rip`, and maybe update flags.

## How it really works

In Intel syntax, the destination comes first:

```asm
add     rax, rcx      ; rax = rax + rcx
mov     rax, rdi      ; rax = rdi
sub     rax, 12       ; rax = rax - 12
```

That destination-first habit is the first thing to internalize. `mov rax, rdi` copies
from `rdi` into `rax`; it does not erase `rdi`. Most integer instructions have the same
shape: the destination is also one of the inputs, and the result overwrites it.

The tiny core:

| Instruction | Mental model | Notes |
|---|---|---|
| `mov dst, src` | copy bits from `src` to `dst` | `dst` can be a register or memory; not memory-to-memory |
| `add dst, src` | `dst += src` | updates condition flags |
| `sub dst, src` | `dst -= src` | updates condition flags |
| `imul dst, src` | signed multiply | common two-operand form: `dst *= src` |
| `xor dst, src` | bitwise xor | `xor eax, eax` is a common zeroing idiom |
| `lea dst, [address]` | compute an address expression | no memory load; flags are not updated |

`mov` is a copy, not a move. `lea` is the opposite of what its name suggests to many
beginners: it computes the **effective address** inside brackets and writes that
integer to a register. It does not dereference memory. Compilers love `lea` because the
x86-64 address-generation hardware can compute forms like `base + index*2/4/8 +
displacement` without changing flags.

## A real compiler trace

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea/`.

```c
long core_ops(long x, long y, long *out) {
    long product = x * y;
    long adjusted = product + x * 8;

    *out = product;
    return adjusted - y + 12;
}
```

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
function body copied from the real output of:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_core_ops:                              ## @core_ops
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rsi + 8]
	imul	rax, rdi
	sub	rax, rsi
	imul	rsi, rdi
	mov	qword ptr [rdx], rsi
	add	rax, 12
	pop	rbp
	ret
	.cfi_endproc
```

Map the registers first: under the ABI, `x` arrives in `rdi`, `y` in `rsi`, `out` in
`rdx`, and the return value leaves in `rax`.

The compiler rewrote the expression algebraically. Instead of literally computing
`product + x * 8 - y + 12`, it computes `x * (y + 8) - y + 12`. The instructions are:

- `mov rbp, rsp` copies the current stack pointer into the frame pointer. This is
  prologue bookkeeping, not the business calculation.
- `lea rax, [rsi + 8]` computes `y + 8` and writes it to `rax`. No memory is touched.
- `imul rax, rdi` computes `(y + 8) * x` in `rax`.
- `sub rax, rsi` subtracts the original `y`.
- `imul rsi, rdi` computes the separate `product = y * x` because the function must
  store it through `out`.
- `mov qword ptr [rdx], rsi` stores eight bytes from `rsi` into the memory pointed to
  by `out`.
- `add rax, 12` finishes the return value.

This is why reading instructions beats imagining them. The C source has a named
`product` and `adjusted`; optimized assembly has register lifetimes and an equivalent
algebraic form.

## `lea` is arithmetic wearing address syntax

`lea` means "load effective address," but in optimized code it is often just integer
math:

```asm
lea     rax, [rdi + 2*rdi]      ; rax = rdi * 3
lea     rcx, [rsi + 4*rsi]      ; rcx = rsi * 5
lea     rax, [rax + 8]          ; rax = rax + 8
```

The bracket syntax describes an address expression. With `mov rax, [rdi + 8]`, the CPU
uses that address to read memory. With `lea rax, [rdi + 8]`, the CPU writes the numeric
address expression itself into `rax`. Same address syntax, different instruction.

Two consequences matter constantly:

- `lea` does not update `rflags`, so it is useful near comparisons and branches.
- `lea` cannot express arbitrary math; the scale is limited to 1, 2, 4, or 8, with one
  base register, one index register, and an optional displacement.

The next note goes deeper on those memory operand shapes; here the point is simply:
when you see `lea`, ask "is this address calculation or arithmetic?"

## ARM64 appendix

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Relevant function body:

```asm
_core_ops:                              ; @core_ops
	.cfi_startproc
; %bb.0:
	mul	x8, x1, x0
	add	x9, x1, #8
	str	x8, [x2]
	neg	x8, x1
	madd	x8, x9, x0, x8
	add	x0, x8, #12
	ret
	.cfi_endproc
```

AArch64 uses a more load/store-shaped vocabulary. `x0`, `x1`, and `x2` hold `x`, `y`,
and `out`. `mul x8, x1, x0` computes the product. `str x8, [x2]` stores it to `*out`.
`madd x8, x9, x0, x8` is fused multiply-add: `x8 = x9 * x0 + x8`, here using `x8` as
`-y`. The same C expression maps to a different instruction set: ARM64 has `madd`;
x86-64 used `imul` plus `sub`/`add`.

## Failure modes & trade-offs

- **Do not read mnemonics as source lines.** A compiler may fold, reorder, or rewrite
  expressions while preserving observable behavior. The `core_ops` output computes an
  equivalent algebraic form, not the source line order.
- **`mov` copies.** The source register still contains its old value. This sounds
  obvious until you start reading code that reuses registers aggressively.
- **Memory operands need size.** `qword ptr [rdx]` tells the assembler this is an
  eight-byte store. Register operands often imply size; memory alone usually does not.
- **Most x86-64 instructions allow at most one memory operand.** `mov [a], [b]` is not
  the normal shape. Load into a register, then store.
- **Flags matter.** `add`, `sub`, `imul`, and `xor` can affect condition flags; `lea`
  does not. If the next instruction is a conditional branch, flags may be the hidden
  dataflow.
- **Signedness is mostly a C-level issue until it is not.** `add`/`sub` are the same
  bits for signed and unsigned integers; comparisons, division, overflow checks, and
  widening are where signedness becomes visible.

## In practice

- **Start with the write.** For every instruction, ask what destination changes.
- **Treat `lea` as "compute this bracket expression."** Then decide whether the result
  is used as a pointer or as integer arithmetic.
- **Keep comments semantic, not transliterated.** "store product to `*out`" is useful;
  "move `rsi` to memory" is just the mnemonic repeated.
- **Expect compiler algebra.** If an optimized sequence looks unlike the C, check
  whether it is an equivalent expression before assuming mystery.
- **Pair this note with the register map.** The instructions only become readable once
  you know the roles from [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|x86-64 registers and the register file]].

**Connects to:** [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Why read assembly — Compiler Explorer as a daily tool]] · [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|x86-64 registers and the register file]] · [[lowlevel/machine-model/registers-and-the-isa|Registers & the ISA]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|The CPU: fetch–decode–execute]] · [[lowlevel/c-from-the-metal/integer-promotions-and-implicit-conversions|Integer promotions & implicit conversions]]

## Sources

- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 and Vol. 2 — authoritative semantics for `mov`, arithmetic instructions, flags, and `lea`. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Felix Cloutier — x86 and amd64 instruction reference** — quick lookup for `mov`, `add`, `sub`, `imul`, `xor`, `lea`, and `ret`; verify edge cases against Intel SDM. https://www.felixcloutier.com/x86/
- **System V AMD64 ABI** — register roles used to read the function arguments and return value. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 — machine-level representation of C expressions and condition codes. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — approachable examples for integer instructions and register-level reading. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Arm Architecture Reference Manual + Apple ARM64 docs** — ARM64 instruction semantics and platform ABI details for the appendix output. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
