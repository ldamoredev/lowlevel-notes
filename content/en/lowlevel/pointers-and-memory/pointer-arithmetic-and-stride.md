---
title: "Pointer arithmetic & stride"
description: Pointer arithmetic moves in elements, not bytes. The pointed-to type defines the stride, pointer subtraction reports elements, and only one array object plus its one-past position is portable territory.
tags: [pointers, pointer-arithmetic, arrays, stride, c]
order: 2
updated: 2026-06-30
---
# Pointer arithmetic & stride

Pointer arithmetic is typed arithmetic over array positions. If `p` is an `int *`, then
`p + 1` means "the next `int`", not "the next byte"; if `p` is a `struct Packet *`, it
means "the next whole packet." The stride comes from `sizeof *p`, and C only gives this
meaning inside one array object, including the special one-past pointer used to stop
iteration. Outside that boundary, the numeric address may look sensible, but the C program
has left defined behavior.

> The reset: pointer arithmetic answers "which element?" Byte arithmetic answers "which
> address?" In C, `p + n` is element math; cast to `unsigned char *` only when you
> intentionally want byte-level inspection.

## The stride rule

For an object pointer `T *p`, adding an integer `n` produces a pointer `n` elements away:

| Expression | Unit | Meaning |
|---|---|---|
| `p + 1` | `sizeof *p` bytes | next element of the same pointed-to type |
| `p - 1` | `sizeof *p` bytes | previous element, if still inside the same array |
| `q - p` | elements | distance between two pointers into the same array |
| `(unsigned char *)p + 1` | 1 byte | next byte of the object's representation |
| `end = a + count` | one-past | valid sentinel, not valid to dereference |

Array indexing is just this rule plus dereference:

```c
a[i] == *(a + i)
```

That equality is why arrays and pointers feel interchangeable in expression contexts, but
the boundary rule still matters. The valid positions are `&a[0]` through `&a[count - 1]`,
plus `&a[count]` as one-past. You may compare the one-past pointer or subtract it from
another pointer into the same array. You may not read or write through it.

## How it really works

The compiler scales the integer by the pointed-to type. Conceptually, if `p` points at a
byte address `A`, then `p + n` points at:

```text
A + n * sizeof *p
```

That formula is a mental model, not permission to treat every pointer as an integer. The C
operation is defined over array elements. An `int[4]`, a `double[4]`, and a
`struct Packet[4]` all occupy contiguous storage, but their pointer steps are different
because their element sizes are different.

Pointer subtraction reverses the same scaling. `&a[3] - &a[0]` is `3`, not the byte count
between the two addresses. The result type is `ptrdiff_t`, a signed integer type from
`<stddef.h>` intended for pointer differences. Subtracting pointers that do not point into
the same array object is undefined behavior, even if both addresses came from nearby stack
locals.

`unsigned char *` is the explicit byte-level escape hatch. C lets you inspect any object's
representation as a sequence of character bytes, so a byte pointer is the right tool for
dumping raw memory, parsing binary formats, or implementing allocators. `void *` is not
that tool: it is a generic object pointer, but portable C does not define arithmetic on
`void *` because `void` has no size.

The rule also explains why struct arrays matter. Pointer arithmetic over
`struct Packet *` jumps by the full struct size, including any padding. You do not have to
know the byte offset manually; the type carries the stride. That is powerful when the type
is correct, and dangerous when you cast a pointer to the wrong target type.

## Executable artifact: stride is type

The demo lives in `examples/pointers-and-memory/pointer-arithmetic-and-stride/demo.c`.

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct Packet {
    uint16_t length;
    uint8_t tag;
    uint8_t payload[5];
};

static ptrdiff_t byte_distance(const void *a, const void *b) {
    const unsigned char *left = a;
    const unsigned char *right = b;
    return right - left;
}

static void print_stride(const char *label,
                         size_t element_size,
                         const void *first,
                         const void *second) {
    printf("%-10s sizeof=%2zu  byte stride=%2td\n",
           label, element_size, byte_distance(first, second));
}

int main(void) {
    int numbers[4] = {10, 20, 30, 40};
    double weights[3] = {1.5, 2.5, 3.5};
    struct Packet packets[2] = {
        {.length = 5, .tag = 7, .payload = {1, 2, 3, 4, 5}},
        {.length = 3, .tag = 9, .payload = {6, 7, 8, 0, 0}},
    };

    print_stride("int", sizeof numbers[0], &numbers[0], &numbers[1]);
    print_stride("double", sizeof weights[0], &weights[0], &weights[1]);
    print_stride("Packet", sizeof packets[0], &packets[0], &packets[1]);

    printf("numbers[2] via *(numbers + 2) = %d\n", *(numbers + 2));
    printf("packets[1].tag via pointer    = %u\n",
           (unsigned int)(packets + 1)->tag);

    int *begin = numbers;
    int *end = numbers + (sizeof numbers / sizeof numbers[0]);
    int sum = 0;

    for (int *it = begin; it != end; it++) {
        sum += *it;
    }

    printf("one-past delta               = %td elements\n", end - begin);
    printf("sum walked by pointer        = %d\n", sum);

    const unsigned char *bytes = (const unsigned char *)(const void *)numbers;
    printf("first int raw bytes          =");
    for (size_t i = 0; i < sizeof numbers[0]; i++) {
        printf(" %02x", bytes[i]);
    }
    printf("\n");

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
int        sizeof= 4  byte stride= 4
double     sizeof= 8  byte stride= 8
Packet     sizeof= 8  byte stride= 8
numbers[2] via *(numbers + 2) = 30
packets[1].tag via pointer    = 9
one-past delta               = 4 elements
sum walked by pointer        = 100
first int raw bytes          = 0a 00 00 00
```

The first three lines show the stride. `int *`, `double *`, and `struct Packet *` move by
different byte counts because the pointed-to element sizes differ. `end - begin` reports
`4` elements, not 16 bytes. The byte dump switches deliberately to `unsigned char *`, so
the same memory is now walked one byte at a time.

## Failure modes & trade-offs

- **Off-by-one dereference.** `end = a + count` is a valid pointer value for comparison.
  `*end` is undefined behavior. The sentinel is not an element.
- **Wrong units.** Adding a byte count to `T *` multiplies by `sizeof(T)`. If you have
  bytes, use a byte pointer; if you have elements, use a typed pointer.
- **Subtracting unrelated pointers.** Pointer difference is defined only within the same
  array object. It is not a general address-distance operator.
- **Casting away the stride.** A cast can make arithmetic compile while making the target
  type false. The next dereference may violate alignment, aliasing, or object lifetime.
- **`void *` arithmetic.** Some compilers accept it as an extension that treats `void` as
  size 1. Portable C does not; cast to `unsigned char *` for byte walking.
- **Integer address math.** Converting to `uintptr_t` can be useful for diagnostics, but
  integer arithmetic does not manufacture a valid pointer unless the implementation and
  surrounding API explicitly say so.

## In practice

- **Carry counts in elements, not bytes, for typed arrays.** `int *items, size_t count`
  means `items + count` is the natural one-past pointer.
- **Name byte counts loudly.** Use names like `byte_count`, `byte_offset`, and `stride`
  when the unit is bytes; reserve `count` for elements.
- **Prefer `for (T *it = begin; it != end; it++)` for tight C loops.** It makes the
  one-past sentinel explicit and keeps the bounds in the same type system as the access.
- **Switch to `unsigned char *` at API boundaries that deal in raw storage.** Allocators,
  serializers, checksums, and binary parsers are byte-oriented; ordinary arrays are
  element-oriented.
- **Let the type carry struct stride.** Arrays of structs, arrays of rows, and typed
  buffers become much simpler when the pointer type matches the storage layout.

**Connects to:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words & addresses]] · [[lowlevel/machine-model/endianness|Endianness]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for additive operators on pointers, pointer subtraction, one-past pointers, and undefined behavior boundaries. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Operator arithmetic** — compact reference for pointer addition, subtraction, `ptrdiff_t`, array bounds, and one-past rules. https://en.cppreference.com/w/c/language/operator_arithmetic
- **cppreference — Pointer declaration** — object pointers, `void *`, null pointers, and pointer conversions in C. https://en.cppreference.com/w/c/language/pointer
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 2 — byte-addressed memory and the machine view underneath typed pointer arithmetic. https://csapp.cs.cmu.edu/
- **Jens Gustedt — *Modern C*** — modern treatment of arrays, pointer-plus-size APIs, and why byte-oriented code should be explicit. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Richard Reese — *Understanding and Using C Pointers*** — practical pointer arithmetic examples and the bugs caused by mixing byte and element units. https://www.oreilly.com/library/view/understanding-and-using/9781449344535/
