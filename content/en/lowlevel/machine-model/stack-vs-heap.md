---
title: "Stack vs heap"
description: The two memory regions every C program lives in — the stack (automatic, LIFO, fast, tied to function calls) and the heap (manual, any lifetime, flexible, slower). What each is, how the address space is laid out, and a C program that prints both so you can see them.
tags: [machine-model, stack, heap, memory, address-space]
order: 6
updated: 2026-06-22
---
# Stack vs heap

A managed language gave you one undifferentiated "memory" and a garbage collector that
decided when things died. Underneath, every program splits its working memory into two
regions with opposite rules. The **stack** is automatic, last-in-first-out storage tied
to function calls: each call gets a frame, and it vanishes the instant the call returns
— you never free it. The **heap** is a large pool you manage by hand with `malloc` and
`free`: a block lives exactly as long as you keep it alive, across any number of calls.
Stack is fast and finite; heap is flexible and slower. Knowing which is which — and who
frees what — is the difference between correct C and crashes.

> The reset: the GC is gone. Stack memory frees itself when a function returns whether
> you like it or not (so a pointer into it dies there). Heap memory frees *never* unless
> you call `free` (so forgetting leaks it). Lifetime is now your job.

## One address space, several regions

A running process sees one flat virtual address space, carved into regions. From low
addresses to high:

| Region | Holds | Grows | Lifetime |
|---|---|---|---|
| Text (code) | machine instructions | fixed | whole program |
| Data (`.data`) | initialized globals/statics | fixed | whole program |
| BSS (`.bss`) | zero-initialized globals/statics | fixed | whole program |
| **Heap** | `malloc`'d blocks | **upward** ↑ | until you `free` |
| *(gap)* | unused virtual space | — | — |
| **Stack** | call frames, locals | **downward** ↓ | per function call |

Heap and stack grow *toward each other* from opposite ends of the gap — a layout chosen
so each can expand without a fixed boundary between them. The
[[lowlevel/machine-model/the-memory-hierarchy|hierarchy]] note was about *speed* of
memory; this is about *organization* of one process's address space. (Virtual memory —
why every process sees its own clean version of this map — gets its own note later.)

## The stack: automatic storage, LIFO

The stack is managed by one register, `rsp` (the stack pointer), and a simple
discipline. Calling a function **pushes** a frame (subtracts from `rsp`); returning
**pops** it (adds back). A frame holds the function's locals, saved registers, and the
return address. Because it's pure LIFO, allocation is nearly free — just move `rsp` —
and deallocation is automatic: when the function returns, its entire frame is gone.

- **Fast.** Allocating a local is one register adjustment, and the top of the stack is
  always hot in cache, so locals are about as cheap as memory gets.
- **Automatic lifetime.** You never free a local; it dies when its function returns.
  That's the trap too: a pointer to a local is **dangling** the moment you return.
- **Small and fixed.** The stack has a hard limit (commonly ~8 MB on Linux, ~1–8 MB on
  macOS threads). Blow it — deep/infinite recursion, or a huge local array — and you get
  a **stack overflow**: instant crash, no diagnostic.

The mechanics of frames (prologue/epilogue, `push`/`pop`, the call instruction) belong
to [[lowlevel/assembly-and-compiler-output/index|assembly & compiler output]]; here the
point is the *policy*: LIFO, automatic, bounded.

## The heap: manual storage, any lifetime

When you need memory whose lifetime isn't tied to one call — it must outlive the
function that created it, or its size isn't known until runtime — you ask the **heap**.
`malloc(n)` finds a free block of `n` bytes and returns a pointer; `free(p)` returns it
to the allocator. Between those two calls the block is yours, across any number of
function returns.

- **Flexible lifetime.** A block lives until *you* free it — perfect for data structures
  that outlive their creator (a list you build and return).
- **Manual and slower.** `malloc`/`free` run real bookkeeping (free lists, size classes,
  coalescing), so each call costs far more than a stack bump, and fresh heap memory is
  cold in cache.
- **You own every consequence.** Forget to `free` → a memory leak. `free` twice or use
  after `free` → corruption. The allocator and these failure modes are the whole subject
  of [[lowlevel/pointers-and-memory/index|pointers & memory]].

## See both regions

This program prints an address from each region. Compile at `-O0` so locals aren't
optimized into registers, then run:

```c
// regions.c — print one address from each part of the address space.
// gcc -O0 -Wall -Wextra regions.c -o regions && ./regions
#include <stdio.h>
#include <stdlib.h>

int global_initialized = 42;   // .data
int global_zero;               // .bss

void show_stack_growth(int depth) {
    int local;                 // a fresh stack slot each call
    printf("  depth %d: &local = %p\n", depth, (void*)&local);
    if (depth < 3) show_stack_growth(depth + 1);
}

int main(void) {
    int stack_var = 7;
    int *heap_var = malloc(sizeof(int));

    printf("code  (&main)        = %p\n", (void*)main);
    printf("data  (&global_init) = %p\n", (void*)&global_initialized);
    printf("bss   (&global_zero) = %p\n", (void*)&global_zero);
    printf("heap  (malloc)       = %p\n", (void*)heap_var);
    printf("stack (&stack_var)   = %p\n", (void*)&stack_var);
    printf("\nstack grows DOWN as we recurse:\n");
    show_stack_growth(0);

    free(heap_var);
    return 0;
}
```

A real run (macOS, x86-64) — your exact addresses differ each run because of ASLR, but
the *ordering* and the direction hold:

```
code  (&main)        = 0x10cf69e10
data  (&global_init) = 0x10cf6b000
bss   (&global_zero) = 0x10cf6b004
heap  (malloc)       = 0x7fb520804080
stack (&stack_var)   = 0x7ff7b2f95988

stack grows DOWN as we recurse:
  depth 0: &local = 0x7ff7b2f95968
  depth 1: &local = 0x7ff7b2f95948   <- each frame is at a LOWER address
  depth 2: &local = 0x7ff7b2f95928
  depth 3: &local = 0x7ff7b2f95908
```

Code and globals sit low and together; the heap is a big region above them; the stack is
way up high — and every recursion puts the new `&local` at a *lower* address, proving the
stack grows downward. You just saw the address-space map the table described.

## Which one: a decision rule

| Use the **stack** when… | Use the **heap** when… |
|---|---|
| size is known at compile time | size is decided at runtime |
| the data dies with the function | the data must outlive the call |
| it's small (bytes–KB) | it's large (KB–GB) |
| you want zero alloc cost | you accept cost for flexibility |

Default to the stack — it's faster and frees itself. Reach for the heap only when
lifetime or size forces you to, and then make ownership (who calls `free`) explicit.

## Failure modes & trade-offs

- **Dangling pointer (returning a local).** `int *f(void){ int x=0; return &x; }` returns
  a pointer to memory that's already freed when `f` returns — using it is undefined
  behavior. Stack memory is auto-freed; pointers into it can't escape.
- **Stack overflow.** Unbounded recursion or a multi-MB local array (`int buf[4'000'000]`)
  exceeds the stack limit and crashes hard. Large or runtime-sized buffers belong on the
  heap.
- **Memory leak.** Heap blocks you never `free` accumulate until the process dies; in a
  long-running server that's a slow death. Every `malloc` needs an owner responsible for
  `free`.
- **Use-after-free / double-free.** The flip side of manual heap management — corruption
  and exploitable bugs. Covered in depth in pointers & memory.

## In practice

- **Locals are stack, `malloc` is heap — that's the whole tell.** No keyword says
  "stack"; automatic variables just *are* stack. Only `malloc`/`calloc`/`realloc` (and
  friends) touch the heap.
- **A pointer doesn't tell you which region it points into.** `int *p` can point at a
  local, a global, or a heap block. *You* must track that — especially "may I `free`
  this?" (only heap blocks, exactly once).
- **Big or long-lived → heap; small or scoped → stack.** This single instinct prevents
  both stack overflows and most lifetime bugs.
- **This is the foundation for everything next.** C's whole memory model, pointers, and
  allocators build directly on these two regions.

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/the-memory-hierarchy|The memory hierarchy]] · [[lowlevel/machine-model/registers-and-the-isa|Registers & the ISA]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]]

## Sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 3 & 9 — the stack at machine level, the process address space, and virtual memory; the spine for this note. https://csapp.cs.cmu.edu/
- **Jens Gustedt — *Modern C*** — automatic vs allocated storage duration, lifetimes, and `malloc`/`free` done right. https://gustedt.gitlabpages.inria.fr/modern-c/
- **cppreference — `malloc` / `free` / storage duration** — the precise C semantics of automatic vs allocated storage. https://en.cppreference.com/w/c/memory/malloc
- **Arpaci-Dusseau — *Operating Systems: Three Easy Pieces*, "Address Spaces"** — why a process sees one clean virtual layout with stack and heap at opposite ends. https://pages.cs.wisc.edu/~remzi/OSTEP/
- **System V AMD64 ABI** — how the stack is organized at function-call boundaries (frames, alignment, the `rsp` discipline). https://gitlab.com/x86-psABIs/x86-64-ABI
