---
title: "The heap: malloc/free & the allocator underneath"
description: Heap allocation gives C objects a lifetime independent from one stack frame. malloc asks the allocator for raw bytes, free returns them, and correctness depends on explicit ownership.
tags: [heap, malloc, free, allocator, ownership]
order: 4
updated: 2026-06-30
---
# The heap: malloc/free & the allocator underneath

The heap is memory whose lifetime is not tied to a function call. `malloc` asks the C
library's allocator for a block of raw bytes; `free` gives that exact block back. Between
those two calls, you own the block and may treat it as storage for objects if you respect
size, alignment, and type rules. The allocator underneath is not magic: it keeps metadata,
reuses freed blocks, requests larger chunks from the operating system, and can be made
slow or corrupt by bad ownership discipline.

> The reset: `malloc` gives you storage, not a managed object. C will not remember the
> length, initialize ordinary `malloc` bytes, call destructors, or free the block for you.
> Ownership is the missing runtime feature you must write into the program.

## The heap contract

The core API is small:

| Function | Contract |
|---|---|
| `malloc(n)` | allocate `n` bytes; contents are indeterminate; returns `NULL` on failure |
| `calloc(count, size)` | allocate `count * size` bytes and zero all bytes; returns `NULL` on failure |
| `realloc(p, n)` | resize an allocated block; may move it; old pointer is invalid on success |
| `free(p)` | release a pointer returned by allocation; `free(NULL)` is allowed and does nothing |

The returned object pointer is suitably aligned for ordinary object types, so this is
valid:

```c
int *items = malloc(count * sizeof *items);
```

Two details in that line are deliberate. `sizeof *items` avoids repeating the type, so the
allocation stays correct if `items` changes type. And `count * sizeof *items` is a byte
count, because `malloc` works in bytes even when the pointer you receive is typed.

Always check the result before dereferencing. On a workstation, small allocations rarely
fail in toy programs. In systems code, long-running processes, constrained environments,
overcommit behavior, and hostile input make failure part of the contract. A `NULL` check
is not ceremony; it is the branch where ownership was never acquired.

## How it really works

Most calls to `malloc` do not trap into the kernel one by one. The allocator, usually in
libc, manages a heap arena inside the process address space. It obtains memory from the OS
in larger chunks, commonly with mechanisms such as `brk`/`sbrk` historically and `mmap`
for large or separate mappings on Unix-like systems. Then it sub-allocates pieces to your
program.

To do that, the allocator stores bookkeeping metadata: block sizes, free/used state, bins
or size classes, and links between free blocks. Real allocators optimize for common sizes,
thread behavior, locality, and fragmentation. A freed block may be reused by a later
`malloc` immediately, kept in a per-thread cache, coalesced with neighbors, or returned to
the OS later. You cannot infer "memory is gone" from `free`; you can only infer "this
program no longer owns that block."

The pointer returned by `malloc` points at the usable payload, not at the allocator's
metadata. Writing before the beginning or after the end of the block corrupts bytes the
allocator may rely on. That is why heap overflows often fail later: the bad write damages
allocator state now, and a later `malloc` or `free` discovers the wreckage.

`realloc` is the sharpest part of the basic API. It can grow in place, move the block to a
new address, shrink it, or fail. On success, the old pointer must no longer be used. On
failure, the old block is still owned by you and must still be freed. That is why the safe
pattern uses a temporary:

```c
int *grown = realloc(items, new_count * sizeof *items);
if (grown == NULL) {
    free(items);
    return false;
}
items = grown;
```

For over-aligned objects, C provides `aligned_alloc` in modern standards, with extra rules
on size and alignment. Ordinary `malloc` is enough for normal C object types; specialized
SIMD, page, DMA, or cache-line use cases need explicit alignment APIs and their own note.

## Executable artifact: own, grow, free

The demo lives in
`examples/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath/demo.c`.

```c
#include <stdio.h>
#include <stdlib.h>

struct Node {
    int value;
    struct Node *next;
};

static void print_ints(const char *label, const int *items, size_t count) {
    printf("%s", label);
    for (size_t i = 0; i < count; i++) {
        printf(" %d", items[i]);
    }
    printf("\n");
}

static void free_chain(struct Node *head) {
    while (head != NULL) {
        struct Node *next = head->next;
        free(head);
        head = next;
    }
}

int main(void) {
    size_t count = 4;
    int *numbers = malloc(count * sizeof *numbers);

    if (numbers == NULL) {
        perror("malloc numbers");
        return 1;
    }

    for (size_t i = 0; i < count; i++) {
        numbers[i] = (int)((i + 1) * 10);
    }

    printf("malloc numbers        = %p\n", (void *)numbers);
    print_ints("numbers initial       =", numbers, count);

    size_t bigger_count = 6;
    int *grown = realloc(numbers, bigger_count * sizeof *numbers);
    if (grown == NULL) {
        free(numbers);
        perror("realloc numbers");
        return 1;
    }

    numbers = grown;
    numbers[4] = 50;
    numbers[5] = 60;
    count = bigger_count;

    printf("realloc numbers       = %p\n", (void *)numbers);
    print_ints("numbers grown         =", numbers, count);

    size_t zero_count = 3;
    int *zeros = calloc(zero_count, sizeof *zeros);
    if (zeros == NULL) {
        free(numbers);
        perror("calloc zeros");
        return 1;
    }

    print_ints("calloc zeros          =", zeros, zero_count);

    struct Node *first = malloc(sizeof *first);
    struct Node *second = malloc(sizeof *second);
    if (first == NULL || second == NULL) {
        free(first);
        free(second);
        free(zeros);
        free(numbers);
        perror("malloc node");
        return 1;
    }

    first->value = 1;
    first->next = second;
    second->value = 2;
    second->next = NULL;

    printf("node chain            = %d -> %d\n",
           first->value, first->next->value);

    free_chain(first);
    free(zeros);
    free(numbers);

    first = NULL;
    zeros = NULL;
    numbers = NULL;

    printf("after free owners     = %s, %s, %s\n",
           first == NULL ? "NULL" : "live",
           zeros == NULL ? "NULL" : "live",
           numbers == NULL ? "NULL" : "live");

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
malloc numbers        = 0x7fc147804080
numbers initial       = 10 20 30 40
realloc numbers       = 0x7fc147804080
numbers grown         = 10 20 30 40 50 60
calloc zeros          = 0 0 0
node chain            = 1 -> 2
after free owners     = NULL, NULL, NULL
```

`realloc` happened to keep the same address in this run; code must not rely on that. The
safe pattern is still `grown = realloc(numbers, ...)`, check `grown`, then replace the
owner. `calloc` produced zero bytes, which show up as integer zeros here. The final line
sets owner variables to `NULL` after `free`; that does not invalidate aliases elsewhere,
but it prevents accidental reuse through these names.

## Failure modes & trade-offs

- **Forgetting to free.** A leak is not "some bytes lost"; it is ownership with no release
  path. In a long-running process, repeated leaks become a reliability bug.
- **Double-free.** Calling `free` twice on the same allocation corrupts allocator state.
  Setting one owner variable to `NULL` helps only if there are no other aliases.
- **Use-after-free.** After `free(p)`, every pointer value into that block is invalid for
  access. The allocator may reuse the bytes for a different object at any time.
- **Overflow in size math.** `count * sizeof *p` can wrap `size_t` before `malloc` sees it,
  allocating a smaller block than the loop later writes. `calloc` implementations are
  required to detect multiplication overflow; manual `malloc` size math needs checks when
  counts come from input.
- **Assuming `malloc` zeroes.** It does not. Use `calloc` for zeroed bytes, or initialize
  every field yourself.
- **Fragmentation and allocator cost.** Heap allocation is flexible, but each call runs
  bookkeeping, may take locks, may miss cache, and can fragment memory. Hot paths often
  batch, reuse, or switch to custom allocators later in this branch.

## In practice

- **Write down the owner.** Every heap pointer should have one clear owner responsible for
  exactly one `free`.
- **Pair pointer with length.** The allocator remembers the block size internally, but C
  does not expose it portably to your program. Keep `ptr + count` together.
- **Use the temporary `realloc` pattern.** Never assign `items = realloc(items, n)` unless
  you are willing to leak the old block on failure.
- **Initialize before publishing.** Allocate, fill every field, then hand the pointer to
  the rest of the program.
- **Free in the reverse shape of ownership.** For linked structures, trees, and arrays of
  owned pointers, write a cleanup function that mirrors construction.
- **Use sanitizers and leak checkers.** AddressSanitizer, LeakSanitizer, and Valgrind turn
  many heap lifetime bugs from "random crash later" into a local test failure.

**Connects to:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Pointer arithmetic & stride]] · [[lowlevel/pointers-and-memory/the-stack-frames-locals-and-automatic-storage|The stack: frames, locals & automatic storage]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/process-address-space-and-virtual-memory|Process address space & virtual memory]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for allocated storage duration, `malloc`, `calloc`, `realloc`, `free`, null pointers, and object lifetime rules. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Dynamic memory management** — compact reference for the C allocation APIs, failure behavior, `free(NULL)`, and `realloc` rules. https://en.cppreference.com/w/c/memory
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, dynamic memory allocation chapter — allocator internals: implicit/explicit free lists, coalescing, splitting, and fragmentation. https://csapp.cs.cmu.edu/
- **Doug Lea — *A Memory Allocator*** — classic allocator design notes from dlmalloc: bins, coalescing, locality, tunability, and trade-offs. https://gee.cs.oswego.edu/dl/html/malloc.html
- **Dan Luu — *Malloc tutorial*** — small allocator walkthrough that makes metadata and `sbrk`-style heap growth concrete. https://danluu.com/malloc-tutorial/
- **Clang AddressSanitizer / LeakSanitizer documentation** — practical tools for catching heap buffer overflows, use-after-free, double-free, and leaks in tests. https://clang.llvm.org/docs/AddressSanitizer.html
