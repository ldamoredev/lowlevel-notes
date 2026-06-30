---
title: "`void*` & type erasure"
description: `void*` is C's generic object pointer: it can carry an address without carrying the pointed-to type. That enables generic APIs like qsort and allocators, but the caller must preserve size, alignment, and meaning.
tags: [void-pointer, type-erasure, generic-programming, qsort, bytes]
order: 6
updated: 2026-06-30
---
# `void*` & type erasure

`void*` is C's generic **object** pointer. It can hold the address of any object, but it
does not remember the object's type, size, ownership, or lifetime. That is type erasure:
the program carries "where" while throwing away "what." This is how APIs such as
`malloc`, `memcpy`, `qsort`, allocators, and callback contexts stay generic in C. The
power is real, and so is the bill: every erased pointer must travel with enough external
information to recover its meaning safely.

> The reset: `void*` means "some object lives there." It does not mean "I can dereference
> this." Before reading or writing, you must choose the correct target type or treat the
> storage explicitly as bytes.

## The erased pointer contract

`void` is an incomplete type with no object size, so a `void*` cannot be dereferenced in
portable C and pointer arithmetic on it is not defined by the language. You can convert
object pointers to `void*` and back:

```c
int x = 42;
void *erased = &x;
int *restored = erased;
```

The restored pointer is useful only if the original type, alignment, and lifetime are
still true. `void*` does not carry those facts. Good generic APIs therefore pass the
missing facts beside the erased pointer:

| API shape | Missing fact supplied elsewhere |
|---|---|
| `malloc(size_t bytes)` | caller chooses the target type and count |
| `memcpy(void *dst, const void *src, size_t n)` | caller supplies byte count |
| `qsort(void *base, size_t count, size_t size, cmp)` | caller supplies element count, element size, comparator |
| `void *ctx` callbacks | caller and callee agree on the context type |

There is one special byte-level path: any object's representation may be inspected through
a character type, commonly `unsigned char*`. Use `void*` to erase object type; use
`unsigned char*` when you actually want bytes.

## How it really works

At runtime, `void*` is just a pointer value. The compiler stops knowing the pointed-to
object's type at that expression. That means operations requiring type information become
illegal or impossible:

```c
void *p = ...;
/* *p;      // invalid: what size and type should be read? */
/* p + 1;   // not portable C: void has no size */
```

The type information moves into the API contract. `qsort` receives a `void *base`, a
count, an element size, and a comparator. When the comparator is called, its two arguments
are `const void*` pointers to elements. The comparator must cast them back to the real
element type before reading:

```c
static int compare_ints(const void *left, const void *right) {
    const int *a = left;
    const int *b = right;
    return (*a > *b) - (*a < *b);
}
```

The allocator side is similar. `malloc` returns `void*` because it provides raw storage,
not a typed object. The assignment to `int *`, `struct Node *`, or another object pointer
is where the caller chooses the interpretation. That is why the size expression must match
the eventual pointer type: `malloc(count * sizeof *items)`.

Do not extend this rule to function pointers. C separates object pointers and function
pointers. A `void*` is a generic object pointer, not a portable container for a function
pointer. Many machines represent both as addresses, but the language does not promise
that object-pointer conversions apply to code pointers.

## Executable artifact: erase, restore, sort

The demo lives in `examples/pointers-and-memory/void-star-and-type-erasure/demo.c`.

```c
// demo.c - shows `void*` as address transport: erased views,
// a `qsort` callback, a byte dump, and a copy with `memcpy`.
// Compiles cleanly and runs with:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AnyView {
    const void *data;
    size_t size;
    void (*print)(const void *data);
};

static void print_int(const void *data) {
    const int *value = data;
    printf("int view              = %d\n", *value);
}

static void print_c_string(const void *data) {
    const char *const *text = data;
    printf("string view           = %s\n", *text);
}

static int compare_ints(const void *left, const void *right) {
    const int *a = left;
    const int *b = right;
    return (*a > *b) - (*a < *b);
}

static void dump_bytes(const void *data, size_t size) {
    const unsigned char *bytes = data;

    printf("raw bytes             =");
    for (size_t i = 0; i < size; i++) {
        printf(" %02x", bytes[i]);
    }
    printf("\n");
}

int main(void) {
    int number = 0x12345678;
    const char *word = "atlas";

    struct AnyView views[] = {
        {&number, sizeof number, print_int},
        {&word, sizeof word, print_c_string},
    };

    for (size_t i = 0; i < sizeof views / sizeof views[0]; i++) {
        views[i].print(views[i].data);
    }

    dump_bytes(&number, sizeof number);

    int values[] = {40, 10, 30, 20};
    qsort(values, sizeof values / sizeof values[0], sizeof values[0], compare_ints);

    printf("qsort result          =");
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        printf(" %d", values[i]);
    }
    printf("\n");

    void *erased = malloc(sizeof number);
    if (erased == NULL) {
        perror("malloc");
        return 1;
    }

    memcpy(erased, &number, sizeof number);
    int *restored = erased;
    printf("restored from void*   = 0x%x\n", *restored);

    free(erased);
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
int view              = 305419896
string view           = atlas
raw bytes             = 78 56 34 12
qsort result          = 10 20 30 40
restored from void*   = 0x12345678
```

The `AnyView` pairs an erased pointer with a print function that knows how to recover the
type. `dump_bytes` deliberately switches from `void*` to `unsigned char*`, so the object is
now viewed as bytes. `qsort` uses `void*` plus element size and a comparator to sort
without knowing the element type.

## Failure modes & trade-offs

- **Missing size.** A `void*` by itself does not know how much storage is valid. Generic
  APIs need a `size_t` count, byte count, or sentinel.
- **Wrong cast on restore.** Casting `void*` to the wrong pointer type and dereferencing
  may violate alignment, effective type, or lifetime rules.
- **Assuming byte arithmetic.** GNU C accepts `void*` arithmetic as an extension. Portable
  C does not. Cast to `unsigned char*` for byte walks.
- **Losing const.** A `const void *` preserves read-only access. Casting it to `void *` and
  writing through it breaks the promise and may be undefined behavior if the original
  object is const.
- **Erasing ownership.** `void*` does not say who frees the object. Callback contexts and
  containers must document whether they borrow or own.
- **Function pointer confusion.** Do not store function pointers in `void*` unless a
  platform-specific API explicitly guarantees it.

## In practice

- **Pass erased pointer plus metadata.** Type-erased APIs should carry size, count,
  alignment assumptions, and ownership rules in parameters or a wrapper struct.
- **Use `const void *` for read-only generic input.** It communicates that the callee may
  inspect but not mutate the pointed-to object.
- **Use `unsigned char *` for raw bytes.** That is the portable byte-inspection and byte
  copy tool.
- **Keep casts close to the boundary.** Cast once at the edge of a callback or generic
  function, then use typed pointers inside.
- **Prefer small typed wrappers around repeated `void*` patterns.** A `Buffer`, `Slice`,
  or `AnyView` struct makes contracts visible.

**Connects to:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Pointer arithmetic & stride]] · [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|The heap: malloc/free & the allocator underneath]] · [[lowlevel/pointers-and-memory/function-pointers|Function pointers]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — object pointer conversions, character-type access to object representations, `void`, and undefined behavior boundaries. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Pointer declaration** — `void*`, object pointer conversions, null pointers, and multi-level pointer syntax. https://en.cppreference.com/w/c/language/pointer
- **cppreference — `qsort`** — canonical standard-library example of type-erased sorting through `void*` and a comparator callback. https://en.cppreference.com/w/c/algorithm/qsort
- **cppreference — `memcpy`** — byte-oriented copying with `void*` parameters and explicit byte counts. https://en.cppreference.com/w/c/string/byte/memcpy
- **Jens Gustedt — *Modern C*** — modern treatment of generic C APIs, object representations, and pointer discipline. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Richard Reese — *Understanding and Using C Pointers*** — practical discussion of `void*`, callbacks, and generic containers in C. https://www.oreilly.com/library/view/understanding-and-using/9781449344535/
