---
title: "Stack frames: function prologue and epilogue"
description: A stack frame is the per-call slice of stack storage a function uses. Learn the x86-64 prologue, epilogue, frame pointer, local slots, calls, and unwind metadata from real compiler output.
tags: [assembly, x86-64, stack-frame, prologue, epilogue]
order: 7
updated: 2026-06-30
---
# Stack frames: function prologue and epilogue

A stack frame is the part of the stack owned by one active function call. It is where a
function can save the caller's frame pointer, reserve local storage, spill registers,
and preserve the state it must restore before returning. The prologue builds that frame;
the epilogue tears it down. Reading those two bookends lets you separate call-frame
mechanics from the actual computation.

> The reset: a stack frame is not a C object. It is an ABI-shaped region of memory around
> `rsp`, often anchored by `rbp`, that exists only for one dynamic call.

## How it really works

The classic x86-64 frame-pointer shape is:

```asm
push    rbp
mov     rbp, rsp
sub     rsp, N
```

Then, before returning:

```asm
add     rsp, N
pop     rbp
ret
```

Read it as a small protocol:

| Instruction | Role |
|---|---|
| `push rbp` | save the caller's frame pointer |
| `mov rbp, rsp` | make a stable base for this frame |
| `sub rsp, N` | reserve `N` bytes of local/spill space |
| `add rsp, N` | release that local/spill space |
| `pop rbp` | restore the caller's frame pointer |
| `ret` | pop the return address into `rip` |

The compiler may omit some of this in optimized code. A tiny leaf function might need no
stack storage. A function compiled with frame-pointer omission may use `rsp` directly
and describe unwinding through metadata instead. But when you see this shape, it is the
function's entry and exit contract written in instructions.

## A real prologue and epilogue

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/stack-frames-function-prologue-and-epilogue/`.

```c
static NOINLINE long add_bias(long value) {
    return value + 7;
}

long framed_total(long a, long b) {
    volatile long local = a + b;
    long adjusted = add_bias(local);

    return adjusted + local;
}
```

`volatile` forces a visible stack slot for `local`. The call to `add_bias` prevents the
whole function from collapsing into a single arithmetic expression.

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
function body copied from the real output of:

```sh
gcc -S -O2 -fno-omit-frame-pointer -masm=intel demo.c -o demo.s
```

```asm
_framed_total:                          ## @framed_total
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	sub	rsp, 16
	add	rdi, rsi
	mov	qword ptr [rbp - 8], rdi
	mov	rdi, qword ptr [rbp - 8]
	call	_add_bias
	add	rax, qword ptr [rbp - 8]
	add	rsp, 16
	pop	rbp
	ret
	.cfi_endproc
```

Map the arguments first: `a` arrives in `rdi`, `b` arrives in `rsi`, and the return
value leaves in `rax`.

Now split mechanics from business logic:

- `push rbp; mov rbp, rsp` establishes a visible frame pointer.
- `sub rsp, 16` reserves stack space. Only 8 bytes are visibly used here, but the ABI
  alignment rules make 16 a natural reservation size.
- `add rdi, rsi` computes `a + b`.
- `mov qword ptr [rbp - 8], rdi` stores `local` in the current frame.
- `mov rdi, qword ptr [rbp - 8]` reloads `local` as the first argument to `add_bias`.
- `call _add_bias` pushes a return address and transfers control.
- `add rax, qword ptr [rbp - 8]` adds `local` to the helper's return value.
- `add rsp, 16; pop rbp; ret` releases the frame and returns to the caller.

The `.cfi_*` directives are not CPU instructions. They are unwind metadata for
debuggers, profilers, exception machinery, and stack walkers. They describe where the
canonical frame address and saved registers are while the function runs.

The helper has a smaller frame because it has no local stack slot:

```asm
_add_bias:                              ## @add_bias
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rdi + 7]
	pop	rbp
	ret
	.cfi_endproc
```

The pattern is the same, but the middle is just `lea rax, [rdi + 7]`. No `sub rsp`
appears because there is no local stack storage to reserve.

## Frame pointer versus stack pointer

`rsp` is the moving top of the stack. It changes when the function reserves storage,
pushes temporary values, calls another function, or releases storage. `rbp` is an
optional stable anchor. With a frame pointer, a local such as `[rbp - 8]` keeps the same
address even if `rsp` changes later.

This is why debug builds tend to preserve `rbp`: stack walking and local inspection are
straightforward. Optimized builds often omit it to free another general-purpose register
and rely on unwind tables. The code is still valid; it is just less visually tidy.

## ARM64 appendix

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Relevant function body:

```asm
_framed_total:                          ; @framed_total
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #32
	.cfi_def_cfa_offset 32
	stp	x29, x30, [sp, #16]             ; 16-byte Folded Spill
	add	x29, sp, #16
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	add	x8, x1, x0
	str	x8, [sp, #8]
	ldr	x0, [sp, #8]
	bl	_add_bias
	ldr	x8, [sp, #8]
	add	x0, x8, x0
	ldp	x29, x30, [sp, #16]             ; 16-byte Folded Reload
	add	sp, sp, #32
	ret
	.cfi_endproc
```

AArch64 uses `sp` as the stack pointer, `x29` as the conventional frame pointer, and
`x30` as the link register. `bl _add_bias` writes the return address into `x30`, so a
function that calls another function usually saves `x29` and `x30` together with `stp`.
The epilogue reloads both with `ldp`, releases stack space, and returns with `ret`.

The same idea is visible, but the vocabulary differs: x86-64 uses `call` to push a
return address to the stack; ARM64 uses a link register and spills it when needed.

## Failure modes & trade-offs

- **Assuming every function has a visible frame.** Optimized leaf functions may have no
  prologue at all, and frame-pointer omission removes the tidy `rbp` chain.
- **Forgetting stack alignment.** The `sub rsp, N` amount is not only about named
  locals. It also keeps the stack aligned for calls and ABI expectations.
- **Confusing `.cfi_*` with executable instructions.** CFI directives matter for
  unwinding, but they do not run on the CPU as part of the function body.
- **Ignoring alternate exits.** Real functions may have multiple returns, error paths,
  or tailcalls. Each path must leave the stack and callee-saved registers in a valid
  state.
- **Using red-zone assumptions in the wrong place.** System V AMD64 user code can use a
  red zone, but kernels and interrupt-heavy code usually cannot rely on it.
- **Treating stack bytes as lifetime-safe.** Once the function returns, its frame is no
  longer yours. Pointers into it are dangling.

## In practice

- **Find the prologue first.** It tells you whether a frame pointer is present and how
  much stack space is reserved.
- **Name local slots by offset.** `[rbp - 8]` is a slot, not a variable name. The note
  you assign to it comes from nearby stores and loads.
- **Separate ABI bookkeeping from computation.** `push`, `mov rbp, rsp`, `sub rsp`, CFI,
  and epilogue code are frame mechanics.
- **Check calls before returns.** A real `call` means a return address is pushed and
  stack alignment matters.
- **Pair this with the raw stack model.** The frame is the structured version of the
  stack operations from [[lowlevel/assembly-and-compiler-output/stack-at-the-assembly-level|the stack at the assembly level]].

**Connects to:** [[lowlevel/assembly-and-compiler-output/stack-at-the-assembly-level|The stack at the assembly level]] · [[lowlevel/assembly-and-compiler-output/system-v-amd64-calling-convention|The System V AMD64 calling convention]] · [[lowlevel/assembly-and-compiler-output/control-flow-jumps-conditions-and-flags|Control flow: jumps, conditions, and the flags register]] · [[lowlevel/pointers-and-memory/the-stack-frames-locals-and-automatic-storage|The stack: frames & automatic storage]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]]

## Sources

- **System V AMD64 ABI** — stack alignment, call-frame rules, red zone, register saving, and unwind expectations. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 and Vol. 2 — semantics for `push`, `pop`, `call`, `ret`, stack addressing, and frame-related instructions. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 — procedure calls, stack frames, register saving, and machine-level C. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — approachable x86-64 stack-frame examples. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Felix Cloutier — x86 and amd64 instruction reference** — quick lookup for `push`, `pop`, `call`, `ret`, and `leave`; verify edge cases against Intel SDM. https://www.felixcloutier.com/x86/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — `sp`, `x29`, `x30`, `bl`, `ret`, and Apple platform calling-convention details for the appendix output. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
