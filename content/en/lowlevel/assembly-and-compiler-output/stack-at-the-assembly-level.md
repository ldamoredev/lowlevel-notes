---
title: "The stack at the assembly level (`push`/`pop`, `rsp`/`rbp`)"
description: The stack is a downward-growing region managed by rsp, with rbp often used as a frame pointer. Learn push/pop, stack slots, red-zone behavior, and how a real call frame appears in compiler output.
tags: [assembly, x86-64, stack, rsp, rbp]
order: 6
updated: 2026-06-30
---
# The stack at the assembly level (`push`/`pop`, `rsp`/`rbp`)

The stack is ordinary memory with a strong convention: it grows downward, `rsp` names
the current top, and function calls use it for return addresses, saved registers, and
temporary storage that fits inside a call frame. `rbp` is often a frame pointer: a
stable anchor that makes stack slots easy to name while `rsp` moves. When you can read
`[rbp - 32]`, `push rbp`, `pop rbp`, and `ret`, stack frames stop being a debugger
abstraction and become visible machine state.

> The reset: the stack is not magic storage. It is memory addressed by registers. A
> "local variable" in assembly is often just bytes at an offset from `rsp` or `rbp`.

## How it really works

On x86-64, the architectural stack pointer is `rsp`. By convention, the stack grows
toward lower addresses:

```asm
push    rbp     ; rsp -= 8; [rsp] = old rbp
pop     rbp     ; rbp = [rsp]; rsp += 8
```

Function calls also use the stack:

| Instruction | Stack effect |
|---|---|
| `call target` | pushes the return address, then jumps to `target` |
| `ret` | pops a return address into `rip` |
| `push reg` | subtracts 8 from `rsp`, stores `reg` |
| `pop reg` | loads from `[rsp]`, adds 8 to `rsp` |

A classic frame-pointer prologue is:

```asm
push    rbp
mov     rbp, rsp
sub     rsp, 32
```

After that, locals are commonly at negative offsets from `rbp`, while arguments and
saved caller state are at positive offsets or in registers depending on the ABI. At
high optimization levels, compilers may omit `rbp`, use only `rsp`, keep locals in
registers, or use the red zone. The source-level idea "this function has locals" does
not force one exact stack shape.

## A real stack-shaped function

The executable artifact for this note lives in
`examples/assembly-and-compiler-output/stack-at-the-assembly-level/`.

```c
static NOINLINE long combine_slots(long first, long second, long sum, long diff) {
    return first + second + sum + diff;
}

long stack_example(long a, long b) {
    volatile long first = a;
    volatile long second = b;
    volatile long sum = a + b;
    volatile long diff = a - b;

    return combine_slots(first, second, sum, diff);
}
```

The `volatile` locals are there for teaching: they force the compiler to materialize
stack slots instead of keeping everything in registers.

On this machine, `gcc` is Apple clang 15 targeting x86-64 Mach-O. This is the relevant
function body copied from the real output of:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_stack_example:                         ## @stack_example
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	qword ptr [rbp - 32], rdi
	mov	qword ptr [rbp - 24], rsi
	lea	rax, [rsi + rdi]
	mov	qword ptr [rbp - 16], rax
	sub	rdi, rsi
	mov	qword ptr [rbp - 8], rdi
	mov	rdi, qword ptr [rbp - 32]
	mov	rsi, qword ptr [rbp - 24]
	mov	rdx, qword ptr [rbp - 16]
	mov	rcx, qword ptr [rbp - 8]
	pop	rbp
	jmp	_combine_slots                  ## TAILCALL
	.cfi_endproc
```

Map the arguments first: `a` arrives in `rdi`, `b` arrives in `rsi`, and the return
value will leave in `rax`.

Now read the frame:

- `push rbp` saves the caller's frame pointer and moves `rsp` down by eight bytes.
- `mov rbp, rsp` makes `rbp` a stable base for this function.
- `[rbp - 32]`, `[rbp - 24]`, `[rbp - 16]`, and `[rbp - 8]` are local stack slots.
- `lea rax, [rsi + rdi]` computes `sum = a + b` without reading memory.
- `sub rdi, rsi` computes `diff = a - b`, overwriting `rdi`.
- The four `mov` loads reload locals into the first four integer argument registers:
  `rdi`, `rsi`, `rdx`, and `rcx`.
- `pop rbp` restores the caller's frame pointer.
- `jmp _combine_slots` is a tailcall: control transfers to the helper without pushing
  another return address.

There is one subtlety: this function stores below `rbp` without a `sub rsp, 32`.
Because `rsp == rbp` after the prologue, those slots live just below the current stack
pointer. On System V AMD64 targets, leaf-like code may use the 128-byte **red zone**:
space below `rsp` that signal/interrupt handlers are not supposed to clobber in normal
user-mode code. The compiler uses it here because the locals are dead before the
tailcall.

The helper receives the values in registers and does not need stack locals:

```asm
_combine_slots:                         ## @combine_slots
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rdi + rsi]
	add	rax, rdx
	add	rax, rcx
	pop	rbp
	ret
	.cfi_endproc
```

`ret` pops the return address left by the original caller. Because `_stack_example`
tailcalled `_combine_slots`, the helper returns directly to whoever called
`stack_example`.

## `push`, `pop`, `rsp`, and `rbp`

The useful mental split is:

| Register | Role |
|---|---|
| `rsp` | current stack pointer; changes when stack storage is pushed, popped, reserved, or released |
| `rbp` | optional frame pointer; stable base for addressing stack slots |
| `rip` | instruction pointer; receives the return address on `ret` |

`rsp` must be restored before returning. If a function subtracts from `rsp`, it must
add the same amount back along every normal return path. If it pushes callee-saved
registers, it must pop or restore them. If stack alignment is required before a call,
the compiler adjusts `rsp` to satisfy the ABI.

`rbp` is optional in optimized code. Debug builds often keep it because it makes stack
walking and local-variable display easier. Optimized builds may free it as another
general-purpose register and describe unwinding through metadata instead of a visible
frame-pointer chain.

## ARM64 appendix

The same `demo.c` was cross-compiled on this machine with:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Relevant function body:

```asm
_stack_example:                         ; @stack_example
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #32
	.cfi_def_cfa_offset 32
	str	x0, [sp, #24]
	str	x1, [sp, #16]
	add	x8, x1, x0
	str	x8, [sp, #8]
	sub	x8, x0, x1
	str	x8, [sp]
	ldr	x0, [sp, #24]
	ldr	x1, [sp, #16]
	ldr	x2, [sp, #8]
	ldr	x3, [sp], #32
	b	_combine_slots
	.cfi_endproc
```

AArch64 uses `sp` as the stack pointer and `x29` as the conventional frame pointer
when a frame pointer is needed. This function does not set up `x29`; it simply
allocates 32 bytes with `sub sp, sp, #32`, stores four 8-byte locals, reloads them into
argument registers `x0` through `x3`, and deallocates with the post-indexed load:

```asm
ldr	x3, [sp], #32
```

That loads `diff` from `[sp]` into `x3`, then adds 32 to `sp`. The final
`b _combine_slots` is the ARM64 tailcall.

The helper is register-only:

```asm
_combine_slots:                         ; @combine_slots
	.cfi_startproc
; %bb.0:
	add	x8, x1, x0
	add	x8, x8, x2
	add	x0, x8, x3
	ret
	.cfi_endproc
```

Same C, different stack vocabulary: x86-64 shows `push`/`pop` around `rbp`; ARM64
adjusts `sp` explicitly with `sub` and a post-indexed `ldr`.

## Failure modes & trade-offs

- **The stack grows downward.** More stack storage means smaller addresses, not larger
  ones.
- **`rbp` may disappear.** Optimized code often omits the frame pointer. Do not assume
  every local is addressed as `[rbp - offset]`.
- **The red zone is ABI-specific.** System V AMD64 user code has a 128-byte red zone;
  Windows x64 does not. Kernels and low-level interrupt code often disable or avoid
  red-zone assumptions.
- **Stack alignment matters.** ABIs require particular alignment before calls. Hand
  assembly that misaligns `rsp` can crash in library code that expects aligned stack
  arguments or vector spills.
- **Tailcalls change stack shape.** A tailcall uses `jmp` instead of `call`, so the
  callee returns to the original caller. That can make backtraces shorter.
- **Stack lifetime is automatic, not safe.** Returning a pointer to a local stack slot
  is still undefined behavior in C. The bytes may remain for a moment, but the lifetime
  is over.

## In practice

- **Identify the prologue first.** `push rbp; mov rbp, rsp` usually means a visible
  frame pointer is being established.
- **Translate offsets into slots.** `[rbp - 8]`, `[rbp - 16]`, and `[rbp - 24]` are
  nearby 8-byte locations in the current frame or red zone.
- **Separate storage from value.** A stack slot is an address. The value is loaded or
  stored by an instruction such as `mov`, `str`, or `ldr`.
- **Check whether there is a real call.** `call` pushes a new return address; tailcall
  `jmp` does not.
- **Pair stack assembly with C lifetimes.** Automatic storage duration from C becomes
  stack-shaped machine storage when the compiler decides it needs memory. Connect this
  to [[lowlevel/pointers-and-memory/the-stack-frames-locals-and-automatic-storage|stack frames and automatic storage]].

**Connects to:** [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|x86-64 registers and the register file]] · [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|The core instruction set: mov, arithmetic, lea]] · [[lowlevel/assembly-and-compiler-output/addressing-modes-and-memory-operands|Addressing modes and memory operands]] · [[lowlevel/assembly-and-compiler-output/control-flow-jumps-conditions-and-flags|Control flow: jumps, conditions, and the flags register]] · [[lowlevel/pointers-and-memory/the-stack-frames-locals-and-automatic-storage|The stack: frames & automatic storage]]

## Sources

- **System V AMD64 ABI** — register calling convention, stack alignment, call-frame rules, and red-zone definition. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 and Vol. 2 — authoritative semantics for `push`, `pop`, `call`, `ret`, `mov`, stack addressing, and `rsp`/`rbp`. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 — stack frames, procedures, register saving, and machine-level function calls. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — approachable stack-frame and procedure examples in x86-64 assembly. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Felix Cloutier — x86 and amd64 instruction reference** — quick lookup for `push`, `pop`, `call`, `ret`, and related instructions; verify edge cases against Intel SDM. https://www.felixcloutier.com/x86/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — ARM64 `sp`, `x29`, load/store addressing, and platform ABI details for the appendix output. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
