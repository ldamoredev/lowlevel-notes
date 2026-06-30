---
title: "What a pointer really is"
description: A C pointer is a typed value that locates an object. The address is the visible part; the type, lifetime, alignment, and array boundary rules are what make it safe or undefined.
tags: [pointers, memory, addresses, c, types]
order: 1
updated: 2026-06-30
---
# What a pointer really is

A pointer is a value whose job is to locate another object. On the x86-64 target this
atlas uses, that value usually prints like a 64-bit virtual address; in C, its portable
meaning is tighter: it points to an object of a particular type, to one-past the last
element of an array object, to nothing (`NULL`), or it is not a valid pointer value at
all. The type is not decoration — it tells the compiler how many bytes a dereference
touches, what alignment is required, and how pointer arithmetic moves. A pointer is
therefore not "just an integer"; it is an address-shaped value under rules the compiler
is allowed to exploit.

> The reset: `int *p = &x` means "`p` stores where an `int` lives." `p` is the pointer,
> `*p` is the object reached through it, and `p + 1` means "one `int` later", not "one
> byte later."

## The address is not the whole story

The address is the part you can print. The type and lifetime are the parts that decide
whether using it is valid.

| Piece | What it means |
|---|---|
| `&x` | address of object `x`; if `x` has type `T`, the result has type `T *` |
| `p` | the pointer value itself; copying it copies the location, not the object |
| `*p` | dereference: the lvalue for the object reached through `p` |
| `p + n` | pointer arithmetic in units of `sizeof *p`, valid only inside one array object or one-past it |
| `NULL` | a null pointer value; it compares unequal to every valid object or function pointer |

The pointer type carries no ownership, no length, and no runtime tag. `int *p` does not
tell you whether the pointed-to `int` is on the stack, in global storage, inside a
`malloc` block, or already dead. It only says that if `p` is valid, aligned, and points
at a live `int`, then `*p` may be used as an `int` lvalue.

That is why the same printed address can behave differently through different pointer
types. `int *` arithmetic steps by `sizeof(int)`. `char *` arithmetic steps by one byte,
and `unsigned char *` is the standard way to inspect an object's raw representation.
`void *` is a generic object pointer: any object pointer can round-trip through it, but
you cannot dereference a `void *` or do portable arithmetic on it until you choose a real
target type again.

Function pointers are separate from object pointers. They often look like addresses on
desktop machines, but the C rules for `void *`, object representation, and arithmetic do
not apply to them. This branch's first pass is about object pointers; function pointers
get their own note later.

## How it really works

Start with a real object:

```c
int x = 7;
int *p = &x;
```

`x` occupies `sizeof(int)` bytes somewhere in the process's virtual address space. `p` is
another object, with its own storage, whose value locates `x`. On a typical LP64 x86-64
process, `sizeof p` is 8 and `sizeof *p` is 4. The first number is the size of the
pointer object; the second is the size of the object you get after following it.

When the compiler sees `*p = 42`, it emits a store through the address held in `p`, using
the target type to choose the access width and alignment. For `int *`, that means an
`int` store. For `unsigned char *`, it means a one-byte store. The type does not sit in
memory next to the object; it is part of the expression the compiler is compiling.

Pointer assignment aliases, not clones:

```c
int *q = p;
*q = 99;  // writes x, because q and p locate the same int
```

This is the foundation of out-parameters, linked data structures, and every memory bug
that starts with "two names for the same storage." The language lets you carry locations
around cheaply; it does not remember who owns them for you.

Pointer arithmetic is also typed. If `p` points at `a[0]` in an `int a[3]`, then `p + 1`
points at `a[1]`, not at the next byte. The only portable arithmetic targets are elements
of the same array object and the one-past pointer used for loop termination. Walking
outside that boundary is undefined behavior even if the printed numeric address looks
plausible.

Finally, a C pointer value is process-local. Address Space Layout Randomization (ASLR)
changes where objects land between runs, and virtual addresses are meaningful only inside
the running process that owns that address space. Use `uintptr_t` from `<stdint.h>` for
diagnostics, hashing, or low-level interfaces when the implementation provides it; do not
serialize raw pointer values into file formats or network protocols.

## Executable artifact: inspect one pointer

The demo lives in `examples/pointers-and-memory/what-a-pointer-really-is/demo.c`.

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static void add_ten(int *slot) {
    if (slot != NULL) {
        *slot += 10;
    }
}

static void print_first_int_bytes(const int *value) {
    const unsigned char *bytes = (const unsigned char *)(const void *)value;

    printf("first int bytes       =");
    for (size_t i = 0; i < sizeof *value; i++) {
        printf(" %02x", bytes[i]);
    }
    printf("\n");
}

int main(void) {
    int cells[3] = {11, 22, 33};
    int *p = &cells[0];
    int *alias = p;
    void *generic = p;
    int *from_void = generic;
    int *none = NULL;

    ptrdiff_t element_delta = &cells[1] - &cells[0];
    ptrdiff_t byte_delta = (const unsigned char *)(const void *)&cells[1] -
                           (const unsigned char *)(const void *)&cells[0];

    printf("sizeof p              = %zu bytes\n", sizeof p);
    printf("sizeof *p             = %zu bytes\n", sizeof *p);
    printf("&cells[0]             = %p\n", (void *)&cells[0]);
    printf("&cells[1]             = %p\n", (void *)&cells[1]);
    printf("element delta         = %td element\n", element_delta);
    printf("byte delta            = %td bytes\n", byte_delta);
    printf("*p before             = %d\n", *p);

    *alias = 41;
    add_ten(p);

    printf("cells[0] after alias  = %d\n", cells[0]);
    printf("*(p + 1)              = %d\n", *(p + 1));
    printf("void* round trip      = %s\n",
           from_void == p ? "same pointer" : "different pointer");

#if defined(UINTPTR_MAX)
    uintptr_t bits = (uintptr_t)(void *)p;
    printf("uintptr_t round trip  = %s\n",
           (void *)bits == (void *)p ? "same bits" : "changed bits");
#endif

    print_first_int_bytes(p);
    printf("null comparison       = %s\n", none == NULL ? "NULL" : "not NULL");

    return 0;
}
```

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

One real output from the demo on a 64-bit little-endian machine:

```
sizeof p              = 8 bytes
sizeof *p             = 4 bytes
&cells[0]             = 0x7ff7b7c1729c
&cells[1]             = 0x7ff7b7c172a0
element delta         = 1 element
byte delta            = 4 bytes
*p before             = 11
cells[0] after alias  = 51
*(p + 1)              = 22
void* round trip      = same pointer
uintptr_t round trip  = same bits
first int bytes       = 33 00 00 00
null comparison       = NULL
```

The two addresses differ by 4 bytes because `cells` is an `int[]` and this machine's
`int` is 4 bytes. The element delta is 1 because pointer subtraction reports elements,
not bytes. `alias` and `p` locate the same `int`, so writing through either one changes
`cells[0]`. The first byte is `0x33` because `51` decimal is `0x33`, and this run is
little-endian.

## Failure modes & trade-offs

- **Uninitialized pointer.** `int *p; *p = 1;` uses an indeterminate pointer value. It is
  not "somewhere"; it is undefined behavior.
- **Dangling pointer.** A pointer can outlive the object it used to locate. Returning the
  address of a local, using a pointer after `free`, or keeping a pointer into a resized
  allocation all leave you with a value that may still print nicely and still be invalid.
- **Null pointer dereference.** `NULL` is a real sentinel value, not a small object at
  address zero. It is useful for "points nowhere"; dereferencing it is undefined behavior.
- **Arithmetic outside one array.** `p + n` is portable only within the same array object
  or one-past it. Treating memory as one giant walkable byte street is a machine habit,
  not a C guarantee.
- **Wrong target type.** Casting an address to an incompatible pointer type and
  dereferencing it can violate alignment and aliasing rules. `unsigned char *` is the
  safe byte-inspection escape hatch; arbitrary type punning is not.
- **Pointer-as-integer thinking.** `uintptr_t` is useful when available, but the integer
  is not stable across processes, not a portable file format, and not permission to
  invent valid pointers from arbitrary numbers.

## In practice

- **Read `T *p` as "`p` points at `T`."** The star binds to the declarator, not the base
  type; `int *a, b;` declares one pointer and one plain `int`.
- **Track length and ownership next to the pointer.** A naked pointer does not know how
  many elements it can access or who must free the storage. APIs should carry that
  information explicitly.
- **Cast to `void *` for `%p`.** `printf("%p", (void *)p)` is the portable shape for
  printing object pointers.
- **Use `const` to say "I will not write through this pointer."** `const int *p` protects
  the pointed-to object through that access path; it does not make the address immortal or
  prove ownership.
- **Debug pointer bugs with lifetimes, not just addresses.** The number can look right
  while the object is dead, out of bounds, misaligned, or accessed through the wrong type.

**Connects to:** [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words & addresses]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/process-address-space-and-virtual-memory|Process address space & virtual memory]] · [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for pointer types, address-of, indirection, null pointers, pointer conversion, pointer arithmetic, and undefined behavior boundaries. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Pointer declaration** — compact reference for object pointers, function pointers, `void *`, null pointers, and pointer-to-pointer syntax. https://en.cppreference.com/w/c/language/pointer
- **cppreference — Operator arithmetic** — the exact rules for pointer addition, subtraction, one-past pointers, and array bounds. https://en.cppreference.com/w/c/language/operator_arithmetic
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 2, 3, and 9 — bytes, addresses, x86-64 machine code, and virtual address spaces. https://csapp.cs.cmu.edu/
- **System V AMD64 ABI** — the LP64 data model and object-pointer size assumptions for the x86-64 Unix-like target used throughout the atlas. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Jens Gustedt — *Modern C*** — modern C treatment of pointer types, object lifetimes, `const`, and pointer-plus-size API discipline. https://gustedt.gitlabpages.inria.fr/modern-c/
