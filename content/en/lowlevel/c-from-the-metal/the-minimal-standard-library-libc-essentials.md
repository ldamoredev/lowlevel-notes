---
title: "The minimal standard library (libc essentials)"
description: The C standard library is a small hosted toolbox, not the language itself. Headers declare functions and types for I/O, memory, strings, allocation, parsing, diagnostics, and utilities, often backed by the OS through libc.
tags: [c, libc, standard-library, errno, malloc, stdio]
order: 11
updated: 2026-06-29
---
# The minimal standard library (libc essentials)

C the language is small, but useful C programs usually live in a **hosted implementation**
with the standard library available. The library gives you declarations for I/O, allocation,
string and byte operations, parsing, math, diagnostics, time, and utility algorithms. It is
not magic and it is not the OS. It is a specified interface, usually implemented by libc,
that may call into the kernel underneath.

> The reset: C gives you expressions, objects, and control flow. libc gives you `printf`,
> `malloc`, `memmove`, `strtol`, `qsort`, `errno`, and friends. Know which contract you are
> relying on.

## Hosted vs freestanding

The C standard distinguishes **hosted** and **freestanding** implementations. Hosted is
what you normally use on Linux, macOS, BSD, and Windows: `main` exists, the full standard
library is expected, and headers such as `<stdio.h>`, `<stdlib.h>`, and `<string.h>` are
available. Freestanding is what kernels, bootloaders, and embedded firmware often start
from: only a small subset is guaranteed.

That matters for this atlas because the OS-from-scratch path is freestanding. Early kernel
code cannot assume `printf`, `malloc`, files, processes, or an operating system underneath.
On a hosted program, libc sits between you and OS services such as files, terminals, memory
mapping, and process exit.

## How it really works

Headers declare the library surface; the definitions come from the linked C library. That
connects directly to
[[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|translation units and linkage]]:
`#include <stdlib.h>` lets the compiler type-check `malloc`, but linking and loading supply
the actual implementation.

The core headers you should recognize early:

| Header | What to know first |
|---|---|
| `<stddef.h>` | `size_t`, `ptrdiff_t`, `NULL`, `offsetof` |
| `<stdint.h>` | fixed-width integer types such as `uint32_t` |
| `<stdbool.h>` | `bool`, `true`, `false` before C23 style changes |
| `<stdio.h>` | `printf`, `fprintf`, `snprintf`, `FILE` |
| `<stdlib.h>` | `malloc`, `free`, `strtol`, `qsort`, `exit` |
| `<string.h>` | `memcpy`, `memmove`, `memset`, `strlen`, `strcmp` |
| `<errno.h>` | thread-local-ish error indicator used by many APIs |
| `<assert.h>` | `assert` for debug-time contract checks |
| `<ctype.h>` | byte classification such as `isdigit`, with unsigned-char caveats |
| `<limits.h>` | implementation limits such as `CHAR_BIT`, `INT_MAX` |

Several library APIs encode old C trade-offs. `malloc` returns raw storage; you own the
lifetime. `strlen` scans for NUL; it does not know capacity. `strtol` reports errors by
using both an end pointer and `errno`. `memcpy` requires non-overlapping ranges; `memmove`
handles overlap. `qsort` erases types through `void *`, so the comparator must restore the
right type.

## Executable artifact: a tiny libc tour

The demo lives in
`examples/c-from-the-metal/the-minimal-standard-library-libc-essentials/demo.c`. It uses
`strtol`, `errno`, `malloc`, `qsort`, `memmove`, and `free`.

```c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_ints(const void *left, const void *right) {
    int a = *(const int *)left;
    int b = *(const int *)right;

    return (a > b) - (a < b);
}

static void print_ints(const int *values, size_t count) {
    for (size_t i = 0; i < count; i++) {
        printf("%s%d", i == 0 ? "" : " ", values[i]);
    }

    printf("\n");
}

int main(void) {
    char *end = NULL;
    errno = 0;
    long parsed = strtol("42kb", &end, 10);

    printf("strtol value          = %ld\n", parsed);
    printf("strtol rest           = %s\n", end);

    errno = 0;
    (void)strtol("999999999999999999999999999999", NULL, 10);
    printf("strtol ERANGE         = %s\n", errno == ERANGE ? "yes" : "no");

    int *values = malloc(4 * sizeof *values);
    if (values == NULL) {
        perror("malloc");
        return 1;
    }

    values[0] = 4;
    values[1] = 1;
    values[2] = 3;
    values[3] = 2;

    qsort(values, 4, sizeof values[0], compare_ints);
    printf("sorted ints           = ");
    print_ints(values, 4);

    /* memmove accepts overlapping ranges; memcpy does not promise that. */
    char text[8] = "abcde";
    memmove(text + 2, text, 3);
    printf("memmove text          = %s\n", text);

    free(values);
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
strtol value          = 42
strtol rest           = kb
strtol ERANGE         = yes
sorted ints           = 1 2 3 4
memmove text          = ababc
```

The interesting part is the shape of the contracts. `strtol` gives you both the parsed
number and the unparsed suffix. Range errors use `errno`, so the code clears it before the
call. `malloc` can fail and must be paired with `free`. `qsort` is generic by erasing types
to `void *`. `memmove` is the right function for overlapping byte ranges.

## Failure modes & trade-offs

- **Forgetting which APIs need capacity.** `snprintf`, `fgets`, and `memmove` take sizes;
  old APIs such as `strcpy` do not know destination capacity.
- **Misusing `errno`.** `errno` is meaningful only when the specific function says it is.
  Set it to zero before calls such as `strtol` when you need to distinguish "no error" from
  a stale value.
- **Ignoring allocation failure.** Hosted systems overcommit sometimes, but `malloc` still
  returns `NULL` by contract. Critical code handles that path.
- **Confusing byte functions and string functions.** `memcpy` copies bytes with a length.
  `strcpy` copies until NUL. Binary data needs byte APIs.
- **Assuming libc exists in freestanding code.** Kernels and bootloaders either avoid libc,
  provide their own tiny subset, or link a freestanding-compatible library.

## In practice

- **Learn the contracts, not just the names.** For each function, ask: who owns storage,
  where is the length, how are errors reported, and what preconditions are undefined if
  broken?
- **Prefer size-aware APIs.** Use `snprintf`, `fgets`, `memmove`, and explicit
  pointer-plus-length interfaces.
- **Wrap rough APIs locally.** A project helper for parsing, allocation, or string copying
  can encode one house rule and reduce repeated mistakes.
- **Keep `stdlib` and syscalls conceptually separate.** `fopen` is libc; `open` is POSIX.
  `printf` is libc; `write` is a syscall wrapper. Different layers, different guarantees.
- **In freestanding work, build your own floor deliberately.** Decide which tiny subset of
  memory, string, and formatting helpers your kernel owns.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/strings-are-just-char-pointers|Strings are just `char*`]] · [[lowlevel/c-from-the-metal/arrays-and-array-to-pointer-decay|Arrays and array-to-pointer decay]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declarations, and linkage]] · [[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]] · [[lowlevel/systems-programming/index|Systems Programming]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for hosted vs freestanding implementations and the standard library contracts. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — C standard library headers** — compact index of standard headers and the declarations each one exposes. https://en.cppreference.com/w/c/header
- **cppreference — Null-terminated byte strings** — reference for `strlen`, `strcmp`, `strcpy`, and why C string APIs depend on a NUL terminator. https://en.cppreference.com/w/c/string/byte
- **cppreference — `strtol`** — parsing contract, end pointer, base handling, `errno`, and range errors. https://en.cppreference.com/w/c/string/byte/strtol
- **cppreference — `qsort`** — generic sorting through `void *`, element size, and comparator contract. https://en.cppreference.com/w/c/algorithm/qsort
- **cppreference — `memmove`** — byte-copying contract for overlapping ranges, contrasted with `memcpy`. https://en.cppreference.com/w/c/string/byte/memmove
