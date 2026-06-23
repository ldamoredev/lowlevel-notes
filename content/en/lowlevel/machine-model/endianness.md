---
title: "Endianness"
description: The order a multi-byte value's bytes sit in memory — little-endian (x86-64, ARM) stores the least-significant byte first, big-endian (network order) the most-significant. Invisible within one machine, it bites the instant bytes cross a file, a socket, or a different architecture.
tags: [machine-model, endianness, byte-order, serialization, networking]
order: 9
updated: 2026-06-22
---
# Endianness

A 32-bit integer is four bytes — but in which *order* are those four bytes laid out in
memory? That choice is **endianness**, and there are two answers. **Little-endian** stores
the least-significant byte at the lowest address ("little end first"); **big-endian**
stores the most-significant byte first. x86-64 and (in practice) ARM64 are little-endian;
network protocols and some older architectures are big-endian. Inside a single machine the
choice is completely invisible — arithmetic and comparisons operate on *values*, not bytes.
It becomes real, and bites hard, the moment raw bytes cross a boundary: a file, a socket,
or a CPU of a different endianness.

> The reset: the *value* `0x0A0B0C0D` is unambiguous; its *byte layout* is not. The number
> you compute with and the bytes you write to disk or the wire are not the same thing, and
> conflating them corrupts data silently.

## Two orderings

Store the 32-bit value `0x0A0B0C0D` (MSB `0A`, LSB `0D`) and read memory from the lowest
address upward:

| Address | Little-endian | Big-endian |
|---|---|---|
| base+0 | `0D` (LSB) | `0A` (MSB) |
| base+1 | `0C` | `0B` |
| base+2 | `0B` | `0C` |
| base+3 | `0A` (MSB) | `0D` (LSB) |

Little-endian puts the *little end* (least-significant byte) first. It feels backwards when
you read a hex dump, but it has a neat property: a value's low byte is at the same address
whether you read it as an 8-, 16-, 32-, or 64-bit type, so narrowing casts need no address
math. Big-endian matches how we write numbers left-to-right, which is why it was chosen as
**network byte order** (RFC 1700) — on the wire, the most-significant byte goes first.

The two families:

- **Little-endian:** x86, x86-64, ARM/ARM64 (default), RISC-V. The overwhelming majority of
  what you'll run on.
- **Big-endian:** network protocols (TCP/IP headers), many file formats (PNG, JPEG, Java
  class files), older SPARC/PowerPC/m68k, and most "this format is big-endian" specs.

## Why it's usually invisible

You can write C for years without thinking about endianness, because the CPU is
*self-consistent*: it stores and loads multi-byte values in the same order every time, so
`x + 1`, `x == y`, and `x << 8` all produce the right *value* regardless of byte layout. The
order only becomes observable when you stop treating memory as typed values and start
treating it as raw bytes:

- casting through `unsigned char*` to inspect individual bytes,
- `fwrite`/`fread` of a struct or integer to a binary file,
- `send`/`recv` of raw integers over a socket,
- `memcpy`-ing between a typed value and a byte buffer.

Every one of those exposes the in-memory byte order — and if the reader assumes a different
order than the writer, the bytes are silently misinterpreted.

## When it bites

- **Serialization across machines.** Write `uint32_t 0x0A0B0C0D` to a file on a
  little-endian box (`0D 0C 0B 0A` on disk) and read it back on a big-endian box, and you
  get `0x0D0C0B0A` — a different number, no error reported.
- **Network protocols.** TCP/UDP/IP fields are big-endian. Send a port or length as a raw
  host-order `int` from x86 and the peer reads garbage. Always convert.
- **Binary file formats.** Every format fixes an endianness in its spec; parsing one means
  reading its bytes in *its* order, not your host's.
- **Type punning.** Reinterpreting a byte buffer as an `int` (or vice versa) bakes in the
  host's endianness — fine locally, wrong the moment the data travels.

## See it

```c
// endian.c — detect host order, dump bytes, and convert to network order.
// gcc -O0 -Wall -Wextra endian.c -o endian && ./endian
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>   // htonl, ntohl

static void dump(const char *label, const void *p, size_t n) {
    const unsigned char *b = p;
    printf("%-24s", label);
    for (size_t i = 0; i < n; i++) printf("%02x ", b[i]);
    putchar('\n');
}

int main(void) {
    uint32_t x = 0x0A0B0C0D;

    unsigned char first = *(unsigned char*)&x;          // first byte in memory
    printf("value: 0x%08X\n", x);
    printf("this machine is: %s\n\n",
           first == 0x0D ? "little-endian (LSB first)" : "big-endian (MSB first)");

    dump("bytes in memory:", &x, sizeof x);

    uint32_t net = htonl(x);                            // host -> network (big-endian)
    dump("after htonl (network):", &net, sizeof net);
    printf("ntohl(htonl(x)) == x ? %s\n", ntohl(net) == x ? "yes" : "no");

    uint32_t s = ((x & 0xFF000000u) >> 24) | ((x & 0x00FF0000u) >> 8) |   // manual bswap
                 ((x & 0x0000FF00u) <<  8) | ((x & 0x000000FFu) << 24);
    printf("manual bswap: 0x%08X -> 0x%08X\n", x, s);
    return 0;
}
```

On a little-endian machine the value `0x0A0B0C0D` sits in memory as `0d 0c 0b 0a`; `htonl`
reorders it to the big-endian `0a 0b 0c 0d` for the wire, and `ntohl` reverses it exactly.
The manual swap shows what the hardware `bswap` instruction (and `__builtin_bswap32`) does
in one op. Detecting endianness by reading the first byte of a known value is the classic
runtime check.

## Doing it right

- **Never serialize raw multi-byte values across a boundary.** Convert to a defined order
  first. For networking, use `htonl`/`htons`/`ntohl`/`ntohs` (host↔network, i.e. big-endian).
- **Or serialize byte-by-byte with explicit shifts** — `buf[0] = x >> 24; buf[1] = x >> 16;
  …` This is *endianness-independent*: it produces the same bytes on any host, which is the
  most portable approach for file/wire formats.
- **Use the format's declared endianness, not your host's.** When parsing, read each field
  in the order its spec mandates; don't assume the host matches.
- **Helpers exist.** `__builtin_bswap16/32/64` (GCC/Clang) and Linux/BSD `<endian.h>`
  (`htole32`, `htobe32`, …) do conversions without hand-rolled masks.

## Failure modes & trade-offs

- **`htonl` on a struct does nothing useful.** Conversions are per-field, per-integer; you
  can't byte-swap a whole struct in one call. Serialize field by field.
- **Forgetting it's a no-op on big-endian.** `htonl` is identity on a big-endian host, so a
  bug that only swaps on little-endian "works" on one machine and fails on the other. Test
  the conversion path, not just the local round-trip.
- **`char` signedness ≠ endianness.** Two separate portability axes; don't conflate "is
  `char` signed?" with byte order.
- **Endianness ≠ bit order.** Within a byte, bit numbering is a separate convention; almost
  everything you touch is byte-granular, so reason in bytes.

## In practice

- **Local-only data: ignore endianness.** A value that never leaves the process can stay in
  host order; converting needlessly just adds bugs.
- **Anything crossing a wire, a file, or an arch boundary: pick an order and convert.**
  Network → big-endian via `htonl`/`ntohl`; custom formats → document the order and
  serialize with explicit shifts.
- **x86-64 and ARM64 are little-endian** — your day-to-day — **but the network is
  big-endian.** That single mismatch is where most endianness bugs live.
- **When bytes look reversed in a hex dump, suspect endianness first.** `0D0C0B0A` where you
  expected `0A0B0C0D` is the tell.

**Connects to:** [[lowlevel/machine-model/index|Machine Model]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words & addresses]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Two's complement & integers]] · [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/systems-programming/index|Systems Programming]]

## Sources

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 2 — byte ordering, big/little-endian, and showing the bytes of any object; the spine for this note. https://csapp.cs.cmu.edu/
- **Beej's Guide to Network Programming — "Byte Order"** — host vs network order and the `htonl`/`ntohl` family, with examples. https://beej.us/guide/bgnet/html/#byte-order
- **RFC 1700 / RFC 791 — network byte order** — the specification that network protocols are big-endian. https://www.rfc-editor.org/rfc/rfc791
- **cppreference — fixed-width integers & `<stdint.h>`** — the exact-width types you serialize with. https://en.cppreference.com/w/c/types/integer
- **Linux `endian.h` man page — `htole32`/`htobe32` etc.** — standard byte-order conversion helpers beyond the network family. https://man7.org/linux/man-pages/man3/endian.3.html
