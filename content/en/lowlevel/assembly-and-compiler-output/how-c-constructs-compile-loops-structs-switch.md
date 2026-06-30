---
title: "How C constructs compile: loops, structs, `switch`"
description: Loops become branches over labels, structs become byte offsets, and switch statements become comparisons, jump tables, or conditional selects. Learn to read all three in real compiler output.
tags: [assembly, x86-64, c, loops, structs, switch]
order: 9
updated: 2026-06-30
---
# How C constructs compile: loops, structs, `switch`

C syntax is high-level compared with the CPU, but most constructs lower to a few
machine patterns. A loop is a label, a condition, a body, and a branch back. A struct
field is a byte offset from a base pointer. A `switch` is a dispatch strategy chosen by
the compiler: compare chain, jump table, conditional moves/selects, or a mix. Once you
see those shapes, C control flow and data layout become readable in assembly.

> The reset: assembly does not know "for loop," "struct field," or "`switch` case." It
> knows addresses, offsets, comparisons, jumps, and arithmetic.

## How it really works

The source constructs map roughly like this:

| C construct | Machine shape |
|---|---|
| `for` / `while` | loop header label, condition, body, increment, branch |
| `items[i]` | base pointer plus `i * sizeof item` |
| `p->field` | load/store at fixed byte offset from `p` |
| `switch` | compare chain, jump table, branch tree, or conditional select |

Optimization changes the visible shape. The compiler may invert a condition, combine
the increment with the exit test, hoist pointer arithmetic, or replace a branch with a
conditional move. Your job is not to reconstruct the exact C syntax. Your job is to
recover the dataflow and control-flow edges.

## A real loop over structs

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/how-c-constructs-compile-loops-structs-switch/`.

```c
struct Packet {
    int tag;
    long value;
    int scale;
};

NOINLINE long classify_and_sum(const struct Packet *items, size_t count) {
    long total = 0;

    for (size_t i = 0; i < count; i++) {
        long scaled = items[i].value * items[i].scale;

        switch (items[i].tag) {
        case 0:
            total += scaled;
            break;
        case 1:
            total -= scaled;
            break;
        case 2:
            total += items[i].value;
            break;
        default:
            total += 7;
            break;
        }
    }

    return total;
}
```

On this target, `struct Packet` has this layout:

| Field | Offset | Reason |
|---|---:|---|
| `tag` | 0 | `int` starts the object |
| padding | 4 | align the next `long` |
| `value` | 8 | `long` needs 8-byte alignment |
| `scale` | 16 | `int` after the `long` |
| tail padding | 20 | make array stride 24 |

The stride is 24 bytes, so `items[i + 1]` is 24 bytes after `items[i]`.

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
function body copied from the real output of:

```sh
gcc -S -O2 -fno-omit-frame-pointer -masm=intel demo.c -o demo.s
```

```asm
_classify_and_sum:                      ## @classify_and_sum
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	test	rsi, rsi
	je	LBB0_10
## %bb.1:
	add	rdi, 16
	xor	eax, eax
	jmp	LBB0_4
	.p2align	4, 0x90
LBB0_8:                                 ##   in Loop: Header=BB0_4 Depth=1
	add	rax, rcx
LBB0_3:                                 ##   in Loop: Header=BB0_4 Depth=1
	add	rdi, 24
	dec	rsi
	je	LBB0_11
LBB0_4:                                 ## =>This Inner Loop Header: Depth=1
	mov	rcx, qword ptr [rdi - 8]
	mov	edx, dword ptr [rdi - 16]
	cmp	edx, 2
	je	LBB0_8
## %bb.5:                               ##   in Loop: Header=BB0_4 Depth=1
	movsxd	r8, dword ptr [rdi]
	imul	rcx, r8
	cmp	edx, 1
	je	LBB0_9
## %bb.6:                               ##   in Loop: Header=BB0_4 Depth=1
	test	edx, edx
	je	LBB0_8
## %bb.2:                               ##   in Loop: Header=BB0_4 Depth=1
	add	rax, 7
	jmp	LBB0_3
	.p2align	4, 0x90
LBB0_9:                                 ##   in Loop: Header=BB0_4 Depth=1
	sub	rax, rcx
	jmp	LBB0_3
LBB0_10:
	xor	eax, eax
LBB0_11:
	pop	rbp
	ret
	.cfi_endproc
```

Map the arguments first: `items` arrives in `rdi`, `count` arrives in `rsi`, and the
return value leaves in `rax`.

Now decode the source constructs:

- `test rsi, rsi; je LBB0_10` is the empty-loop check. If `count == 0`, return zero.
- `xor eax, eax` initializes `total = 0`.
- `add rdi, 16` makes `rdi` point at the `scale` field of the current item. This lets
  the compiler address nearby fields with negative offsets.
- `mov rcx, qword ptr [rdi - 8]` loads `value`, because `scale_offset - 8 == value_offset`.
- `mov edx, dword ptr [rdi - 16]` loads `tag`, because `scale_offset - 16 == tag_offset`.
- `movsxd r8, dword ptr [rdi]` loads signed `scale` and widens it to 64 bits.
- `imul rcx, r8` computes `scaled = value * scale`.
- `cmp edx, 2`, `cmp edx, 1`, and `test edx, edx` implement the `switch`.
- `add rdi, 24` advances to the next `struct Packet`.
- `dec rsi; je LBB0_11` decrements the remaining count and exits when it reaches zero.

The compiler did not preserve the source order literally. It checks `case 2` before it
computes `scaled`, because that case only needs `value`. That is legal because C's
observable behavior is unchanged.

## Reading the switch

For this small `switch`, the compiler chose a compare chain:

```asm
	cmp	edx, 2
	je	LBB0_8
	movsxd	r8, dword ptr [rdi]
	imul	rcx, r8
	cmp	edx, 1
	je	LBB0_9
	test	edx, edx
	je	LBB0_8
	add	rax, 7
```

`edx` holds `tag`. If `tag == 2`, the code jumps to the path that adds `value`. If not,
it computes `scaled`. `tag == 1` subtracts `scaled`; `tag == 0` adds `scaled`; anything
else adds 7.

For dense switches with many cases, compilers often use jump tables. For tiny switches,
branch chains or conditional moves are often cheaper and smaller.

## ARM64 appendix

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Relevant function body:

```asm
_classify_and_sum:                      ; @classify_and_sum
	.cfi_startproc
; %bb.0:
	cbz	x1, LBB0_4
; %bb.1:
	mov	x8, x0
	mov	x0, #0
	add	x8, x8, #16
LBB0_2:                                 ; =>This Inner Loop Header: Depth=1
	ldur	x9, [x8, #-8]
	ldrsw	x10, [x8]
	mul	x10, x9, x10
	ldur	w11, [x8, #-16]
	add	x9, x9, x0
	sub	x12, x0, x10
	add	x13, x0, #7
	add	x10, x10, x0
	cmp	w11, #0
	csel	x10, x13, x10, ne
	cmp	w11, #1
	csel	x10, x12, x10, eq
	cmp	w11, #2
	csel	x0, x9, x10, eq
	add	x8, x8, #24
	subs	x1, x1, #1
	b.ne	LBB0_2
; %bb.3:
	ret
LBB0_4:
	mov	x0, #0
	ret
	.cfi_endproc
```

ARM64 makes the same loop/struct facts visible: `x0` starts as `items`, `x1` is
`count`, `x8` becomes a pointer to the current `scale` field, and the pointer advances
by 24 bytes each iteration. `ldur x9, [x8, #-8]` loads `value`, `ldur w11, [x8, #-16]`
loads `tag`, and `ldrsw x10, [x8]` loads and sign-extends `scale`.

The switch lowering differs. Instead of a branch chain, ARM64 uses `csel` conditional
selects to choose the next `total` value after comparisons. Same C, different machine
strategy.

## Failure modes & trade-offs

- **Expecting source order.** The compiler may reorder loads, comparisons, and arithmetic
  as long as observable behavior is preserved.
- **Forgetting struct padding.** `items[i]` advances by `sizeof(struct Packet)`, not by
  the sum of field sizes you imagined.
- **Assuming every switch is a jump table.** Small switches often compile to branches or
  conditional selects. Dense large switches often become jump tables.
- **Missing signed extension.** `movsxd` and `ldrsw` matter: `scale` is a signed `int`
  widened to 64-bit `long` arithmetic.
- **Confusing loop count with index.** Optimized loops may use a decrementing remaining
  count instead of an increasing `i`.
- **Reading labels as source blocks.** Labels are compiler-generated control-flow
  targets, not original C block names.

## In practice

- **Start from layout.** Know `sizeof` and offsets before reading struct access.
- **Find the induction variable.** It may be a pointer stepping by stride, not an
  explicit integer index.
- **Trace one iteration.** Follow loads, case dispatch, accumulator update, pointer
  advance, and exit test.
- **Treat switch lowering as a choice.** Compare chain, jump table, and conditional
  select are all valid outputs for the same source idea.
- **Connect fields to addressing modes.** This note is where
  [[lowlevel/assembly-and-compiler-output/addressing-modes-and-memory-operands|addressing modes and memory operands]]
  become source-level C again.

**Connects to:** [[lowlevel/assembly-and-compiler-output/addressing-modes-and-memory-operands|Addressing modes and memory operands]] · [[lowlevel/assembly-and-compiler-output/control-flow-jumps-conditions-and-flags|Control flow: jumps, conditions, and the flags register]] · [[lowlevel/assembly-and-compiler-output/system-v-amd64-calling-convention|The System V AMD64 calling convention]] · [[lowlevel/c-from-the-metal/structs-unions-and-bitfields|Structs, unions, and bitfields]] · [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alignment & padding]]

## Sources

- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 and Vol. 2 — instruction semantics for branches, compares, loads, sign extension, and integer multiplication. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **System V AMD64 ABI** — argument registers and data-layout expectations used to read the generated x86-64 output. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 — machine-level loops, conditionals, switch statements, and struct layout. https://csapp.cs.cmu.edu/
- **Felix Cloutier — x86 and amd64 instruction reference** — quick lookup for `test`, `cmp`, `imul`, `movsxd`, and jumps; verify edge cases against Intel SDM. https://www.felixcloutier.com/x86/
- **ISO/IEC 9899 (WG14 C standard working drafts)** — source-level rules for structs, array indexing, integer conversions, and switch semantics. https://www.open-std.org/jtc1/sc22/wg14/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — ARM64 load/store addressing, `csel`, branches, and platform ABI details for the appendix output. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
