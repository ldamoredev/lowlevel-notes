---
title: "Why read assembly — Compiler Explorer as a daily tool"
description: Assembly is the fastest way to check what the compiler really emitted. Use Compiler Explorer and local -S/objdump workflows to answer performance and behavior questions without guessing.
tags: [assembly, compiler-explorer, x86-64, arm64, compiler-output]
order: 1
updated: 2026-06-22
---
# Why read assembly — Compiler Explorer as a daily tool

The compiler is not a black box; it is a machine-code generator with inspectable
output. Reading that output answers two daily questions: "what does this code really
do?" and "is this likely to be fast?" without folklore. You do not need to write much
assembly to gain a lot from reading it. A small amount of fluency turns compiler
output into another debugging surface, right next to tests, logs, and a profiler.

> The reset: C is not what the CPU runs. C is what the compiler consumes. Assembly is
> the first readable form of what the CPU will execute.

## How it really works

[Compiler Explorer](https://godbolt.org) is the fastest feedback loop for this skill:
paste a small C function, choose a compiler, choose the target and flags, and read the
assembly pane. The useful habit is to make the experiment tiny. Change one line, one
type, or one flag, then watch the emitted instructions change.

Read a Compiler Explorer pane in this order:

- **Compiler and flags first.** `gcc -O2 -masm=intel` and `clang -O0` are different
  experiments. Keep the flags visible when you share a result.
- **Source-to-assembly coloring.** The highlighting maps source lines to instruction
  ranges. It is not a proof of one-to-one correspondence, but it tells you which C
  expression caused which region of output.
- **Function boundary next.** Find the label for the function you care about and
  ignore unrelated startup, constants, exception tables, and library noise.
- **Registers before mnemonics.** Identify the incoming argument registers and the
  outgoing return register first. That anchors the rest of the reading.
- **Hot path last.** For performance questions, focus on the loop body, branches,
  loads/stores, calls, and spills. Pretty setup code outside the hot path rarely
  matters.

This ties directly back to [[lowlevel/machine-model/registers-and-the-isa|registers & the ISA]]: the compiler has to express your high-level code using a fixed
register set, an instruction set, memory operands, and a calling convention. Reading
assembly is reading that translation.

## A tiny function, compiled

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/why-read-assembly-compiler-explorer/`. The core
function is intentionally boring:

```c
long weighted_score(long packets, long cost) {
    return packets * 3 + cost * 5 + 7;
}
```

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
function body copied from the real output of:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_weighted_score:                        ## @weighted_score
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rdi + 2*rdi]
	lea	rcx, [rsi + 4*rsi]
	add	rax, rcx
	add	rax, 7
	pop	rbp
	ret
	.cfi_endproc
```

Start with the registers, not the cleverness. Under the usual x86-64 C calling
convention on this platform, the first two integer arguments arrive in `rdi` and
`rsi`, and the return value leaves in `rax`. That is the same mental map from
[[lowlevel/machine-model/registers-and-the-isa|registers & the ISA]].

The interesting instructions:

- `push rbp`, `mov rbp, rsp`, `pop rbp` are the frame-pointer prologue/epilogue this
  compiler keeps by default for this target. With different flags, they can disappear.
- `lea rax, [rdi + 2*rdi]` computes `packets * 3`. Here `lea` is arithmetic; it does
  not load from memory.
- `lea rcx, [rsi + 4*rsi]` computes `cost * 5` into a scratch register.
- `add rax, rcx` and `add rax, 7` finish the expression.
- `ret` returns to the caller, with the answer already in `rax`.

That is the payoff: the source said "multiply by constants"; the compiler emitted
address-generation arithmetic. You did not need to guess whether this became actual
`imul` instructions. You looked.

## Reproduce it locally

Compiler Explorer is for fast exploration. Your local compiler is the final authority
for the binary you are actually shipping.

```sh
cd examples/assembly-and-compiler-output/why-read-assembly-compiler-explorer

gcc -O0 -Wall -Wextra demo.c -o demo
./demo

gcc -S -O2 -masm=intel demo.c -o demo.s
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Use `-S` when you want the compiler's assembly file. Use `objdump` when you already
have an object file or executable and want to disassemble the bytes:

```sh
gcc -g -O2 demo.c -o demo
objdump -d demo
objdump -d -S demo
```

`-g` asks the compiler to include debug information, and `objdump -S` uses that
information to intermix source and disassembly when possible. On macOS you may reach
for `llvm-objdump` or `otool -tvV` instead; the workflow is the same: compile with
debug info, disassemble the binary, and line up source with instructions.

Two syntax families show up constantly:

| | Intel syntax | AT&T syntax |
|---|---|---|
| Operand order | destination first: `add rax, rcx` | source first: `addq %rcx, %rax` |
| Registers | `rax`, `rdi` | `%rax`, `%rdi` |
| Immediates | `7` | `$7` |
| Memory | `[rdi + 2*rdi]` | `(%rdi,%rdi,2)` |
| Size | often inferred from operands | often in mnemonic suffix: `q`, `l`, `w`, `b` |

This atlas uses Intel syntax for x86-64 because it matches common Compiler Explorer
settings and keeps memory operands visually close to the addressing-mode formula.

## ARM64 appendix

The same `demo.c` was also cross-compiled on this machine with:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Relevant function body:

```asm
_weighted_score:                        ; @weighted_score
	.cfi_startproc
; %bb.0:
	add	x8, x0, x0, lsl #1
	add	x9, x1, x1, lsl #2
	add	x8, x8, x9
	add	x0, x8, #7
	ret
	.cfi_endproc
```

AArch64 makes the dataflow feel more regular. The first two integer arguments arrive
in `x0` and `x1`; the return value also leaves in `x0`. `add x8, x0, x0, lsl #1`
means "add `x0` to `x0 << 1`", so it computes `packets * 3`. `add x9, x1, x1, lsl #2`
computes `cost * 5`. Then the function adds the partial sums, adds `7`, and returns.

Notice the differences from x86-64: no `rbp`/`rsp` frame setup in this tiny leaf
function, a larger general-purpose register file, shifted-register operands built into
the arithmetic instruction, and `x0` serving as both first argument and return value.
The same C expression became different instructions because the target
[[lowlevel/machine-model/registers-and-the-isa|ISA]] changed.

## Failure modes & trade-offs

- **Assembly is not unique.** Compiler, compiler version, target triple, ABI, CPU
  tuning, and flags all matter. `gcc -O2`, Apple clang `-O2`, and GCC on Linux can all
  produce different correct answers.
- **Do not over-read `-O0`.** Unoptimized output is built for debuggability, not for
  representing the code the optimizer would choose. It often spills variables to the
  stack and preserves source-shaped control flow.
- **`-O2` changes the shape, not the contract.** The optimizer can fold constants,
  reorder work, inline calls, and delete dead code as long as the observable behavior
  obeys the C abstract machine. If your C has undefined behavior, that contract is
  already broken.
- **C++ names are mangled.** A C function named `weighted_score` stays recognizable.
  A C++ function may become a long encoded symbol unless you use `extern "C"` for the
  experiment boundary.
- **Symbols are platform-shaped.** Mach-O commonly prefixes C symbols with `_`
  (`_weighted_score`); ELF/Linux usually shows `weighted_score`. Learn the convention
  before assuming you are looking at a different function.
- **Source mapping needs debug info.** Use `-g` with `objdump -S` when you want source
  lines next to disassembly. Without debug info, addresses and symbols are often all
  you get.

## In practice

- **Keep Compiler Explorer open while designing small C changes.** It is especially
  good for checking integer conversions, structs passed by value, tiny loops, function
  calls, and whether a helper got inlined.
- **Move one variable at a time.** If the output changes too much, shrink the example.
  Assembly is readable when the experiment is small.
- **Read registers first.** Seeing `rdi`/`rsi`/`rax` or `x0`/`x1`/`x0` turns noise into
  a dataflow sketch.
- **Use local assembly for final claims.** Compiler Explorer is a lab bench. The
  compiler, flags, platform, and binary you ship are the ground truth.
- **Connect assembly back to the pipeline.** `-S` is just one stop in the path from
  source to execution; object files, linking, loading, and runtime behavior are the
  next layers from [[lowlevel/machine-model/how-source-becomes-execution|how source becomes execution]].

**Connects to:** [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/machine-model/registers-and-the-isa|Registers & the ISA]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|The CPU: fetch–decode–execute]] · [[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **Compiler Explorer** — the daily tool for comparing source with compiler output across compilers, flags, and targets. https://godbolt.org/
- **GCC Manual + GNU Binutils `objdump` manual** — `-S` for compiler-emitted assembly, then `objdump -d/-S` for disassembling existing objects and binaries. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html · https://sourceware.org/binutils/docs/binutils/objdump.html
- **System V AMD64 ABI** — the register calling convention behind `rdi`, `rsi`, and `rax` on Unix-like x86-64 targets. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Jonathan Bartlett — *Programming from the Ground Up*** — assembly from first principles, useful for building the "CPU executes instructions" mental model. https://savannah.nongnu.org/projects/pgubook/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — approachable x86-64 register, instruction, and calling-convention reference with examples. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Intel SDM + Felix Cloutier x86 reference** — Intel is the authority; Felix Cloutier is the fast lookup for `lea`, `add`, `ret`, and friends. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html · https://www.felixcloutier.com/x86/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — authoritative AArch64 semantics plus Apple-platform ABI details for `clang -arch arm64` output. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
