---
title: "Strings are just `char*` (and the consequences)"
description: A C string is not a managed string object. It is a sequence of char bytes ending at the first NUL byte, usually reached through a char pointer that carries no length or capacity.
tags: [c, strings, char-pointer, null-terminated, buffers]
order: 9
updated: 2026-06-29
---
# Strings are just `char*` (and the consequences)

C does not have a built-in string object with length, capacity, encoding, and bounds.
What C programmers call a "string" is a **null-terminated byte string**: a sequence of
`char` bytes whose end is marked by the first `'\0'` byte. The value most APIs receive is
just `char *` or `const char *`, a pointer to the first byte. That pointer does not know
how many bytes are valid, how much capacity remains, whether the storage is writable, or
whether a terminator exists at all.

> The reset: `char *` is an address, not a string object. The string exists only if the
> bytes reachable from that address eventually contain a NUL terminator.

## The convention: bytes until NUL

This is a real array object:

```c
char word[] = "metal";
```

The initializer copies six bytes into `word`: `m`, `e`, `t`, `a`, `l`, and `'\0'`.
`sizeof word` is therefore 6. `strlen(word)` is 5 because `strlen` counts bytes before the
first NUL and does not include the terminator.

This is different:

```c
const char *literal = "metal";
```

Here `"metal"` is a string literal with array type in storage the program must treat as
read-only. In most expressions it decays to a pointer to its first character, and that
pointer is stored in `literal`. `sizeof literal` is the size of the pointer, not the size of
the literal. `sizeof "metal"` is still 6 because `sizeof` is one of the contexts where an
array does not decay.

That ties directly to [[lowlevel/c-from-the-metal/arrays-and-array-to-pointer-decay|array-to-pointer decay]]:
arrays can hold string bytes, but the pointer passed to a function has lost the array
bound.

## How it really works

The standard library's string functions operate on this convention. `strlen` walks forward
until it sees `'\0'`. `printf("%s", p)` prints bytes until it sees `'\0'`. `strcpy` copies
through the terminator. None of these functions can infer the destination buffer's
capacity from a naked pointer.

That makes four facts non-negotiable:

| Thing | What C knows |
|---|---|
| `char buf[16]` in its own scope | 16 bytes of storage |
| `char *p = buf` | address of the first byte |
| `const char *s = "hi"` | pointer to read-only literal bytes by convention |
| `strlen(s)` | count by scanning until first `'\0'` |

`char` also means byte here, not Unicode character. A UTF-8 string uses multiple `char`
bytes for some user-visible characters. `strlen` counts bytes before NUL, not graphemes,
code points, or display columns. Embedded NUL bytes end the C string early, which is why
binary data needs pointer-plus-length APIs such as `memcpy`.

String literals deserve special caution. In C, their type is an array of `char`, but
attempting to modify one has undefined behavior. Use `const char *` for pointers to
literals so the type system helps keep read-only storage read-only.

## Executable artifact: pointer, array, terminator

The demo lives in `examples/c-from-the-metal/strings-are-just-char-pointers/demo.c`. It
prints the difference between array size, pointer size, string length, and raw bytes.

```c
#include <stdio.h>
#include <string.h>

static void inspect_parameter(const char *text) {
    printf("sizeof parameter      = %zu bytes\n", sizeof text);
    printf("strlen parameter      = %zu chars\n", strlen(text));
}

static void print_bytes(const char *label, const char *bytes, size_t count) {
    printf("%s", label);

    for (size_t i = 0; i < count; i++) {
        printf(" %02X", (unsigned char)bytes[i]);
    }

    printf("\n");
}

int main(void) {
    char word[] = "metal";
    const char *literal = "metal";
    char copied[8] = {0};
    char not_a_string[4] = {'b', 'u', 'g', '!'};
    char small[6] = {0};

    memcpy(copied, "low", 4);

    printf("sizeof word array     = %zu bytes\n", sizeof word);
    printf("strlen word           = %zu chars\n", strlen(word));
    printf("sizeof \"metal\"        = %zu bytes\n", sizeof "metal");
    printf("sizeof literal ptr    = %zu bytes\n", sizeof literal);
    printf("strlen literal        = %zu chars\n", strlen(literal));

    inspect_parameter(word);

    /* The array is writable; the string literal must be treated as read-only. */
    word[0] = 'M';
    printf("modified array        = %s\n", word);

    /* strlen walks until NUL; this is not a C string. */
    print_bytes("not_a_string bytes   =", not_a_string, sizeof not_a_string);

    int needed = snprintf(small, sizeof small, "%s", "systems");
    printf("snprintf needed       = %d chars\n", needed);
    printf("small buffer          = %s\n", small);

    print_bytes("copied bytes         =", copied, sizeof copied);

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
sizeof word array     = 6 bytes
strlen word           = 5 chars
sizeof "metal"        = 6 bytes
sizeof literal ptr    = 8 bytes
strlen literal        = 5 chars
sizeof parameter      = 8 bytes
strlen parameter      = 5 chars
modified array        = Metal
not_a_string bytes   = 62 75 67 21
snprintf needed       = 7 chars
small buffer          = syste
copied bytes         = 6C 6F 77 00 00 00 00 00
```

The writable array has storage for the terminator, so `sizeof word` is 6 and `strlen` is 5.
The pointer to the literal is 8 bytes on this platform because it is just a pointer.
Passing `word` to `inspect_parameter` loses the array bound. `not_a_string` deliberately
has no NUL terminator, so the program prints its bytes and never calls `strlen` on it.
`snprintf` writes a terminated truncated string and returns the length it would have needed:
`"systems"` is 7 bytes before the terminator, but `small` can hold only five visible bytes
plus NUL.

## Failure modes & trade-offs

- **Missing terminator.** Calling `strlen`, `printf("%s")`, or `strcpy` on bytes without a
  reachable NUL makes the function read past the object. That is undefined behavior.
- **Buffer overflow.** A destination `char *` does not reveal capacity. Unbounded copies can
  overwrite adjacent objects even when the source is a valid string.
- **Literal mutation.** `char *p = "hello"; p[0] = 'H';` compiles in some modes and has
  undefined behavior. Use `const char *`.
- **`sizeof` confusion.** `sizeof buf` works only while `buf` is an array in the current
  scope. Once it decays to `char *`, `sizeof` gives pointer size.
- **Embedded NULs.** C string APIs stop early. Network frames, files, and compressed data
  are bytes with lengths, not strings.
- **`strncpy` is not a safe `strcpy`.** It may leave the destination unterminated when the
  source is too long, and it pads with zeros when the source is short. Prefer explicit
  capacity handling such as `snprintf`, `memcpy` with known lengths, or a project-local
  helper with clear semantics.

## In practice

- **Carry capacity with mutable buffers.** APIs that write should receive `(char *buf,
  size_t cap)` and should define whether they always NUL-terminate.
- **Carry length with byte data.** APIs that process arbitrary bytes should receive
  `(const unsigned char *data, size_t len)` or a slice struct, not pretend the data is a C
  string.
- **Use `const char *` for borrowed text.** It documents that the callee must not mutate the
  pointed bytes and protects string literals.
- **Check `snprintf` results.** A return value greater than or equal to capacity means the
  output was truncated.
- **Reserve one byte for NUL.** A buffer of size `N` can hold at most `N - 1` visible bytes
  as a C string.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/arrays-and-array-to-pointer-decay|Arrays and array-to-pointer decay]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|The C type system is weak]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: the contract]] · [[lowlevel/c-from-the-metal/structs-unions-and-bitfields|Structs, unions, and bitfields]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words & addresses]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for string literals, arrays of character type, library string functions, object bounds, and undefined behavior. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — String literal** — exact rules for string literal storage, terminators, types, concatenation, and why modification is undefined. https://en.cppreference.com/c/language/string_literal
- **cppreference — Null-terminated byte strings** — reference for C's string model and the standard library functions that consume NUL-terminated byte strings. https://en.cppreference.com/w/c/string/byte
- **cppreference — `strlen`** — precise contract: count characters before the first null character, with behavior depending on a valid null-terminated byte string. https://en.cppreference.com/w/c/string/byte/strlen
- **cppreference — `snprintf`** — return value, truncation behavior, and capacity-aware formatting contract. https://en.cppreference.com/w/c/io/fprintf
- **Beej's Guide to C — Strings** — approachable explanation of arrays, pointers, NUL terminators, and why string length is a convention. https://beej.us/guide/bgc/html/split/strings.html
