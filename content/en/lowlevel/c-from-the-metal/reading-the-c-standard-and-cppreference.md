---
title: "Reading the C standard and cppreference"
description: The C standard is the contract; cppreference is a map. Learn how to read definitions, constraints, semantics, undefined behavior, implementation-defined behavior, version markers, and compiler extensions without treating examples as law.
tags: [c, standard, cppreference, wg14, reference]
order: 13
updated: 2026-06-29
---
# Reading the C standard and cppreference

Low-level C work eventually reaches the question "what does C actually promise here?" The
answer lives in the C standard, but the standard is dense, normative, and not written as a
tutorial. cppreference is the fast map: searchable, cross-linked, versioned, and practical.
Use cppreference to orient yourself; use the standard when the exact contract matters.

> The reset: cppreference tells you where the rule is and how people usually talk about it.
> The standard is the rule. Compiler behavior is evidence about one implementation, not the
> language.

## What you are reading

The C standard is produced by WG14 and published by ISO. Official final standards are not
always freely downloadable, but WG14 working drafts are widely used for study. They are not
the same legal artifact as the final standard, but they are close enough to teach the shape
of the language: terms, constraints, semantics, library contracts, undefined behavior, and
implementation-defined choices.

cppreference is a secondary reference. It is excellent for daily work because it links
concepts together, marks C99/C11/C17/C23 changes, summarizes constraints, and includes
examples. It can still have omissions or wording that is less precise than the standard.
When the bug or API boundary is high stakes, follow the references back to normative text
and compiler documentation.

## How it really works

Read C reference material with a few words loaded:

| Word | Meaning when reading |
|---|---|
| shall | requirement; violating a "shall" constraint often requires a diagnostic or makes behavior undefined |
| constraint | rule the implementation must diagnose when violated in a translation unit |
| semantics | what a valid construct means |
| undefined behavior | no requirements after the program violates the rule |
| unspecified behavior | implementation may choose among valid possibilities without documenting which |
| implementation-defined behavior | implementation must choose and document its choice |
| footnote | helpful context, usually not normative |
| example | illustration, not the whole rule |

On cppreference, first check which page you are on: language, library, header, or compiler
support. Then check version markers. A rule that says "since C23" is not a rule for a
`-std=c17` build. Read the "Notes" and "Defect reports" sections, but do not stop there if
you are designing a portability boundary.

Compiler extensions are a separate layer. GCC and Clang support useful extensions such as
statement expressions, attributes, builtins, and extra warnings. They are not portable C
merely because your compiler accepts them. Decide deliberately whether a file is ISO C,
GNU C, Clang-specific C, or freestanding target-specific C.

## Executable artifact: ask the implementation

The demo lives in `examples/c-from-the-metal/reading-the-c-standard-and-cppreference/demo.c`.
It prints facts that the standard describes but the implementation chooses or instantiates.

```c
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

int main(void) {
#ifdef __STDC_VERSION__
    printf("__STDC_VERSION__      = %ld\n", (long)__STDC_VERSION__);
#else
    printf("__STDC_VERSION__      = pre-C90\n");
#endif

    printf("CHAR_BIT              = %d\n", CHAR_BIT);
    printf("INT_MAX               = %d\n", INT_MAX);
    printf("sizeof(size_t)        = %zu bytes\n", sizeof(size_t));

    /* The standard leaves plain char signedness to the implementation. */
    if ((char)0xFF < 0) {
        printf("plain char signed     = yes\n");
    } else {
        printf("plain char signed     = no\n");
    }

    return 0;
}
```

Compile and run as C17:

```bash
gcc -std=c17 -O0 -Wall -Wextra -pedantic demo.c -o demo
./demo
```

Real output:

```
__STDC_VERSION__      = 201710
CHAR_BIT              = 8
INT_MAX               = 2147483647
sizeof(size_t)        = 8 bytes
plain char signed     = yes
```

The standard tells you `CHAR_BIT` exists and is at least 8, not that every target must look
like this one. It tells you `INT_MAX` comes from `<limits.h>`, not that every ABI has the
same `int`. It tells you plain `char` signedness is implementation-defined, so the last
line is something to ask your compiler/target, not something to assume globally.

## Failure modes & trade-offs

- **Reading examples as exhaustive rules.** Examples teach shape; constraints and semantics
  define the contract.
- **Ignoring version markers.** C90, C99, C11, C17, and C23 differ. Your compiler mode
  decides which dialect you asked for.
- **Confusing undefined and implementation-defined.** Implementation-defined behavior is a
  documented choice. Undefined behavior is no contract.
- **Assuming cppreference is normative.** It is useful and usually excellent, but it is not
  the standard.
- **Forgetting the implementation.** ABI, compiler flags, target architecture, libc, and
  OS all instantiate choices the standard leaves open.

## In practice

- **Start with cppreference, finish with the contract.** Use it to find the concept, then
  verify exact edge cases in WG14 text or implementation docs.
- **Keep a local vocabulary list.** "Object," "lvalue," "effective type," "constraint,"
  "storage duration," and "linkage" have technical meanings in C.
- **Check the selected dialect.** `-std=c17` and `-std=gnu17` are not identical promises.
- **Prefer implementation queries for implementation choices.** Use `<limits.h>`,
  `<stdint.h>`, `_Alignof`, `sizeof`, compiler macros, and small programs to inspect the
  target.
- **Mark unverifiable claims.** If you cannot tie a rule to the standard, cppreference, or
  compiler documentation, treat it as a hypothesis.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: the contract]] · [[lowlevel/c-from-the-metal/integer-promotions-and-implicit-conversions|Integer promotions and implicit conversions]] · [[lowlevel/c-from-the-metal/build-and-run-a-c-program-from-zero|Build and run a C program from zero]] · [[lowlevel/reference-registry|Reference Registry]]

## Sources

- **WG14 documents** — official working group document index for C drafts, papers, defect reports, and meeting material. https://www.open-std.org/jtc1/sc22/wg14/www/docs/
- **ISO/IEC 9899:2011 draft N1570** — freely available C11 draft useful for learning the structure of clauses, constraints, semantics, and library wording. https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- **cppreference — C reference** — searchable map of C language and library pages with version markers, examples, notes, and defect reports. https://en.cppreference.com/w/c
- **cppreference — Undefined behavior** — practical catalog of undefined, unspecified, and implementation-defined behavior with examples. https://en.cppreference.com/w/c/language/behavior
- **GCC manual — Standards** — how GCC selects C dialects, GNU extensions, and conformance modes. https://gcc.gnu.org/onlinedocs/gcc/Standards.html
- **Clang language compatibility** — Clang's compatibility notes and extension/conformance context for C-family languages. https://clang.llvm.org/compatibility.html
