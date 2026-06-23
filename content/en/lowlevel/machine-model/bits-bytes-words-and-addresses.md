---
title: "Bits, bytes, words & addresses"
description: The vocabulary of raw memory — a bit is the atom, a byte (8 bits) is the smallest addressable unit, a word is the CPU's natural chunk, and an address is just an index into one giant byte array. Why type sizes aren't guaranteed and when to reach for stdint.
tags: [machine-model, bits, bytes, words, addresses, stdint]
order: 7
updated: 2026-06-22
---
# Bits, bytes, words & addresses

Strip away every abstraction and memory is one enormous array of **bytes**, each with a
numeric **address** — its index. A **bit** is the atom: a single 0 or 1. Eight bits make
a **byte**, the smallest unit the machine can individually address. A **word** is the
chunk the CPU prefers to move and compute on in one go — 64 bits on x86-64. Everything
else in this atlas — pointers, structs, integers, strings — is a particular way of
interpreting bytes at addresses. This note nails down that vocabulary, because sloppy
intuitions here (an `int` is "a number", an address is "a thing") cause real bugs later.

> The reset: there are no "variables" in memory, only bytes at addresses. The type system
> is a story the compiler tells about which bytes mean what. Underneath, it's `byte[2^64]`.

## The ladder: bit → byte → word

| Unit | Size | What it is |
|---|---|---|
| bit | 1 bit | the atom: 0 or 1 |
| nibble | 4 bits | one hex digit (`0x0`–`0xf`); half a byte |
| **byte** | 8 bits | the smallest *addressable* unit; holds 0–255 |
| **word** | machine-dependent | the CPU's natural operand width (64-bit on x86-64) |

Two pinned-down facts. **A byte is 8 bits** in any environment you'll touch — C exposes
this as `CHAR_BIT`, and while the standard only mandates *at least* 8, every modern
machine is exactly 8. And **"word" is overloaded**, which trips people up:

- A **machine word** = the register/natural width = **64 bits** on x86-64 and ARM64.
- In **x86 assembler**, `word` means **16 bits** for historical reasons (the 16-bit
  8086), with `dword` = 32 and `qword` = 64. Same syllable, different size — always read
  it in context.

## Addresses name bytes

Memory is **byte-addressable**: every individual byte has its own address, and an address
is simply an integer index into that flat array. A **pointer** is a value holding such an
address; its width equals the address width, so on a 64-bit machine `sizeof(void*) == 8`
and the address space is, in principle, 2⁶⁴ bytes (real hardware wires up far fewer bits).

Because addresses count *bytes*, consecutive elements of an `int[]` sit `sizeof(int)`
bytes apart — 4 on a typical machine:

```
&a[0] = 0x...970
&a[1] = 0x...974   <- +4 bytes
&a[2] = 0x...978
&a[3] = 0x...97c
```

This is exactly why pointer arithmetic scales by element size rather than by 1 — the
subject of [[lowlevel/pointers-and-memory/index|pointers & memory]]. The key idea now:
an address is a byte index, and a type tells the compiler how many bytes to read there
and how to interpret them.

## Sizes are not guaranteed

A trap for anyone arriving from fixed-size managed types: **C does not fix `int` at 32
bits.** The standard only guarantees minimum ranges and an ordering
`sizeof(char) ≤ sizeof(short) ≤ sizeof(int) ≤ sizeof(long) ≤ sizeof(long long)`. Actual
sizes are implementation-defined. The common 64-bit layouts:

| Type | LP64 (Linux/macOS) | LLP64 (Windows) |
|---|---|---|
| `char` | 1 | 1 |
| `short` | 2 | 2 |
| `int` | 4 | 4 |
| `long` | **8** | **4** |
| `long long` | 8 | 8 |
| `void*` | 8 | 8 |

Notice `long` is 8 bytes on Linux/macOS but 4 on Windows — a classic portability bug.
When the exact width matters (file formats, network protocols, hardware registers, an OS
kernel), don't use `int`/`long`; use the **fixed-width types** from `<stdint.h>`:
`int8_t`, `uint16_t`, `int32_t`, `uint64_t`, and `uintptr_t` for "an integer big enough
to hold a pointer." These mean the same thing on every platform.

## A word is just bytes (a peek at endianness)

Take the 32-bit value `0x11223344` and look at its four bytes *in memory order* with a
`char*`:

```
the 4 bytes of 0x11223344 in memory order:
  44 33 22 11
```

The least-significant byte (`44`) is stored first. That byte ordering is **endianness** —
x86-64 is little-endian — and it has its own note coming. The point here: a "number" is
not atomic; it's a sequence of bytes at consecutive addresses, and you can inspect or
reinterpret those bytes directly — which is why byte *order*
([[lowlevel/machine-model/endianness|endianness]]) bites when bytes cross machines or
wires.

## See it

```c
// bytes.c — sizes, byte addressing, and the bytes inside a word.
// gcc -O0 -Wall -Wextra bytes.c -o bytes && ./bytes
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

int main(void) {
    printf("CHAR_BIT (bits per byte) = %d\n\n", CHAR_BIT);

    printf("type    bytes  bits\n");
    printf("char    %5zu  %4zu\n", sizeof(char),  sizeof(char)  * CHAR_BIT);
    printf("int     %5zu  %4zu\n", sizeof(int),   sizeof(int)   * CHAR_BIT);
    printf("long    %5zu  %4zu\n", sizeof(long),  sizeof(long)  * CHAR_BIT);
    printf("void*   %5zu  %4zu\n", sizeof(void*), sizeof(void*) * CHAR_BIT);

    int a[4];                                   // byte addressing
    printf("\neach int is %zu bytes apart:\n", sizeof(int));
    for (int i = 0; i < 4; i++)
        printf("  &a[%d] = %p\n", i, (void*)&a[i]);

    uint32_t w = 0x11223344u;                   // a word is just bytes
    unsigned char *p = (unsigned char*)&w;
    printf("\nthe 4 bytes of 0x11223344 in memory order:\n  ");
    for (size_t i = 0; i < sizeof w; i++) printf("%02x ", p[i]);
    printf("\n");
    return 0;
}
```

`sizeof` returns a count of **bytes**; multiply by `CHAR_BIT` for bits. The address loop
shows byte addressing (consecutive `int`s 4 apart), and the `char*` cast lets you read a
word one byte at a time — the foundation of how every larger type is built from bytes.

## Failure modes & trade-offs

- **Assuming `int` is 32 bits / `long` is 64.** True on LP64, false on Windows LLP64 and
  on small embedded targets. Code that hard-codes widths breaks when ported.
- **Assuming `sizeof(void*) == sizeof(int)`.** On 64-bit machines a pointer is 8 bytes and
  `int` is 4 — storing a pointer in an `int` truncates it. A textbook source of crashes.
- **`sizeof` is bytes, not bits.** `sizeof(int)` is 4, not 32. Mixing the two up corrupts
  bit-level math.
- **KB vs KiB.** "Kilobyte" is ambiguous; 1 KiB = 1024 bytes, 1 KB = 1000. Memory and
  cache sizes are powers of two (KiB/MiB); be explicit when it matters.

## In practice

- **Default to `int` for ordinary counters; reach for `<stdint.h>` when layout matters.**
  Fixed-width types (`uint32_t`, `int64_t`, `uintptr_t`) make on-the-wire and on-disk
  formats portable and unambiguous.
- **Think "address = byte index, type = how to read it."** This single reframing
  demystifies pointers, arrays, and struct layout before you even get there.
- **Use `size_t` for sizes and indices.** It's the unsigned type `sizeof` returns and is
  guaranteed wide enough to index any object — the right type for lengths and loops over
  memory.
- **When in doubt, print `sizeof` and the bytes.** The demo above settles arguments about
  width and layout in seconds; never guess what the compiler chose.

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/endianness|Endianness]] · [[lowlevel/machine-model/registers-and-the-isa|Registers & the ISA]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 2 — information representation: bits, bytes, words, byte ordering, and integer encodings; the spine for this note. https://csapp.cs.cmu.edu/
- **Jens Gustedt — *Modern C*** — C's integer types, `sizeof`, `size_t`, and why fixed-width types exist. https://gustedt.gitlabpages.inria.fr/modern-c/
- **cppreference — Fixed-width integer types (`<stdint.h>`)** — `int32_t`, `uint64_t`, `uintptr_t`, and their guarantees. https://en.cppreference.com/w/c/types/integer
- **cppreference — `sizeof` operator and `CHAR_BIT`** — what `sizeof` measures and the bits-per-byte macro. https://en.cppreference.com/w/c/language/sizeof
- **ISO/IEC 9899 (C standard), §5.2.4.2 & §6.2.5** — the minimum integer ranges and the implementation-defined nature of type sizes. https://www.open-std.org/jtc1/sc22/wg14/
