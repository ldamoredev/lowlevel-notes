---
title: "Classic bug: memory leaks & ownership discipline"
description: A leak is allocated storage that no live owner can free. Ownership discipline turns `malloc` and `free` from scattered calls into a lifecycle with one clear cleanup path.
tags: [memory-leaks, ownership, malloc, free, realloc, cleanup]
order: 14
updated: 2026-06-30
---
# Classic bug: memory leaks & ownership discipline

A memory leak is not "memory was allocated." It is "memory was allocated and the program
lost the ability or discipline to free it." C gives you allocation primitives, not an
ownership system. So you build one in the API: who owns this allocation, who may borrow it,
who may transfer it, and which cleanup path runs exactly once.

> The reset: `malloc` is easy. The hard part is preserving a path from every successful
> allocation to exactly one matching cleanup.

## How it really works

`malloc`, `calloc`, and `realloc` return storage with allocated lifetime. That storage
lasts until it is freed. If the program overwrites the last owning pointer, exits a scope
without cleanup, or takes an early return that skips cleanup, the allocation remains live
from the allocator's point of view but unreachable from the program's ownership model.
Long-running programs feel that as rising memory use. Short tools may hide it until the
same habits land in a daemon, game loop, kernel component, or embedded service.

Ownership is a design convention layered over C:

| Role | Meaning |
|---|---|
| owner | responsible for eventually releasing the resource |
| borrower | may use the resource only while the owner keeps it alive |
| transfer | moves the release responsibility to another owner |
| cleanup | the single code path that releases all currently owned resources |

`realloc` deserves special care. On failure, it returns `NULL` and leaves the original
allocation untouched. This is wrong:

```c
p = realloc(p, new_size);      /* leaks old allocation if realloc fails */
```

This is the safer shape:

```c
void *next = realloc(p, new_size);
if (next == NULL) {
    /* p is still valid and must still be freed */
} else {
    p = next;
}
```

The same rule applies to every resource, not only heap memory: file descriptors, mapped
memory, locks, temporary files, GPU buffers, and OS handles. C does not enforce a
destructor. A disciplined cleanup block is how you make lifetimes visible.

## Executable artifact: resize without losing ownership

The demo lives in `examples/pointers-and-memory/memory-leaks-and-ownership-discipline/demo.c`.

```c
// demo.c - shows ownership discipline: create/destroy, `realloc` through a
// temporary, and cleanup that returns the owner to an empty state.
// Compiles cleanly and runs with:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>
#include <stdlib.h>

struct OwnedInts {
    int *data;
    size_t count;
};

static int owned_ints_create(struct OwnedInts *owner, size_t count) {
    owner->data = calloc(count, sizeof *owner->data);
    if (owner->data == NULL) {
        owner->count = 0;
        return -1;
    }

    owner->count = count;
    for (size_t i = 0; i < count; i++) {
        owner->data[i] = (int)((i + 1) * 5);
    }

    return 0;
}

static void owned_ints_destroy(struct OwnedInts *owner) {
    free(owner->data);
    owner->data = NULL;
    owner->count = 0;
}

static int owned_ints_resize(struct OwnedInts *owner, size_t new_count) {
    int *new_data = realloc(owner->data, new_count * sizeof *owner->data);
    if (new_data == NULL) {
        return -1;
    }

    for (size_t i = owner->count; i < new_count; i++) {
        new_data[i] = (int)((i + 1) * 5);
    }

    owner->data = new_data;
    owner->count = new_count;
    return 0;
}

static int owned_ints_sum(const struct OwnedInts *owner) {
    int sum = 0;
    for (size_t i = 0; i < owner->count; i++) {
        sum += owner->data[i];
    }
    return sum;
}

int main(void) {
    struct OwnedInts owner = {0};

    if (owned_ints_create(&owner, 3) != 0) {
        perror("create");
        return 1;
    }

    printf("sum initial           = %d\n", owned_ints_sum(&owner));

    if (owned_ints_resize(&owner, 5) != 0) {
        owned_ints_destroy(&owner);
        perror("resize");
        return 1;
    }

    printf("sum after resize      = %d\n", owned_ints_sum(&owner));
    printf("owner before cleanup  = count %zu\n", owner.count);

    owned_ints_destroy(&owner);
    printf("owner after cleanup   = %s count=%zu\n",
           owner.data == NULL ? "NULL" : "live", owner.count);

    return 0;
}
```

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Real output:

```
sum initial           = 30
sum after resize      = 75
owner before cleanup  = count 5
owner after cleanup   = NULL count=0
```

The wrapper struct owns one allocation. Creation initializes it, resize uses a temporary
for `realloc`, and destruction returns the owner to an empty state. This is not a full
generic container. It is the smallest unit of ownership discipline.

## Failure modes & trade-offs

- **Overwriting the only owner.** Assigning a new allocation to the same pointer before
  freeing the old one loses the old allocation.
- **Early returns.** Error branches that return before cleanup are the classic leak path.
- **Partial construction.** If step 4 of 6 fails, cleanup must release steps 1 through 3
  and ignore steps 4 through 6.
- **Ambiguous ownership transfer.** If a function sometimes steals a pointer and sometimes
  only borrows it, callers will leak or double-free.
- **Leaks hidden by process exit.** The OS reclaims memory when a process exits, but that
  does not help long-lived programs or teach correct ownership.
- **Over-cleanup.** Fixing leaks by freeing too aggressively can create use-after-free.
  Ownership and lifetime have to move together.

## In practice

- **Initialize owners to empty states.** `{0}` plus a destructor that tolerates empty state
  makes cleanup paths simple.
- **Use one exit cleanup block for multi-step functions.** It is explicit, boring, and
  reliable in C.
- **Assign `realloc` to a temporary.** Never lose the original owner on allocation failure.
- **Document transfers in function names or comments.** `take_buffer`, `borrow_buffer`,
  and `clone_buffer` should mean different things.
- **Use leak tools regularly.** LeakSanitizer and Valgrind Memcheck find missed cleanup
  paths before production traffic does.
- **Prefer local lifetimes when possible.** Stack storage, arenas, and fixed buffers reduce
  ownership edges when their limits fit the job.

**Connects to:** [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|The heap: `malloc`/`free` and the allocator underneath]] · [[lowlevel/pointers-and-memory/use-after-free-and-double-free|Use-after-free & double-free]] · [[lowlevel/pointers-and-memory/custom-allocators-arena-pool-and-bump|Custom allocators: arena, pool & bump]] · [[lowlevel/pointers-and-memory/pointers-to-pointers-double-indirection|Pointers to pointers]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **ISO WG14 N1570 — C11 draft** — allocated storage duration and library contracts for allocation functions. https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- **cppreference — `malloc`, `calloc`, `realloc`, `free`** — concise behavior reference for C heap APIs and failure cases. https://en.cppreference.com/w/c/memory
- **Clang LeakSanitizer documentation** — leak detection model and how it integrates with AddressSanitizer. https://clang.llvm.org/docs/LeakSanitizer.html
- **Valgrind Memcheck manual** — leak kinds, heap tracking, and practical diagnostics. https://valgrind.org/docs/manual/mc-manual.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — allocator and memory-management context from a systems perspective. https://csapp.cs.cmu.edu/
- **Jens Gustedt — *Modern C*** — pragmatic C patterns for initialization, cleanup, and resource ownership. https://gustedt.gitlabpages.inria.fr/modern-c/
