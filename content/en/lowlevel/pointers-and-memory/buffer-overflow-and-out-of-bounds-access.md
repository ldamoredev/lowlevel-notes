---
title: "Classic bug: buffer overflow & out-of-bounds access"
description: C pointers carry addresses, not bounds. Buffer overflows and out-of-bounds reads happen when code forgets which object, array, and length a pointer is valid for.
tags: [buffer-overflow, out-of-bounds, arrays, bounds, undefined-behavior]
order: 13
updated: 2026-06-30
---
# Classic bug: buffer overflow & out-of-bounds access

C lets you compute addresses inside an array, but it does not attach a runtime bound to a
pointer. The program must remember which object the pointer came from and how many
elements are valid. A buffer overflow writes outside that range; an out-of-bounds read
loads outside that range. Both bugs start as bad arithmetic and end as undefined behavior.

> The reset: a pointer plus a length is a different abstraction from a pointer alone. Most
> robust C APIs eventually rediscover slices.

## How it really works

For an array of `N` elements, C pointer arithmetic is defined within the array object and
one element past the end. The one-past pointer is useful for loop termination and range
comparison. It is not a valid element to dereference.

```c
int a[3] = {10, 20, 30};
int *begin = &a[0];
int *end = &a[3];      /* one past, valid as a boundary */
int bad = *end;        /* undefined behavior */
```

Array indexing is only syntax over pointer arithmetic: `a[i]` means `*(a + i)`. If `i` is
wrong, the dereference is wrong. The language does not check it for you.

The danger gets sharper with byte buffers and C strings. A string needs room for its
terminating `'\0'`. Copying `strlen(src)` bytes copies the characters but not the
terminator. Copying `strlen(src) + 1` bytes is correct only if the destination has that
many bytes available. `sizeof dst` works when `dst` is a real array in the same scope; it
does not work after the array has decayed to a pointer parameter.

Out-of-bounds writes can corrupt adjacent stack locals, heap allocator metadata, another
field in the same struct, or control data depending on layout and platform. Out-of-bounds
reads can leak data, fold undefined values into decisions, or appear harmless until an
optimizer relies on the assumption that undefined behavior does not happen.

The local repair is simple and repetitive: carry the bound, check before arithmetic, and
make the check use the same unit as the operation. Element counts check element indexes.
Byte counts check byte copies. String copies include the terminator.

## Executable artifact: slices and checked string copy

The demo lives in `examples/pointers-and-memory/buffer-overflow-and-out-of-bounds-access/demo.c`.

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Real output:

```
slice[2]              = 30
slice[3] status       = out of bounds
copy short            = atlas
copy long status      = too long
```

The `Slice` type makes the bound travel with the pointer. `slice_get` checks element
indexes before dereferencing. `copy_c_string` checks bytes, including the null terminator,
before calling `memcpy`. None of this is glamorous; that is why it works.

## Failure modes & trade-offs

- **Losing the bound at API boundaries.** `void f(int a[])` receives an `int *`, not the
  array length.
- **Off-by-one at the terminator.** Strings need one more byte than their visible
  character count.
- **Mixed units.** Comparing an element index to a byte size or copying elements with a
  byte count creates quiet gaps.
- **Integer overflow before allocation or copy.** `count * sizeof *p` can wrap if `count`
  is attacker-controlled or simply huge.
- **Trusting sentinel data too much.** A missing `'\0'` turns string code into an
  out-of-bounds read.
- **Assuming reads are safe.** Out-of-bounds reads can be exploitable and can still give
  the optimizer permission to transform code unexpectedly.

## In practice

- **Pass pointer and count together.** Prefer `struct Slice { T *data; size_t count; }`
  when a pair crosses several calls.
- **Validate before deriving pointers.** Check `index < count` before computing and using
  `data + index`.
- **Separate byte APIs from element APIs.** Name parameters `byte_count` or `element_count`
  so call sites expose unit mistakes.
- **Prefer bounded formatting.** `snprintf` and explicit `memcpy` with checked sizes beat
  unbounded string functions.
- **Compile with warnings and run sanitizers.** `-Wall -Wextra` catches shape problems;
  ASan catches many runtime bounds violations.
- **Design hostile-input paths as parsers.** Files, sockets, and firmware blobs deserve
  length-first parsing, not struct casts and hopeful indexing.

**Connects to:** [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Pointer arithmetic & stride]] · [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alignment & padding]] · [[lowlevel/pointers-and-memory/use-after-free-and-double-free|Use-after-free & double-free]] · [[lowlevel/pointers-and-memory/void-star-and-type-erasure|`void*` and type erasure]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **ISO WG14 N1570 — C11 draft** — pointer arithmetic, array subscripting, and undefined behavior around invalid access. https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- **cppreference — Array declaration** — practical reference for array objects, decay, and array parameter behavior. https://en.cppreference.com/w/c/language/array
- **cppreference — `memcpy`** — documents the byte-copy primitive used once bounds are checked. https://en.cppreference.com/w/c/string/byte/memcpy
- **Clang AddressSanitizer documentation** — runtime detection of heap, stack, and global buffer overflows. https://clang.llvm.org/docs/AddressSanitizer.html
- **MITRE CWE-120: Classic Buffer Overflow** — security taxonomy for unchecked writes past a buffer. https://cwe.mitre.org/data/definitions/120.html
- **MITRE CWE-125: Out-of-bounds Read** — security taxonomy for reading outside valid memory. https://cwe.mitre.org/data/definitions/125.html
