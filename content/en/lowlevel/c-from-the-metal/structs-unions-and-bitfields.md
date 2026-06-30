---
title: "Structs, unions, and bitfields"
description: Structs give names to laid-out bytes, unions reuse the same storage for one active interpretation, and bitfields pack small integer fields with implementation-defined layout. The useful parts, the padding traps, and what not to serialize.
tags: [c, structs, unions, bitfields, layout, padding]
order: 8
updated: 2026-06-29
---
# Structs, unions, and bitfields

C aggregates are where the type system meets memory layout. A `struct` lays out named
members in order, with padding inserted so each member is properly aligned. A `union`
overlays several member types on the same storage, so only one interpretation is meant to
be active at a time. Bitfields let you name small integer fields inside a storage unit,
but their exact layout is deliberately implementation-defined. These tools are powerful
because they are close to bytes; they are dangerous for the same reason.

> The reset: `struct` is not a JSON object, `union` is not a safe sum type, and bitfields
> are not a portable wire format. They are C's way to describe objects that occupy bytes.

## Structs are layout with padding

A `struct` stores its members in declaration order. The compiler may insert unnamed bytes
between members and after the last member so that each object obeys the target's alignment
rules. That padding is part of the object representation, but it is not a field you can
read as a value.

```c
struct PacketHeader {
    uint8_t tag;
    uint32_t length;
    uint16_t flags;
};
```

On a typical 64-bit target, `tag` starts at offset 0, then three padding bytes appear so
`length` can start at offset 4. `flags` starts at offset 8, and tail padding rounds the
whole object up to the struct's alignment. Reordering members by decreasing alignment can
reduce padding:

```c
struct CompactHeader {
    uint32_t length;
    uint16_t flags;
    uint8_t tag;
};
```

That is not an aesthetic tweak; it changes `sizeof`, array stride, cache footprint, and ABI
expectations. Use `sizeof`, `_Alignof`, and `offsetof` to ask the compiler what it actually
chose. Do not count bytes by eye once padding is possible.

## How it really works

`struct` definitions create aggregate types. Assignment copies the whole object, including
padding bytes as representation; passing a struct by value copies it according to the ABI;
arrays of structs use `sizeof(struct T)` as the stride from one element to the next.
Designated initializers make the mapping explicit:

```c
struct PacketHeader header = {
    .tag = 7,
    .length = 1024,
    .flags = 3,
};
```

A `union` allocates enough storage for its largest member, with enough alignment for the
strictest member. All members start at offset 0. That makes a union useful for tagged data:
keep a separate tag that says which member is currently meaningful.

```c
union Payload {
    uint32_t as_u32;
    double as_double;
};

struct Value {
    enum ValueKind kind;
    union Payload payload;
};
```

Without the tag, a union is only shared storage. Reading a member other than the one most
recently written is not a portable way to deserialize bytes or bypass the type system; it
depends on representation details and can run into trap values, effective type, and strict
aliasing rules. If you need bytes, copy bytes with `memcpy`. If you need a sum type, carry
the tag.

Bitfields attach widths to integer members:

```c
struct StatusBits {
    unsigned ready : 1;
    unsigned error : 1;
    unsigned mode : 3;
    unsigned reserved : 3;
};
```

They are useful for compact in-memory flags and register-shaped code when you also control
the compiler and target. But the standard leaves important choices to the implementation:
allocation order inside the storage unit, whether fields cross allocation units, the
signedness of plain `int` bitfields, and padding. You cannot take the address of a
bitfield, and `offsetof` does not apply to one.

## Executable artifact: ask the compiler

The demo lives in `examples/c-from-the-metal/structs-unions-and-bitfields/demo.c`. It asks
the compiler for offsets and sizes instead of assuming them.

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct PacketHeader {
    uint8_t tag;
    uint32_t length;
    uint16_t flags;
};

struct CompactHeader {
    uint32_t length;
    uint16_t flags;
    uint8_t tag;
};

union Payload {
    uint32_t as_u32;
    double as_double;
};

enum ValueKind {
    VALUE_U32,
    VALUE_DOUBLE,
};

struct Value {
    enum ValueKind kind;
    union Payload payload;
};

struct StatusBits {
    unsigned ready : 1;
    unsigned error : 1;
    unsigned mode : 3;
    unsigned reserved : 3;
};

static void print_packet_layout(void) {
    printf("PacketHeader size     = %zu bytes\n", sizeof(struct PacketHeader));
    printf("PacketHeader align    = %zu bytes\n", _Alignof(struct PacketHeader));
    printf("  tag offset          = %zu\n", offsetof(struct PacketHeader, tag));
    printf("  length offset       = %zu\n", offsetof(struct PacketHeader, length));
    printf("  flags offset        = %zu\n", offsetof(struct PacketHeader, flags));
    printf("CompactHeader size    = %zu bytes\n", sizeof(struct CompactHeader));
}

static void print_value(struct Value value) {
    if (value.kind == VALUE_U32) {
        printf("tagged union value    = u32:%u\n", value.payload.as_u32);
        return;
    }

    printf("tagged union value    = double:%.2f\n", value.payload.as_double);
}

int main(void) {
    struct PacketHeader header = {
        .tag = 7,
        .length = 1024,
        .flags = 3,
    };
    struct Value id = {
        .kind = VALUE_U32,
        .payload.as_u32 = 42,
    };
    struct Value ratio = {
        .kind = VALUE_DOUBLE,
        .payload.as_double = 0.75,
    };
    struct StatusBits status = {
        .ready = 1,
        .error = 0,
        .mode = 5,
        .reserved = 0,
    };

    /* Field order changes the padding inserted by the compiler. */
    print_packet_layout();

    printf("header values         = tag:%u length:%u flags:%u\n",
           (unsigned)header.tag, header.length, (unsigned)header.flags);
    printf("Payload union size    = %zu bytes\n", sizeof(union Payload));
    printf("Value struct size     = %zu bytes\n", sizeof(struct Value));

    /* The external tag says which union member is active. */
    print_value(id);
    print_value(ratio);

    /* Bitfields are useful for in-memory flags, not portable formats. */
    printf("StatusBits size       = %zu bytes\n", sizeof(struct StatusBits));
    printf("status bits           = ready:%u error:%u mode:%u\n",
           status.ready, status.error, status.mode);

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
PacketHeader size     = 12 bytes
PacketHeader align    = 4 bytes
  tag offset          = 0
  length offset       = 4
  flags offset        = 8
CompactHeader size    = 8 bytes
header values         = tag:7 length:1024 flags:3
Payload union size    = 8 bytes
Value struct size     = 16 bytes
tagged union value    = u32:42
tagged union value    = double:0.75
StatusBits size       = 4 bytes
status bits           = ready:1 error:0 mode:5
```

The first layout wastes space because `length` needs 4-byte alignment after a 1-byte
field. The compact order fits the same values in 8 bytes on this target. The union is 8
bytes because `double` is the largest and strictest member. `struct Value` is larger than
the union because the tag and alignment padding also occupy space. `StatusBits` taking 4
bytes is a compiler choice, not a promise you should put on disk.

## Failure modes & trade-offs

- **Assuming no padding.** `sizeof(struct T)` can be larger than the sum of members. Arrays,
  binary I/O, networking, hashing, and `memcmp` all care.
- **Serializing raw structs.** Padding bytes, endianness, alignment, and compiler ABI make
  raw `fwrite(&header, sizeof header, 1, file)` a portability trap. Serialize fields
  deliberately.
- **Using unions as untagged variants.** A union does not remember which member is active.
  Store a tag next to it, or your reader is guessing.
- **Type punning through unions.** Some compilers support common union-punning idioms, but
  portable C should prefer `memcpy` for object representations and explicit conversion for
  values.
- **Bitfield layout dependence.** Bit order and allocation are not portable. For files,
  packets, and cross-compiler ABIs, use masks and shifts on fixed-width integers.
- **Packing pragmas cut both ways.** Compiler extensions can remove padding, but they may
  create misaligned loads, slower code, or ABI mismatch. Use them at boundaries, not as a
  default style.

## In practice

- **Use `offsetof` and `sizeof` as instrumentation.** When layout matters, write a small
  check or `static_assert` instead of trusting intuition.
- **Order hot structs intentionally.** Group fields by alignment and access pattern, but do
  not reorder public ABI structs casually.
- **Prefer tagged unions for variants.** `enum kind` plus `union payload` is the C pattern;
  every function should switch on the tag before reading the payload.
- **Use fixed-width integers for binary formats.** Then encode endianness and bit layout
  explicitly with masks, shifts, and byte writes.
- **Keep bitfields local.** They are fine for private flags on one compiler/target. They
  are suspicious in public headers, persistent storage, and protocol definitions.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|The C type system is weak]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: the contract]] · [[lowlevel/c-from-the-metal/arrays-and-array-to-pointer-decay|Arrays and array-to-pointer decay]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words & addresses]] · [[lowlevel/machine-model/endianness|Endianness]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for structures, unions, bitfields, alignment, padding, member access, and object representations. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Struct and union initialization** — practical reference for aggregate and designated initialization of structs and unions. https://en.cppreference.com/c/language/struct_initialization
- **cppreference — Struct declaration** — layout rules, bitfield syntax, anonymous members, and constraints such as not taking a bitfield address. https://en.cppreference.com/c/language/struct
- **cppreference — Object model and alignment** — object representation, padding, alignment, trap representations, and effective type background. https://en.cppreference.com/c/language/object
- **System V AMD64 ABI** — how aggregate layout and calling conventions become ABI on x86-64 Unix-like systems. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Jens Gustedt — *Modern C*** — modern guidance on aggregate types, initializers, object representation, and portable layout discipline. https://gustedt.gitlabpages.inria.fr/modern-c/
