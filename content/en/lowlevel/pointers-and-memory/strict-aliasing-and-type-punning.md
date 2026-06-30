---
title: "Strict aliasing & type punning"
description: Strict aliasing is the optimizer's permission to assume that unrelated pointer types do not name the same object. Type punning breaks when you reinterpret storage through the wrong lvalue instead of copying bytes deliberately.
tags: [strict-aliasing, type-punning, effective-type, undefined-behavior, memcpy]
order: 11
updated: 2026-06-30
---
# Strict aliasing & type punning

The C compiler does not only translate your loads and stores. It reasons about which
loads and stores can refer to the same object. Strict aliasing is the rule that lets it
assume pointers to unrelated types usually do not alias, and type punning is the act of
looking at one object's bytes as if they were another type. The safe mental model: values
have types, objects have lifetimes and representation bytes, and `memcpy` is the boring
portable bridge between typed values and raw bytes.

> The reset: a `void*` or a cast does not erase the rules of the object underneath. It only
> changes the type of the expression you are about to use.

## How it really works

Every C object has an **object representation**: the bytes that store its value. Character
types can inspect those bytes. That is why `unsigned char *` is the right tool for byte
dumps, serialization primitives, checksums, and copying opaque payloads.

The stricter rule is about accessing an object through an lvalue. The C standard says an
object's stored value may only be accessed through certain compatible types, qualified
versions of those types, suitable aggregate/union types, or a character type. If you write
a `float` object and later read the same storage through an `int *`, the compiler is
allowed to treat that as impossible in strictly conforming C. At optimization levels where
strict aliasing is enabled, that assumption can move, combine, or delete memory operations
in ways that make the program look "miscompiled." The bug is earlier: the program gave
the optimizer a false contract.

This matters even when the addresses are numerically equal:

```c
float f = 1.0f;
int *p = (int *)&f;
printf("%x\n", *p);        /* not portable C */
```

The cast produces an `int *`, but it does not create an `int` object at that address. It
also does not promise that the bit pattern of a `float` is a valid `int` object read
through an `int` lvalue. On a common IEEE-754 machine you might get the bits you expected
in a debug build. That is not the rule the optimizer must preserve.

`memcpy` is different. It copies bytes from one actual object into another actual object:

```c
float f = 1.0f;
uint32_t bits;
memcpy(&bits, &f, sizeof bits);
```

Now `bits` is a real `uint32_t` object, and reading `bits` uses the correct lvalue type.
The bytes came from the float object representation, but the access is no longer an
incompatible typed access to the float object. Compilers understand this pattern and
usually optimize the copy away.

Union punning is the gray zone people reach for in C. It is common in systems code, and C
has union-specific wording, but it still exposes representation details and can produce
trap or implementation-specific results for some types. Use a union when the union is the
data model. Use `memcpy` when the operation is "copy these bytes into an object of another
type."

## Executable artifact: byte copy, not aliasing by cast

The demo lives in `examples/pointers-and-memory/strict-aliasing-and-type-punning/demo.c`.

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Real output:

```
float 1.0 bits        = 0x3f800000
restored float        = 1.0
object bytes          = 22 11 44 33
pair copied as u32    = 0x33441122
```

The `float` bits are copied with `memcpy`, then copied back into a real `float` object.
The struct byte dump uses `unsigned char`, which is allowed to inspect object
representation. The final `uint32_t` is also created by byte copy. On this little-endian
machine, the bytes print least significant byte first; that byte order is an observation,
not a portable serialization format.

## Failure modes & trade-offs

- **Casting through a different object type.** `T *` casts can silence diagnostics while
  keeping the same undefined behavior.
- **Assuming `void*` is magic.** `void*` is untyped transport for an address. The access
  after the cast must still respect the real object's type and lifetime.
- **Packing protocols with struct casts.** Network packets, disk blocks, and MMIO layouts
  often need byte parsing, endian conversion, and alignment handling, not a cast to a C
  struct pointer.
- **Depending on debug behavior.** `-O0` often hides strict-aliasing bugs because the
  optimizer is not exploiting its assumptions yet.
- **Turning off strict aliasing globally.** `-fno-strict-aliasing` can be a practical
  migration switch for old code, but it also removes optimization room from correct code.
- **Forgetting alignment.** Even if aliasing were allowed, dereferencing a misaligned
  pointer can be undefined or slow on real machines.

## In practice

- **Use `memcpy` for bit reinterpretation.** It says exactly what you mean and optimizes
  well in modern compilers.
- **Use `unsigned char *` for byte inspection.** Do not generalize that exemption to
  arbitrary pointer types.
- **Keep storage ownership and typed views close.** If an API hands out raw bytes, document
  which function creates the typed object and when that object's lifetime starts.
- **Serialize explicitly.** For files and networks, write endian-aware encode/decode
  functions instead of dumping structs.
- **Treat strict-aliasing warnings as smoke, not coverage.** The compiler cannot prove
  every bad alias. Code review still has to know the rule.
- **Reach for `-fno-strict-aliasing` only as a boundary decision.** It may be reasonable
  for one legacy library; it should not be a substitute for understanding object access.

**Connects to:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is]] · [[lowlevel/pointers-and-memory/void-star-and-type-erasure|`void*` and type erasure]] · [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alignment & padding]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Pointer arithmetic & stride]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **ISO WG14 N1570 — C11 draft** — object representation, effective type, aliasing, and byte access rules in standard wording. https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- **cppreference — Object model** — compact reference for effective type, strict aliasing, alignment, and object representation. https://en.cppreference.com/w/c/language/object
- **cppreference — `memcpy`** — documents byte copying as the portable primitive for moving object representations. https://en.cppreference.com/w/c/string/byte/memcpy
- **GCC manual — Optimize Options** — explains `-fstrict-aliasing` and the optimizer assumption exposed by the flag. https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
- **Jens Gustedt — *Modern C*** — practical C treatment of object representation, aliasing, and portable low-level idioms. https://gustedt.gitlabpages.inria.fr/modern-c/
