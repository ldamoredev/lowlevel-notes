---
title: "The stack: frames, locals & automatic storage"
description: A stack frame is one live function activation. Automatic locals live inside that activation, recursive calls get fresh objects, and pointers into a frame are only borrowed while the frame is alive.
tags: [stack, stack-frame, automatic-storage, lifetime, pointers]
order: 3
updated: 2026-06-30
---
# The stack: frames, locals & automatic storage

The stack is not just "where locals go." It is the runtime record of active function
calls. Each call gets a stack frame: a slice of storage for return machinery, spills, and
automatic objects that exist only while that activation is alive. Recursion creates fresh
objects because it creates fresh frames. A pointer into a caller's frame can be a valid
borrow during the call chain; the same pointer becomes poison the instant the frame
returns.

> The reset: automatic storage is a lifetime rule, not an allocation API. You do not
> `free` a local; the function return frees the whole frame. That also means you cannot
> return, store, or asynchronously use a pointer into that frame.

## Frames are function activations

A frame is one live invocation of a function. The exact byte layout is ABI- and
compiler-dependent, but the policy is stable:

| Thing | What it means |
|---|---|
| stack pointer (`rsp` on x86-64) | register that marks the current top of the stack |
| return address | where execution resumes when the call returns |
| saved registers / spills | values the compiler preserves across calls or moves out of registers |
| automatic locals | block-scope objects without `static` storage duration |
| caller frame | the frame that remains alive while the callee runs |

On the System V AMD64 ABI target used throughout the atlas, the stack grows downward:
reserving frame storage usually subtracts from `rsp`, and returning gives that storage
back by restoring `rsp`. Many optimized functions omit a traditional frame pointer
(`rbp`) and keep locals in registers when their address is never needed. The model is
still "one activation, one lifetime"; the physical layout is an implementation detail.

## How it really works

In C, most block-scope local objects have **automatic storage duration**. Their lifetime
begins when execution enters the block and ends when execution leaves it. That includes
ordinary locals:

```c
void f(void) {
    int x = 42;   // automatic storage duration
}
```

It also includes recursive calls. Every invocation of `f` has its own `x`; recursion is
not reusing the same local object. If a callee receives `&x`, that pointer is valid only
while the invocation of `f` that owns `x` is still active.

Taking a local's address often forces the compiler to place it in memory, but a local is
not guaranteed to have a stable visible address in every build. At higher optimization
levels, the compiler may keep it entirely in a register, split it, reuse its slot for
another object with a non-overlapping lifetime, or remove it. `-O0` demos show the frame
shape because they deliberately make observation easy, not because production frames are
that literal.

Automatic does not mean "always stack" in the C standard. The standard specifies lifetime,
not hardware placement. On real hosted x86-64 systems, automatic objects whose addresses
matter usually live in the stack frame. `static` locals are different: they have static
storage duration, are initialized once, persist for the whole program, and are not freed
when the function returns.

Variable Length Arrays (VLAs), where supported, also have automatic storage duration. They
make the frame size depend on runtime input. That can be useful in small controlled cases,
but it is also how a harmless-looking local becomes a stack overflow.

## Executable artifact: watch frames appear

The demo lives in
`examples/pointers-and-memory/the-stack-frames-locals-and-automatic-storage/demo.c`.

```c
#include <stdio.h>

static void bump_static_local(void) {
    static int visits = 0;
    visits += 1;
    printf("static local: value=%d address=%p\n", visits, (void *)&visits);
}

static void show_frame(int depth, int *borrowed_from_caller) {
    int local = depth * 10 + 1;
    int scratch[2] = {local, local + 1};

    printf("depth %d: &local=%p  borrowed=%p  borrowed_value=%d\n",
           depth, (void *)&local, (void *)borrowed_from_caller,
           *borrowed_from_caller);

    if (depth < 3) {
        show_frame(depth + 1, &local);
    }

    printf("depth %d returning with local=%d scratch_sum=%d\n",
           depth, local, scratch[0] + scratch[1]);
}

int main(void) {
    int main_local = 100;

    printf("main frame: &main_local=%p\n", (void *)&main_local);
    show_frame(0, &main_local);

    bump_static_local();
    bump_static_local();

    printf("main local after calls=%d\n", main_local);
    return 0;
}
```

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

One real output:

```
main frame: &main_local=0x7ff7b1dff298
depth 0: &local=0x7ff7b1dff25c  borrowed=0x7ff7b1dff298  borrowed_value=100
depth 1: &local=0x7ff7b1dff21c  borrowed=0x7ff7b1dff25c  borrowed_value=1
depth 2: &local=0x7ff7b1dff1dc  borrowed=0x7ff7b1dff21c  borrowed_value=11
depth 3: &local=0x7ff7b1dff19c  borrowed=0x7ff7b1dff1dc  borrowed_value=21
depth 3 returning with local=31 scratch_sum=63
depth 2 returning with local=21 scratch_sum=43
depth 1 returning with local=11 scratch_sum=23
depth 0 returning with local=1 scratch_sum=3
static local: value=1 address=0x10e101000
static local: value=2 address=0x10e101000
main local after calls=100
```

The recursive frames land at lower addresses in this run, matching the downward-growing
x86-64 stack. Each depth has a different `local`, and passing `&local` to the next call is
safe because the caller has not returned yet. The `static` local keeps the same address and
value across calls because it is not automatic storage.

## Failure modes & trade-offs

- **Returning a local's address.** `return &local;` hands the caller a pointer into a dead
  frame. Using that pointer is undefined behavior, even if the old bytes have not been
  overwritten yet.
- **Saving a borrowed stack pointer.** Storing `&local` in a global, a heap object, a
  thread argument, or an async callback outlives the frame unless the API proves otherwise.
- **Uninitialized automatic objects.** Plain automatic locals are not zeroed by default.
  Reading an uninitialized `int local;` is undefined behavior.
- **Huge locals and VLAs.** Stack space is fast but limited. Large arrays, input-sized
  VLAs, and deep recursion can blow the stack long before the heap is exhausted.
- **Assuming frame layout.** The direction and spacing you print at `-O0` are teaching
  artifacts. Optimization, ABI choices, sanitizers, and frame-pointer omission change the
  layout.
- **Lifetime vs performance.** Stack allocation is cheap because deallocation is one frame
  pop, but that speed is purchased by an inflexible lifetime: strictly scoped, strictly
  last-in-first-out.

## In practice

- **Treat pointers to locals as borrows.** They may be passed down the call stack; they
  must not be kept after the owner frame returns.
- **Return data by value, caller-provided buffer, or heap allocation.** Pick based on size
  and ownership. Do not return pointers to local arrays.
- **Keep stack objects small and bounded.** Runtime-sized or large buffers belong in a
  deliberate allocation strategy, not casually in a frame.
- **Initialize automatic locals before reading them.** Managed-language muscle memory
  lies here; C does not zero ordinary stack locals.
- **Use tools when lifetimes get subtle.** Compiler warnings catch many `return local
  address` cases, and AddressSanitizer can catch stack-use-after-scope/use-after-return
  patterns in tests.
- **Use frame pointers deliberately for debugging/profiling.** `-fno-omit-frame-pointer`
  can make call stacks easier to sample, but it is a build choice, not a C semantic.

**Connects to:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Pointer arithmetic & stride]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/process-address-space-and-virtual-memory|Process address space & virtual memory]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — storage duration, object lifetime, block scope, and undefined behavior when an object's lifetime ends. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Storage-class specifiers and storage duration** — concise reference for automatic, static, thread, and allocated storage duration in C. https://en.cppreference.com/w/c/language/storage_duration
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 and 9 — stack frames, calls, machine code, and process address-space organization. https://csapp.cs.cmu.edu/
- **System V AMD64 ABI** — x86-64 stack alignment, calling sequence, register save rules, and frame conventions for Unix-like targets. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Jens Gustedt — *Modern C*** — practical treatment of automatic objects, initialization, lifetimes, and pointer discipline in modern C. https://gustedt.gitlabpages.inria.fr/modern-c/
- **AddressSanitizer documentation** — runtime checks for stack-use-after-scope and related lifetime bugs during tests. https://clang.llvm.org/docs/AddressSanitizer.html
