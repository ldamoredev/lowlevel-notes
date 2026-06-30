---
title: "Struct layout: alignment & padding"
description: C structs are laid out in declaration order, but alignment inserts padding between fields and at the end. Field order affects size, array stride, cache use, and binary compatibility.
tags: [structs, alignment, padding, layout, offsetof]
order: 9
updated: 2026-06-30
---
# Struct layout: alignment & padding

A C `struct` is a sequence of fields in declaration order, with invisible padding inserted
so each field can satisfy its alignment requirement. The compiler may also add tail
padding so every element in an array of that struct is properly aligned. That means field
order changes `sizeof`, array stride, cache footprint, and binary compatibility. The
bytes you did not write are still part of the object representation.

> The reset: structs are not just "field bags." They are byte layouts chosen by the C
> implementation under alignment rules. `sizeof(struct T)` includes padding.

## Alignment creates holes

Most machines prefer or require that objects of certain types start at addresses that are
multiples of some alignment. A `double` commonly wants 8-byte alignment; an `int` commonly
wants 4. If a `char` is followed by a `double`, the compiler inserts padding between them:

```c
struct BadOrder {
    char tag;
    double score;
    int count;
};
```

A better field order can reduce padding:

```c
struct BetterOrder {
    double score;
    int count;
    char tag;
};
```

This does not change semantics if code only uses field names. It does change ABI, file
formats, network layouts, and any code that assumes offsets.

## How it really works

The C standard guarantees that non-bit-field struct members appear in declaration order
and that a pointer to a struct object, converted appropriately, points at its initial
member. It does not promise there is no padding. It also does not promise the same layout
across compilers, targets, ABI settings, packing pragmas, or different definitions.

Use `offsetof(type, member)` from `<stddef.h>` to ask the compiler for a field offset:

```c
offsetof(struct BetterOrder, count)
```

Use `_Alignof(type)` to ask for alignment in modern C. Together with `sizeof`, these are
the tools for auditing layout without guessing.

Tail padding matters in arrays. If `sizeof(struct BetterOrder)` is 16, then
`items[i + 1]` starts 16 bytes after `items[i]` even if the named fields only account for
13 bytes. The extra bytes keep the next element's `double` aligned.

Padding bytes have unspecified values after ordinary assignment to fields. Comparing whole
struct objects with `memcmp`, hashing raw struct bytes, or writing structs directly to
disk can accidentally include uninitialized padding. For stable binary formats, serialize
field by field with explicit widths and byte order.

Packed structs reduce padding but can create misaligned accesses. On some targets that is
slow; on others it traps. Packing is for external binary layouts and hardware protocols,
not for routine memory savings without measurement.

## Executable artifact: print offsets and stride

The demo lives in `examples/pointers-and-memory/struct-layout-alignment-and-padding/demo.c`.

```c
// demo.c - shows `sizeof`, `_Alignof`, `offsetof`, field-order padding,
// and the stride of arrays of structs.
// Compiles cleanly and runs with:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>

struct BadOrder {
    char tag;
    double score;
    int count;
};

struct BetterOrder {
    double score;
    int count;
    char tag;
};

struct Header {
    unsigned char kind;
    unsigned char flags;
    unsigned short length;
};

int main(void) {
    printf("BadOrder sizeof       = %zu align=%zu\n",
           sizeof(struct BadOrder), _Alignof(struct BadOrder));
    printf("  tag offset          = %zu\n", offsetof(struct BadOrder, tag));
    printf("  score offset        = %zu\n", offsetof(struct BadOrder, score));
    printf("  count offset        = %zu\n", offsetof(struct BadOrder, count));

    printf("BetterOrder sizeof    = %zu align=%zu\n",
           sizeof(struct BetterOrder), _Alignof(struct BetterOrder));
    printf("  score offset        = %zu\n", offsetof(struct BetterOrder, score));
    printf("  count offset        = %zu\n", offsetof(struct BetterOrder, count));
    printf("  tag offset          = %zu\n", offsetof(struct BetterOrder, tag));

    struct BetterOrder items[2] = {
        {.score = 1.5, .count = 2, .tag = 'a'},
        {.score = 2.5, .count = 3, .tag = 'b'},
    };

    ptrdiff_t stride = (const char *)(const void *)&items[1] -
                       (const char *)(const void *)&items[0];
    printf("array stride          = %td\n", stride);

    printf("Header sizeof         = %zu align=%zu\n",
           sizeof(struct Header), _Alignof(struct Header));

    return 0;
}
```

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Real output on this machine:

```
BadOrder sizeof       = 24 align=8
  tag offset          = 0
  score offset        = 8
  count offset        = 16
BetterOrder sizeof    = 16 align=8
  score offset        = 0
  count offset        = 8
  tag offset          = 12
array stride          = 16
Header sizeof         = 4 align=2
```

Both structs hold the same field types. The bad order spends space aligning `score` after
`tag` and padding the end. The better order packs the smaller fields after the `double`,
so the array stride drops from 24 bytes to 16 bytes.

## Failure modes & trade-offs

- **Assuming no padding.** `sizeof` can exceed the sum of field sizes. That difference is
  real storage and affects arrays.
- **Raw serialization.** Writing a struct with `fwrite(&s, sizeof s, 1, f)` bakes in
  padding, endianness, type widths, and ABI layout.
- **`memcmp` on structs.** Two structs with equal fields can differ in padding bytes.
  Compare fields, not raw bytes, unless the type is explicitly designed for byte compare.
- **Changing field order in public ABI.** Reordering fields can break binary users even
  when source code still compiles.
- **Overusing packing.** Packed structs may save bytes but cause misaligned loads/stores.
  Use them at boundaries, then copy into aligned internal structs if needed.
- **False memory savings.** Reordering one struct matters only if many instances exist or
  the struct sits in a hot path. Measure before contorting readable types.

## In practice

- **Order fields from strictest alignment to loosest when size matters.** Commonly:
  pointers/doubles, then integers, then shorts/chars/flags.
- **Use `offsetof` in layout assertions.** If a layout is part of an ABI, test the offsets
  you require.
- **Separate wire layout from runtime layout.** Parse bytes into explicit fields; do not
  make the compiler's struct layout your protocol.
- **Watch array stride.** The cost of padding multiplies by element count.
- **Group hot fields together.** Layout is not only about size; it is also about what data
  arrives together in a cache line.

**Connects to:** [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Pointer arithmetic & stride]] · [[lowlevel/pointers-and-memory/data-layout-and-cache-friendliness|Data layout & cache-friendliness]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words & addresses]] · [[lowlevel/machine-model/cache-lines-and-locality|Cache lines & locality]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — struct member order, padding, alignment, object representation, `offsetof`, and `_Alignof`. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Structure declaration** — struct layout rules, padding, flexible array members, and initialization. https://en.cppreference.com/w/c/language/struct
- **cppreference — `_Alignof` / alignment** — C alignment queries and alignment requirements. https://en.cppreference.com/w/c/language/_Alignof
- **System V AMD64 ABI** — concrete data layout and alignment rules for the x86-64 Unix-like target used in the atlas. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — data representation, alignment, and memory layout in systems programs. https://csapp.cs.cmu.edu/
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — alignment, cache behavior, and why layout affects performance. https://www.akkadia.org/drepper/cpumemory.pdf
