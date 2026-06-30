---
title: "Control flow: jumps, conditions, and the flags register"
description: Conditional control flow in x86-64 is flags plus jumps. Learn how cmp/test set status, how jcc reads those flags, and how if/else and tailcalls appear in real compiler output.
tags: [assembly, x86-64, control-flow, flags, jumps]
order: 5
updated: 2026-06-30
---
# Control flow: jumps, conditions, and the flags register

Branches are how straight-line instruction streams become decisions. In C you write
`if (x > limit)`; in x86-64 the CPU usually sees one instruction that sets status
bits, followed by another instruction that jumps or falls through based on those bits.
The condition is not a value in a normal register. It is hidden dataflow through
`rflags`.

> The reset: `cmp a, b` does not store a boolean. It sets flags as if it had computed
> `a - b`. The next conditional jump decides whether those flags mean "go elsewhere."

## How it really works

x86-64 control flow is built from a small set of ideas:

| Piece | Mental model | Common examples |
|---|---|---|
| label | named instruction address | `LBB0_2:` |
| `jmp` | unconditional branch | always set `rip` to the target |
| `cmp a, b` | set flags from `a - b` | compare without keeping the subtraction result |
| `test a, b` | set flags from `a & b` | common zero/null check: `test rdi, rdi` |
| `jcc label` | conditional branch | `je`, `jne`, `jl`, `jle`, `jg`, `ja`, `jb` |

The CPU's instruction pointer (`rip`) normally advances to the next instruction. A
jump writes a different next address into that control flow. An unconditional `jmp`
always does it. A conditional jump reads condition flags first.

The flags you read most often are:

| Flag | Meaning after arithmetic/compare |
|---|---|
| `ZF` | zero flag: result was zero |
| `SF` | sign flag: result's high bit was set |
| `OF` | overflow flag: signed overflow happened |
| `CF` | carry flag: unsigned borrow/carry happened |

Signed and unsigned comparisons use the same `cmp`, but different jump mnemonics. For
signed `<=`, x86 uses `jle`, which depends on `ZF`, `SF`, and `OF`. For unsigned
`<=`, it uses `jbe`, which depends on `CF` and `ZF`.

## A real branch

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/control-flow-jumps-conditions-and-flags/`.

```c
static NOINLINE long above_limit(long x, long limit) {
    return (x - limit) * 3;
}

static NOINLINE long at_or_below_limit(long x, long limit) {
    return (limit - x) * 2;
}

long branch_score(long x, long limit) {
    if (x > limit) {
        return above_limit(x, limit);
    }

    return at_or_below_limit(x, limit);
}
```

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
function body copied from the real output of:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_branch_score:                          ## @branch_score
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	cmp	rdi, rsi
	jle	LBB0_2
## %bb.1:
	pop	rbp
	jmp	_above_limit                    ## TAILCALL
LBB0_2:
	pop	rbp
	jmp	_at_or_below_limit              ## TAILCALL
	.cfi_endproc
```

Map the arguments first: `x` arrives in `rdi`, `limit` arrives in `rsi`, and the return
value leaves in `rax`.

Now read the decision:

- `cmp rdi, rsi` sets flags as if the CPU had computed `rdi - rsi`, or `x - limit`.
- `jle LBB0_2` means "jump if signed less-or-equal." If `x <= limit`, execution goes
  to the second path.
- If the jump is not taken, control falls through into `%bb.1`, the `x > limit` path.
- `jmp _above_limit` and `jmp _at_or_below_limit` are tailcalls. The compiler restores
  `rbp` and jumps to the helper instead of calling it and returning through an extra
  stack frame.

The helpers are deliberately small, so the branch is easy to isolate:

```asm
_above_limit:                           ## @above_limit
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	sub	rdi, rsi
	lea	rax, [rdi + 2*rdi]
	pop	rbp
	ret
	.cfi_endproc
```

```asm
_at_or_below_limit:                     ## @at_or_below_limit
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	sub	rsi, rdi
	lea	rax, [rsi + rsi]
	pop	rbp
	ret
	.cfi_endproc
```

Notice the compiler did not materialize a C boolean. There is no `is_greater` local in
memory. The comparison lives in flags for exactly long enough for `jle` to consume it.

## Flags are hidden dataflow

Flags make assembly compact, but they also make it easy to miss dependencies. These
two instructions are connected even though no named register is passed between them:

```asm
cmp     rdi, rsi
jle     LBB0_2
```

If another flag-writing instruction appears between them, the meaning changes:

```asm
cmp     rdi, rsi
add     rax, rcx      ; updates flags too
jle     LBB0_2        ; now reads flags from add, not from cmp
```

Compilers are careful about this. Humans reading assembly should be careful too. Treat
`rflags` as a real register that many instructions write implicitly.

Two patterns show up constantly:

```asm
cmp     rdi, 0
je      was_zero
```

```asm
test    rdi, rdi
je      was_zero
```

Both can implement a zero check. `test rdi, rdi` computes the bitwise AND only for
flags, so if `rdi` is zero, `ZF` becomes 1. It is shorter and idiomatic for null/zero
tests.

## ARM64 appendix

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Relevant function body:

```asm
_branch_score:                          ; @branch_score
	.cfi_startproc
; %bb.0:
	cmp	x0, x1
	b.le	LBB0_2
; %bb.1:
	b	_above_limit
LBB0_2:
	b	_at_or_below_limit
	.cfi_endproc
```

ARM64 has the same broad shape: compare, conditional branch, fallthrough, branch. `x0`
is `x`, `x1` is `limit`, and `cmp x0, x1` sets ARM condition flags from `x0 - x1`.
`b.le` branches on signed less-or-equal. The unconditional `b` instructions are the
ARM64 equivalent of the tailcall jumps in the x86-64 output.

The helpers show a different instruction vocabulary, but the same control-flow split:

```asm
_above_limit:                           ; @above_limit
	.cfi_startproc
; %bb.0:
	sub	x8, x0, x1
	add	x0, x8, x8, lsl #1
	ret
	.cfi_endproc
```

```asm
_at_or_below_limit:                     ; @at_or_below_limit
	.cfi_startproc
; %bb.0:
	sub	x8, x1, x0
	lsl	x0, x8, #1
	ret
	.cfi_endproc
```

The lesson carries across architectures: conditions are usually not heap objects,
locals, or explicit booleans. They are temporary machine status, immediately consumed
by branch instructions or conditional selects.

## Failure modes & trade-offs

- **Signed and unsigned jumps are different.** `jle` is signed. `jbe` is unsigned.
  The same bits can produce a different control-flow edge depending on C type.
- **Flags are fragile state.** `add`, `sub`, `cmp`, `test`, `and`, `or`, `xor`, and many
  other instructions update flags. A jump reads the most recent relevant flags, not
  the nearest human-looking comparison.
- **Fallthrough is a path.** If `jle` is not taken, execution continues at the next
  instruction. Always read both the jump target and the fallthrough block.
- **Tailcalls erase a frame.** `jmp _above_limit` transfers control without pushing a
  new return address. That is correct here, but it changes how the stack looks in a
  debugger.
- **C undefined behavior gives the optimizer room.** If a branch condition depends on
  behavior the C standard does not define, the compiler may remove or rewrite the
  branch in ways that are legal but surprising.
- **Branch prediction is performance-relevant.** A conditional jump can be cheap when
  predicted and expensive when mispredicted. Hot code often trades branches for
  conditional moves, masks, or table lookups.

## In practice

- **Read `cmp` as subtraction without storage.** `cmp rdi, rsi` means "set flags for
  `rdi - rsi`."
- **Name the signedness.** Say "`jle` signed less-or-equal" or "`jbe` unsigned
  below-or-equal" out loud until the distinction sticks.
- **Trace fallthrough first.** Conditional branches always have two destinations: the
  label and the next instruction.
- **Watch for `test reg, reg`.** It usually means "is this register zero?" without
  modifying the register itself.
- **Connect branches to source types.** C signedness, integer promotions, and undefined
  behavior influence which jump the compiler can use. Pair this with
  [[lowlevel/c-from-the-metal/integer-promotions-and-implicit-conversions|integer promotions and implicit conversions]]
  and [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|undefined behavior]].

**Connects to:** [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Why read assembly — Compiler Explorer as a daily tool]] · [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|x86-64 registers and the register file]] · [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|The core instruction set: mov, arithmetic, lea]] · [[lowlevel/assembly-and-compiler-output/stack-at-the-assembly-level|The stack at the assembly level]] · [[lowlevel/machine-model/registers-and-the-isa|Registers & the ISA]]

## Sources

- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 and Vol. 2 — authoritative semantics for `cmp`, `test`, `jcc`, `jmp`, flags, and `ret`. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Felix Cloutier — x86 and amd64 instruction reference** — quick lookup for conditional jumps and flag effects; verify edge cases against Intel SDM. https://www.felixcloutier.com/x86/
- **System V AMD64 ABI** — register roles and call/return conventions used to read the example output. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 — condition codes, branches, and machine-level control flow. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — approachable examples for jumps, comparisons, and condition codes. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Arm Architecture Reference Manual + Apple ARM64 docs** — ARM64 condition flags, conditional branches, and platform ABI details for the appendix output. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
