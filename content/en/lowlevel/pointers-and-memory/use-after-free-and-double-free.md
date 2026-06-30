---
title: "Classic bug: use-after-free & double-free"
description: `free` ends an allocation's lifetime, not every pointer value that still names its old address. Use-after-free and double-free are ownership failures that turn allocator reuse into undefined behavior.
tags: [use-after-free, double-free, ownership, heap, undefined-behavior]
order: 12
updated: 2026-06-30
---
# Classic bug: use-after-free & double-free

`free(p)` does not delete the number stored in `p`. It ends the lifetime of the heap
allocation that `p` used to designate. Any pointer copies still containing that address
become stale, and using them is undefined behavior. A double-free is the same ownership
failure from the allocator's side: you asked it to reclaim storage that is no longer a
live allocation.

> The reset: pointer equality is not object liveness. An address can look familiar and
> still no longer be yours.

## How it really works

The C heap interface is small:

```c
void *malloc(size_t size);
void free(void *ptr);
```

`malloc` returns a pointer to allocated storage, or `NULL`. `free` accepts either `NULL`
or a pointer value previously returned by an allocation function and not already freed.
After a successful `free`, the allocation's lifetime is over. Reading through the old
pointer, writing through it, passing it back to `free`, or using it as if it still owned
storage is undefined behavior.

The allocator normally keeps metadata around chunks: sizes, state bits, free-list links,
or bins. A double-free can corrupt those structures. A use-after-free can be worse because
the allocator may quickly recycle the same address for a different object. Your stale
pointer then writes into a new owner. In security terms, that can become memory
corruption, type confusion, control-flow hijacking, or an information leak. In ordinary
systems work, it often becomes intermittent state corruption that appears far away from
the line that freed the memory.

Setting a pointer to `NULL` after `free` is useful only for the owner variable you clear.
It makes repeated destruction through that owner safe because `free(NULL)` is defined to
do nothing. It does not fix other aliases:

```c
int *a = malloc(sizeof *a);
int *b = a;
free(a);
a = NULL;
*b = 7;      /* stale alias: still undefined behavior */
```

So the deeper fix is ownership discipline. At any moment, decide which variable or
structure owns the allocation, which pointers merely borrow it, and when all borrowed
pointers stop being valid.

AddressSanitizer makes this visible by surrounding allocations with red zones and often
quarantining freed chunks before reuse. That is a debugging strategy, not the C execution
model. In a normal build, the allocator is free to reuse the block immediately.

## Executable artifact: a destructor that clears ownership

The demo lives in `examples/pointers-and-memory/use-after-free-and-double-free/demo.c`.

```c
// demo.c - shows heap ownership with a destructor that calls `free`, clears
// the owning pointer, and makes a second destruction safe.
// Compiles cleanly and runs with:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>
#include <stdlib.h>

struct Buffer {
    int *data;
    size_t count;
};

static int buffer_init(struct Buffer *buffer, size_t count) {
    buffer->data = malloc(count * sizeof *buffer->data);
    if (buffer->data == NULL) {
        buffer->count = 0;
        return -1;
    }

    buffer->count = count;
    for (size_t i = 0; i < count; i++) {
        buffer->data[i] = (int)(i + 1);
    }

    return 0;
}

static void buffer_destroy(struct Buffer *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->count = 0;
}

static int buffer_sum(const struct Buffer *buffer) {
    int sum = 0;
    for (size_t i = 0; i < buffer->count; i++) {
        sum += buffer->data[i];
    }
    return sum;
}

int main(void) {
    struct Buffer buffer = {0};

    if (buffer_init(&buffer, 4) != 0) {
        perror("buffer_init");
        return 1;
    }

    printf("sum while alive       = %d\n", buffer_sum(&buffer));
    printf("owner before destroy  = %s\n",
           buffer.data != NULL ? "live" : "NULL");

    buffer_destroy(&buffer);
    printf("owner after destroy   = %s count=%zu\n",
           buffer.data == NULL ? "NULL" : "live", buffer.count);

    buffer_destroy(&buffer);
    printf("second destroy safe   = %s count=%zu\n",
           buffer.data == NULL ? "NULL" : "live", buffer.count);

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
sum while alive       = 10
owner before destroy  = live
owner after destroy   = NULL count=0
second destroy safe   = NULL count=0
```

The demo does not demonstrate the bug by triggering undefined behavior. Instead, it shows
the local pattern that prevents one class of double-free: put the owning pointer and its
size in a struct, free through one destructor, then reset the owner to an empty state. Any
borrowed pointer would still have to be invalidated by API design.

## Failure modes & trade-offs

- **Aliases outliving the owner.** Clearing the owner pointer does not clear copies stored
  elsewhere.
- **Freeing through the wrong pointer.** `free` needs the pointer returned by the
  allocator, not a pointer into the middle of the object.
- **Two owners for one allocation.** If two structs both think they must call `free`, one
  of them will eventually be wrong.
- **Reusing freed memory "because it still prints right."** Freed bytes can remain
  unchanged until the allocator reuses them. That makes the bug timing-dependent.
- **Error paths.** Early returns often skip cleanup or run cleanup twice unless ownership
  has a single exit discipline.
- **Debug-only confidence.** ASan, allocator checks, and hardened mallocs catch many bugs,
  but they are not a proof that all lifetime edges are correct.

## In practice

- **Name ownership in the type.** A `struct Buffer` with `buffer_destroy` is clearer than
  a naked `int *` passed through many functions.
- **Make borrowed pointers short-lived.** Do not store borrows across calls that may free,
  resize, or reset the owner.
- **Destroy to an empty state.** Set owning pointers to `NULL` and sizes to zero after
  cleanup.
- **Use one cleanup path.** In C, a `goto cleanup` block is often clearer and safer than
  duplicated frees across several returns.
- **Run sanitizers during development.** Compile suspicious code with
  `-fsanitize=address,undefined -fno-omit-frame-pointer` when your toolchain supports it.
- **Document transfer.** APIs should say whether the caller keeps ownership, gives it
  away, or only lends a pointer for the duration of the call.

**Connects to:** [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|The heap: `malloc`/`free` and the allocator underneath]] · [[lowlevel/pointers-and-memory/memory-leaks-and-ownership-discipline|Memory leaks & ownership discipline]] · [[lowlevel/pointers-and-memory/buffer-overflow-and-out-of-bounds-access|Buffer overflow & out-of-bounds access]] · [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **ISO WG14 N1570 — C11 draft** — standard rules for allocated storage duration, pointer validity, and undefined behavior around `free`. https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- **cppreference — `free`** — concise contract for `free`, including `NULL` and already-freed pointers. https://en.cppreference.com/w/c/memory/free
- **Clang AddressSanitizer documentation** — practical detection model for use-after-free, double-free, and heap red zones. https://clang.llvm.org/docs/AddressSanitizer.html
- **MITRE CWE-416: Use After Free** — security taxonomy and consequences for stale heap pointers. https://cwe.mitre.org/data/definitions/416.html
- **MITRE CWE-415: Double Free** — security taxonomy for freeing the same memory twice. https://cwe.mitre.org/data/definitions/415.html
- **Valgrind Memcheck manual** — dynamic analysis behavior for invalid frees and invalid heap accesses. https://valgrind.org/docs/manual/mc-manual.html
