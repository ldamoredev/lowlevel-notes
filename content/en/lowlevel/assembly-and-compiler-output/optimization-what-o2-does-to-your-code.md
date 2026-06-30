---
title: "Optimization: what `-O2` does to your code"
description: Optimized compiler output is not source code with fewer lines. Learn how -O2 removes dead work, folds constants, keeps values in registers, rewrites branches, and still preserves observable behavior.
tags: [assembly, compiler, optimization, o2, x86-64]
order: 10
updated: 2026-06-30
---
# Optimization: what `-O2` does to your code

`-O2` asks the compiler to preserve your program's observable behavior, not its source
shape. Locals can disappear, constants can be folded, redundant arithmetic can be
deleted, branches can become conditional moves, and stack slots can turn into register
lifetimes. Reading optimized assembly means following values and side effects, not
expecting a line-by-line translation from C.

> The reset: optimized code is a proof result. If the compiler can prove some work is
> unnecessary under the C rules, it can remove or rewrite that work.

## How it really works

At `-O0`, compilers usually prioritize debuggability and source-shaped output. They keep
many locals in stack slots, emit obvious branches, and avoid most rewrites. At `-O2`,
the compiler spends more time improving ordinary runtime code.

Common `-O2` transformations:

| Transformation | What you see in assembly |
|---|---|
| constant folding | `40 + 2` becomes immediate `42` |
| dead-code elimination | values that cannot affect behavior vanish |
| register allocation | locals become registers instead of `[rbp - offset]` slots |
| strength reduction | multiply by 2 becomes shift or `lea` |
| branch conversion | simple `if` can become `cmov` or `csel` |
| inlining | a small call may disappear into the caller |

The legal boundary is the C abstract machine. Volatile accesses, function calls with
unknown side effects, atomics, I/O, and memory that may be observed constrain the
optimizer. Undefined behavior expands the optimizer's room because the compiler does not
need to preserve behavior that C does not define.

## A real `-O0` versus `-O2` trace

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/optimization-what-o2-does-to-your-code/`.

```c
NOINLINE long optimize_me(long x, long y) {
    long twice = x * 2;
    long dead = y * 0;
    long folded = 40 + 2;

    if (twice > y) {
        return twice + folded + dead;
    }

    return y - twice + folded + dead;
}
```

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
`-O0` output:

```sh
gcc -S -O0 -fno-omit-frame-pointer -masm=intel demo.c -o demo.O0.s
```

```asm
_optimize_me:                           ## @optimize_me
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	qword ptr [rbp - 16], rdi
	mov	qword ptr [rbp - 24], rsi
	mov	rax, qword ptr [rbp - 16]
	shl	rax, 1
	mov	qword ptr [rbp - 32], rax
	imul	rax, qword ptr [rbp - 24], 0
	mov	qword ptr [rbp - 40], rax
	mov	qword ptr [rbp - 48], 42
	mov	rax, qword ptr [rbp - 32]
	cmp	rax, qword ptr [rbp - 24]
	jle	LBB0_2
```

`-O0` keeps source-shaped locals: `x`, `y`, `twice`, `dead`, `folded`, and the return
slot all appear as stack locations. It even emits `imul ... , 0` for `dead = y * 0`
because the point of `-O0` is not to aggressively reason about useless work.

Now the same function at `-O2`:

```sh
gcc -S -O2 -fno-omit-frame-pointer -masm=intel demo.c -o demo.O2.s
```

```asm
_optimize_me:                           ## @optimize_me
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rdi + rdi]
	sub	rsi, rax
	cmovge	rax, rsi
	add	rax, 42
	pop	rbp
	ret
	.cfi_endproc
```

Map the arguments: `x` is in `rdi`, `y` is in `rsi`, return value leaves in `rax`.

Then read the optimized dataflow:

- `lea rax, [rdi + rdi]` computes `twice = x * 2`.
- `dead = y * 0` is gone because it is always zero and has no side effect.
- `folded = 40 + 2` became the immediate `42`.
- `sub rsi, rax` computes `y - twice`.
- `cmovge rax, rsi` selects the else value when the earlier arithmetic/flags say that
  `twice <= y`. The source `if` no longer appears as a branch.
- `add rax, 42` adds the folded constant once after the selection.

This is the same function, but not the same shape. The optimizer found a smaller
equivalent expression.

## Why the branch became `cmov`

The source had:

```c
if (twice > y) {
    return twice + 42;
}

return y - twice + 42;
```

The optimized sequence computes both candidate bases cheaply: `twice` in `rax` and
`y - twice` in `rsi`. Then `cmovge` conditionally copies `rsi` into `rax` instead of
jumping. This can avoid a branch misprediction for tiny decision logic.

That does not mean `cmov` is always better. For expensive alternatives, predictable
branches, or code with memory side effects, a branch may be the right output. `-O2` is a
bundle of heuristics and analyses, not a promise to use one instruction pattern.

## ARM64 appendix

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Relevant function body:

```asm
_optimize_me:                           ; @optimize_me
	.cfi_startproc
; %bb.0:
	lsl	x8, x0, #1
	subs	x9, x1, x8
	csel	x8, x8, x9, lt
	add	x0, x8, #42
	ret
	.cfi_endproc
```

ARM64 tells the same story in a different vocabulary. `x0` is `x`, `x1` is `y`, and the
return value leaves in `x0`. `lsl x8, x0, #1` computes `x * 2`. `subs x9, x1, x8`
computes `y - twice` and sets flags. `csel x8, x8, x9, lt` conditionally selects the
right candidate. `add x0, x8, #42` adds the folded constant.

The cross-architecture lesson is the point: source variables are gone; value flow
remains.

## Failure modes & trade-offs

- **Debugging optimized code is different.** Variables may be optimized out, merged, or
  live only in registers for part of a function.
- **Undefined behavior matters more at `-O2`.** If your C relies on signed overflow,
  invalid pointers, out-of-bounds access, or strict-aliasing violations, optimization can
  produce shocking but legal output.
- **Not all work can be removed.** Volatile operations, atomics, calls with side effects,
  and observable memory writes constrain rewrites.
- **Inlining changes stack traces.** Calls can disappear, which is good for runtime and
  confusing for naive backtrace expectations.
- **Branchless is not automatically faster.** Conditional moves/selects avoid branches
  but may compute more values than a branch would.
- **Compiler versions differ.** The exact instruction sequence is not a stable API.
  Read the intent, not only the mnemonic list.

## In practice

- **Use `-O2` for performance questions.** `-O0` output is useful for mechanics, but it
  often lies about real register pressure and instruction count.
- **Track values, not source locals.** Ask where the return value comes from, not where
  the variable name went.
- **Look for deleted work.** If something is gone, ask whether it had any observable
  effect under C rules.
- **Compare one function at both levels.** A local `-O0`/`-O2` diff is one of the fastest
  ways to learn compiler transformations.
- **Connect this to UB.** Optimization is where the contract from
  [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|undefined behavior]]
  becomes very concrete.

**Connects to:** [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Why read assembly — Compiler Explorer as a daily tool]] · [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|The core instruction set: mov, arithmetic, lea]] · [[lowlevel/assembly-and-compiler-output/control-flow-jumps-conditions-and-flags|Control flow: jumps, conditions, and the flags register]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: the contract]] · [[lowlevel/c-from-the-metal/more-ub-signed-overflow-aliasing-and-sequencing|More UB: signed overflow, aliasing, sequencing]]

## Sources

- **LLVM Clang command guide** — optimization levels, code-generation options, and how Clang treats `-O0` through `-O3`. https://clang.llvm.org/docs/CommandGuide/clang.html
- **GCC Optimize Options** — GCC's documented optimization-level behavior and flags enabled by `-O2`. https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 and 5 — optimized machine code, conditionals, procedures, and performance reasoning. https://csapp.cs.cmu.edu/
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 and Vol. 2 — instruction semantics for `lea`, `cmov`, arithmetic, and flags. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Felix Cloutier — x86 and amd64 instruction reference** — quick lookup for `cmov`, `lea`, `sub`, and flag behavior; verify edge cases against Intel SDM. https://www.felixcloutier.com/x86/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — ARM64 `csel`, shifts, flag-setting arithmetic, and ABI details for the appendix output. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
