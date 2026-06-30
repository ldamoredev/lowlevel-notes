---
title: "Inline assembly (and when to reach for it)"
description: Inline assembly is a contract with the compiler, not just text pasted into the instruction stream. Learn constraints, clobbers, volatile, memory barriers, and why intrinsics or builtins are usually safer.
tags: [assembly, inline-asm, compiler, constraints, clobbers]
order: 11
updated: 2026-06-30
---
# Inline assembly (and when to reach for it)

Inline assembly lets C source contain target-specific assembly, but the hard part is not
the mnemonic. The hard part is telling the compiler what the assembly reads, writes, and
clobbers. If that contract is wrong, the optimizer may legally move, remove, duplicate,
or reorder surrounding code in ways that break your intent. Reach for inline assembly
only when a builtin, intrinsic, library API, or separate `.s` file is not the better tool.

> The reset: inline asm is not invisible magic. It is a compiler boundary with declared
> inputs, outputs, clobbers, and side effects.

## How it really works

GCC/Clang extended asm has the shape:

```c
__asm__ volatile (
    "template"
    : outputs
    : inputs
    : clobbers
);
```

The pieces mean:

| Piece | Meaning |
|---|---|
| template | assembly text emitted for the target assembler |
| outputs | C lvalues the asm writes |
| inputs | C values the asm reads |
| clobbers | registers, flags, or memory effects not otherwise described |
| `volatile` | tells the compiler the asm has side effects and must not be deleted as dead |

The compiler does not understand the semantics of your assembly template. It understands
the contract you write around it. The constraints and clobbers are the bridge between
C optimization and the raw instruction text.

## A real memory-clobber demo

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/inline-assembly-and-when-to-reach-for-it/`.

```c
static NOINLINE long plain_twice(const long *value) {
    long first = *value;
    long second = *value;
    return first + second;
}

static NOINLINE long barrier_twice(const long *value) {
    long first = *value;
    __asm__ volatile("" ::: "memory");
    long second = *value;
    return first + second;
}
```

The inline asm string is empty. It emits no machine instruction. The interesting part is
the `"memory"` clobber: it tells the compiler that memory may have changed across this
point, so it must not reuse a previous load as if memory were unchanged.

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
output:

```sh
gcc -S -O2 -fno-omit-frame-pointer -masm=intel demo.c -o demo.s
```

```asm
_plain_twice:                           ## @plain_twice
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	rax, qword ptr [rdi]
	add	rax, rax
	pop	rbp
	ret
	.cfi_endproc
```

`plain_twice` loads once and doubles the register. The compiler proved that two ordinary
loads from the same `const long *` can be represented as one load plus `add rax, rax`.

Now the version with an inline asm memory clobber:

```asm
_barrier_twice:                         ## @barrier_twice
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	rax, qword ptr [rdi]
	## InlineAsm Start
	## InlineAsm End
	add	rax, qword ptr [rdi]
	pop	rbp
	ret
	.cfi_endproc
```

No instruction appears between `InlineAsm Start` and `InlineAsm End`, but the compiler
still reloads memory after the barrier. The inline asm changed optimization because the
contract said memory might have changed.

## What `volatile` and `"memory"` do not mean

`asm volatile` means the compiler should not delete the asm as dead and should preserve
its relative position with respect to other volatile asm. It is not a CPU memory fence by
itself. The `"memory"` clobber is a compiler barrier for memory accesses: it constrains
compiler reordering around the asm. It is also not a hardware fence by itself.

For real inter-thread ordering, use C atomics or target fence instructions through
well-defined intrinsics/builtins unless you are deliberately writing low-level runtime or
kernel code and know the platform memory model.

## ARM64 appendix

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Relevant output:

```asm
_plain_twice:                           ; @plain_twice
	.cfi_startproc
; %bb.0:
	ldr	x8, [x0]
	lsl	x0, x8, #1
	ret
	.cfi_endproc
```

```asm
_barrier_twice:                         ; @barrier_twice
	.cfi_startproc
; %bb.0:
	ldr	x8, [x0]
	; InlineAsm Start
	; InlineAsm End
	ldr	x9, [x0]
	add	x0, x9, x8
	ret
	.cfi_endproc
```

Same idea: no target instruction was emitted by the empty asm, but the `"memory"`
clobber forced a second load. ARM64 uses `x0` for the pointer argument and return value,
with `x8`/`x9` as scratch registers chosen by the compiler.

## When to reach for inline asm

Good reasons are narrow:

- You need a target instruction unavailable through C, a builtin, or an intrinsic.
- You are writing a low-level runtime, kernel, context switch, syscall stub, or port I/O
  boundary.
- You need a compiler barrier and you understand that it is not a hardware fence.
- You are experimenting and explicitly inspecting compiler output.

Bad reasons are common:

- You want to force "faster" code without measuring.
- You want to hide C undefined behavior from the optimizer.
- You do not want to learn the available intrinsic or builtin.
- You are trying to write large blocks of assembly inside C instead of using a separate
  assembly file with a clear ABI boundary.

## Failure modes & trade-offs

- **Wrong constraints miscompile code.** If the compiler does not know what the asm reads
  or writes, it can allocate overlapping registers or reuse stale values.
- **Missing clobbers are bugs.** If your asm changes flags, memory, or registers not
  listed as outputs, say so.
- **`volatile` is not a memory model.** It prevents some compiler deletion/reordering of
  the asm itself; it does not give atomicity or cross-core ordering.
- **Assembly dialects are compiler/target-specific.** GCC/Clang extended asm is not
  ISO C, and MSVC has different rules.
- **Inline asm blocks optimization.** The compiler cannot see through your template, so
  it may lose scheduling, register-allocation, or vectorization opportunities.
- **Portability drops fast.** Even x86-64 and ARM64 need different templates, register
  names, constraints, and sometimes different semantics.

## In practice

- **Prefer builtins and intrinsics first.** They let the compiler understand the operation.
- **Keep asm tiny.** One instruction or one narrow boundary is easier to constrain than a
  mini-program hidden in a string.
- **Declare the full contract.** Outputs, inputs, `cc`, `memory`, and tied operands are
  the real interface.
- **Inspect optimized output.** Inline asm must be checked at `-O2`, where the optimizer
  will test your contract.
- **Move big assembly out of line.** A separate `.s` file plus a normal C prototype often
  gives a cleaner ABI boundary.

**Connects to:** [[lowlevel/assembly-and-compiler-output/optimization-what-o2-does-to-your-code|Optimization: what -O2 does to your code]] · [[lowlevel/assembly-and-compiler-output/system-v-amd64-calling-convention|The System V AMD64 calling convention]] · [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|x86-64 registers and the register file]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: the contract]] · [[lowlevel/concurrency-and-performance/index|Concurrency & Performance]]

## Sources

- **GCC — Extended Asm** — authoritative syntax for GCC extended asm constraints, clobbers, volatile asm, and memory clobbers. https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
- **Clang Language Extensions — Inline Assembly** — Clang's compatibility and target-specific behavior for GNU-style inline asm. https://clang.llvm.org/docs/LanguageExtensions.html#inline-assembly
- **System V AMD64 ABI** — register roles and ABI boundaries that inline asm must respect on x86-64 Unix-like targets. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 and Vol. 2 — instruction semantics and flags for x86-64 templates. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Arm Architecture Reference Manual + Apple ARM64 docs** — ARM64 register, instruction, and calling-convention details for target-specific inline asm. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
- **cppreference — Atomics** — the C-level alternative for inter-thread ordering that should usually replace hand-rolled asm fences. https://en.cppreference.com/w/c/atomic
