---
title: "Addressing modes and memory operands"
description: x86-64 memory operands name addresses with base, index, scale, and displacement. Learn how array and struct access compile into real memory operands, and why address syntax is not the same as a load.
tags: [assembly, x86-64, addressing-modes, memory, compiler-output]
order: 4
updated: 2026-06-30
---
# Addressing modes and memory operands

Memory operands are where assembly stops looking like "register math" and starts
looking like real programs: arrays, structs, stack slots, globals, and pointers. On
x86-64, a memory operand is not a variable name; it is an **address expression**. The
CPU computes an effective address, then the instruction decides whether to load from
that address, store to it, or merely compute it with `lea`. Reading that bracketed
shape is the bridge from C pointer syntax to machine instructions.

> The reset: `[rdi + 8*rsi + 16]` is not "the value." It is an address expression. The
> instruction around it decides whether bytes are read, written, or not touched at all.

## How it really works

The common x86-64 memory-address formula is:

```text
base + index * scale + displacement
```

In Intel syntax that shows up inside brackets:

```asm
qword ptr [rdi + 8*rsi + 16]
```

Read it as:

| Piece | Meaning |
|---|---|
| `rdi` | base register: usually a pointer |
| `rsi` | index register: often an array index |
| `8` | scale: element size for `long` / pointer-sized values |
| `16` | displacement: fixed byte offset |
| `qword ptr` | operand size: eight bytes |

The scale can be 1, 2, 4, or 8. The displacement is a signed constant. Not every
operand uses every piece: `[rdi]`, `[rdi + 8]`, `[rdi + 8*rsi]`, and
`[rip + symbol]` are all normal shapes. The address expression computes bytes, not
elements; the compiler multiplies array indexes by element size.

## A real array access

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/addressing-modes-and-memory-operands/`.

```c
long touch_neighbors(long *xs, long i, long bias) {
    long current = xs[i];
    long ahead = xs[i + 2];
    long updated = current + bias;

    xs[i + 1] = updated;
    return updated + ahead;
}
```

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
function body copied from the real output of:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_touch_neighbors:                       ## @touch_neighbors
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	rax, rdx
	add	rax, qword ptr [rdi + 8*rsi]
	mov	qword ptr [rdi + 8*rsi + 8], rax
	add	rax, qword ptr [rdi + 8*rsi + 16]
	pop	rbp
	ret
	.cfi_endproc
```

Map the arguments first: `xs` is in `rdi`, `i` is in `rsi`, `bias` is in `rdx`, and
the return value leaves in `rax`.

Now decode the memory operands:

- `mov rax, rdx` copies `bias` into the return/accumulator register.
- `add rax, qword ptr [rdi + 8*rsi]` loads `xs[i]` and adds it to `bias`. `8*rsi`
  exists because `sizeof(long) == 8` on this target.
- `mov qword ptr [rdi + 8*rsi + 8], rax` stores `updated` into `xs[i + 1]`. The extra
  `+ 8` is one more `long`.
- `add rax, qword ptr [rdi + 8*rsi + 16]` loads `xs[i + 2]` and adds it to the return
  value. The extra `+ 16` is two `long` elements.

The important distinction: `add rax, [memory]` reads memory; `mov [memory], rax`
writes memory. The bracket expression is just the address. The instruction supplies
the action.

## What can be a memory operand?

x86-64 is flexible, but not arbitrary. Many instructions can use one memory operand:

```asm
mov     rax, qword ptr [rdi]            ; load
mov     qword ptr [rdi], rax            ; store
add     rax, qword ptr [rdi + 8*rsi]    ; load and add
sub     qword ptr [rdi], rax            ; read-modify-write memory
```

But most instructions do **not** allow two memory operands. The CPU cannot usually do
this in one instruction:

```asm
mov     qword ptr [rdi], qword ptr [rsi] ; not the normal x86-64 shape
```

You load into a register, then store:

```asm
mov     rax, qword ptr [rsi]
mov     qword ptr [rdi], rax
```

This is one reason registers are the center of the machine. Memory is where data lives
longer; registers are where most computation becomes possible.

## `lea` uses addressing without memory

Addressing mode syntax also powers `lea`:

```asm
lea     rax, [rdi + 8*rsi + 16]
```

That computes the numeric address `rdi + 8*rsi + 16` and writes it to `rax`. It does
not read from memory at that address. In C terms, `lea` is closer to computing
`&xs[i + 2]` than to reading `xs[i + 2]`.

Compilers use this in two ways:

- **Pointer formation.** Compute an address that will be used later.
- **Cheap arithmetic.** Compute forms like `x + 4*y + 16` without touching flags.

That dual use is why you must read the instruction, not just the brackets. Brackets
inside `mov` or `add` usually mean memory. Brackets inside `lea` mean arithmetic over
an address expression.

## ARM64 appendix

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Relevant function body:

```asm
_touch_neighbors:                       ; @touch_neighbors
	.cfi_startproc
; %bb.0:
	add	x8, x0, x1, lsl #3
	ldr	x9, [x8]
	ldr	x10, [x8, #16]
	add	x9, x9, x2
	add	x0, x9, x10
	str	x9, [x8, #8]
	ret
	.cfi_endproc
```

AArch64 is more explicit. `x0` is `xs`, `x1` is `i`, and `x2` is `bias`. The first
instruction computes a base pointer for `&xs[i]`: `x8 = x0 + (x1 << 3)`. The loads and
store then use small immediate offsets: `[x8]` for `xs[i]`, `[x8, #8]` for `xs[i + 1]`,
and `[x8, #16]` for `xs[i + 2]`.

The contrast is the lesson. x86-64 folded `base + index*8 + displacement` directly
into memory operands. ARM64 first computed the indexed base, then used load/store
instructions. Same C, different addressing vocabulary.

## Failure modes & trade-offs

- **Address syntax is not a dereference by itself.** `[rdi + 8]` inside `mov rax, ...`
  loads; inside `lea rax, ...` it does not.
- **Offsets are bytes.** `+ 16` means sixteen bytes, not sixteen elements. For `long`,
  that is two elements on this platform.
- **Memory operands hide latency.** `add rax, [addr]` looks like arithmetic, but it
  may wait on cache or RAM. Register-only `add` and memory `add` are not the same cost.
- **Read-modify-write is a real memory update.** Instructions like `add [addr], rax`
  both read and write memory. That matters for aliasing, sharing, and concurrency.
- **Most instructions get one memory operand.** If both source and destination live in
  memory, expect a register temporary.
- **C bounds are not checked.** If `i` is wrong, the address expression is still
  computed. C undefined behavior means the compiler does not owe you a trap.

## In practice

- **Translate array access to bytes.** `xs[i + 2]` becomes base `xs`, index `i`, scale
  `sizeof *xs`, displacement `2 * sizeof *xs`.
- **Name the pieces.** When reading `[rdi + 8*rsi + 16]`, say "base pointer, scaled
  index, fixed offset" before worrying about the surrounding instruction.
- **Watch memory operands in hot code.** A loop with many `[base + index*scale]`
  operands may be load/store bound, even if the arithmetic looks small.
- **Connect this to pointers.** Pointer arithmetic in C is element-based; machine
  addressing is byte-based. The compiler is the translator between
  [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|pointer arithmetic and stride]]
  and these address expressions.
- **Keep `lea` separate in your head.** `lea` uses address syntax to produce an integer
  address; it is not a load.

**Connects to:** [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|The core instruction set: mov, arithmetic, lea]] · [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|x86-64 registers and the register file]] · [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Pointer arithmetic & stride]] · [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alignment & padding]]

## Sources

- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 and Vol. 2 — authoritative x86-64 addressing modes, memory operands, and instruction semantics. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Felix Cloutier — x86 and amd64 instruction reference** — quick lookup for instruction forms that accept memory operands and the semantics of `lea`. https://www.felixcloutier.com/x86/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 — machine-level representation of arrays, pointers, and memory references. https://csapp.cs.cmu.edu/
- **System V AMD64 ABI** — register roles used to map function arguments before reading the memory operands. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — approachable examples for memory operands, arrays, and addressing modes. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Arm Architecture Reference Manual + Apple ARM64 docs** — ARM64 load/store addressing forms and platform ABI details for the appendix output. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
