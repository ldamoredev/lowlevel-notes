---
title: "Translation units, declarations vs definitions, and linkage"
description: C is compiled one preprocessed source file at a time. Headers declare names, source files define storage and function bodies, and the linker resolves external symbols across object files.
tags: [c, translation-units, declarations, definitions, linkage, linker]
order: 6
updated: 2026-06-29
---
# Translation units, declarations vs definitions, and linkage

A C compiler does not see your whole program at once. It sees one **translation unit**:
one `.c` file after preprocessing has pasted in the headers and expanded macros. Each
translation unit becomes an object file, and the final program appears only when the
linker stitches those object files together. That means a header can make a name known to
the compiler, but only a definition in some object file can give the linker something real
to resolve.

> The reset: a declaration is "this name exists with this type." A definition is "here is
> the storage or function body." Linkage decides whether another translation unit can refer
> to the same name.

## The compiler sees one translation unit

The preprocessor runs first. If `main.c` includes `counter.h`, the compiler does not treat
that as an import boundary; it receives one large token stream: `main.c` plus the included
declarations. That preprocessed source is the translation unit. The compiler parses it,
checks types inside it, and emits `main.o`.

The compiler can trust declarations from headers:

```c
extern int counter_value;
int counter_next(void);
```

Those lines are enough for `main.c` to type-check an expression like `counter_next()` or
`counter_value`. They are not enough to build a program. The object file produced from
`main.c` still contains holes for those names. The linker later fills those holes from
`counter.o`, the object file that contains the definitions.

This is the C-side version of the symbol table you saw in
[[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]]:
`printf` appeared as an unresolved `U` symbol in one object file until the link step found
the library definition.

## How it really works

A **declaration** introduces an identifier and its type. A **definition** is a declaration
that also provides the storage, the initializer, or the function body. You may repeat
compatible declarations; you must not provide competing definitions for the same external
object or function.

| Source | Meaning |
|---|---|
| `extern int x;` | declaration of an object defined elsewhere |
| `int x = 0;` | definition of an object with storage |
| `int next(void);` | function declaration / prototype |
| `int next(void) { return 1; }` | function definition |

Headers usually contain declarations: function prototypes, `extern` declarations for
shared objects, type definitions, macros, and `static inline` helpers when chosen
carefully. Source files usually contain definitions: real file-scope objects and function
bodies. That split is why a header can be included by many `.c` files without creating
many copies of the same global variable.

Linkage controls whether two declarations in different scopes or translation units can
denote the same entity:

| Linkage | How you usually get it | Who can name it |
|---|---|---|
| External linkage | file-scope functions and non-`static` file-scope objects | other translation units can refer to it |
| Internal linkage | `static` at file scope | only the current translation unit can refer to it |
| No linkage | local automatic variables, labels, most block-scope names | only that scope has the name |

The overloaded keyword is `static`. At file scope, `static int step;` gives `step`
internal linkage, making the name private to that translation unit. Inside a function,
`static int calls;` is different: the object has static storage duration, but the local
name still has no linkage. Same keyword, different rule.

The linker works over symbols. If `main.o` says `U _counter_next`, it means "this object
file calls a function named `_counter_next`, but it is not defined here." If `counter.o`
says `T _counter_next`, it contains the function body. The linker matches the reference to
the definition and writes the final executable.

## Executable artifact: two objects and a linker

The example lives in
`examples/c-from-the-metal/translation-units-declarations-and-linkage/`. It has a header,
one implementation translation unit, and one user translation unit.

`counter.h` declares the public surface:

```c
#ifndef COUNTER_H
#define COUNTER_H

extern int counter_value;

void counter_reset(int value);
int counter_next(void);
int counter_read(void);

#endif
```

`counter.c` defines the public names and keeps helper state private with file-scope
`static`:

```c
#include "counter.h"

int counter_value = 0;

static int step = 1;

static int add_step(int value) {
    return value + step;
}

void counter_reset(int value) {
    counter_value = value;
}

int counter_next(void) {
    counter_value = add_step(counter_value);
    return counter_value;
}

int counter_read(void) {
    return counter_value;
}
```

`main.c` includes the declarations and uses names defined elsewhere:

```c
#include <stdio.h>

#include "counter.h"

int main(void) {
    counter_reset(40);

    printf("counter_read() = %d\n", counter_read());
    printf("counter_next() = %d\n", counter_next());
    printf("counter_value  = %d\n", counter_value);

    return 0;
}
```

Compile and link directly:

```bash
gcc -O0 -Wall -Wextra counter.c main.c -o demo
./demo
```

Real output:

```
counter_read() = 40
counter_next() = 41
counter_value  = 41
```

Now stop at object files, link explicitly, and inspect symbols:

```bash
./run.sh
```

Real output:

```
== compile each translation unit to an object file ==
== link the objects into one executable ==
== run ==
counter_read() = 40
counter_next() = 41
counter_value  = 41
== nm counter.o ==
0000000000000040 t _add_step
0000000000000020 T _counter_next
0000000000000060 T _counter_read
0000000000000000 T _counter_reset
00000000000001a8 S _counter_value
000000000000006c d _step
== nm main.o ==
                 U _counter_next
                 U _counter_read
                 U _counter_reset
                 U _counter_value
0000000000000000 T _main
                 U _printf
```

The exact symbol prefix and data-section letters are object-format details. This run is on
macOS/Mach-O, so C names appear with leading underscores and `counter_value` appears as
`S`; on ELF/Linux you may see names without `_` and data as `D` or `B`. The lesson is the
same: `T` is externally visible code defined here, `U` is an unresolved external reference,
and lowercase symbols such as `t` and `d` are local to the object file. `add_step` and
`step` exist, but they are not exported for `main.o` to bind against.

## Failure modes & trade-offs

- **Undefined reference.** The compiler accepted a declaration, but the linker never found
  a matching definition. Common causes: you forgot to compile one `.c` file, forgot to link
  a library such as `-lm`, misspelled a symbol, or put the definition behind a build flag.
- **Multiple definition.** Two object files both define the same external object or
  function. The classic beginner bug is writing `int global = 0;` in a header and including
  it from multiple `.c` files. Put `extern int global;` in the header and `int global = 0;`
  in exactly one source file.
- **Tentative definitions are a historical edge.** A file-scope `int x;` without
  `extern` or an initializer is a tentative definition. Multiple tentative definitions in
  one translation unit collapse to one zero-initialized definition, but across translation
  units this interacts with compiler defaults and can still produce link surprises.
- **`static` in headers can duplicate state.** A file-scope `static int cache;` in a header
  creates a separate internal object in every translation unit that includes it. Sometimes
  that is intentional; often it is a bug disguised as privacy.
- **C `inline` is not C++ `inline`.** C's `inline`, `extern inline`, and `static inline`
  have linkage rules that surprise people; use `static inline` in headers unless you have a
  specific external-definition plan.

## In practice

- **Headers declare; one `.c` defines.** That rule prevents most duplicate-symbol bugs.
- **Keep file-scope `static` as the default for private helpers.** If no other translation
  unit should call it, do not export it.
- **Make public headers boring.** Prefer declarations, type names, macros, and carefully
  chosen `static inline` helpers. Avoid non-`static` object definitions in headers.
- **Read link errors as link errors.** If the message says "undefined reference" or
  "duplicate symbol," the compiler stage already finished. Look at object files, libraries,
  and symbol names.
- **Use `nm` early.** It turns vague linking anxiety into concrete facts: which object
  defines the symbol, which object references it, and whether a helper stayed internal.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/the-preprocessor-macros-includes-conditional-compilation|The preprocessor]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|The C type system is weak]] · [[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]] · [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for translation units, declarations, definitions, storage duration, linkage, tentative definitions, and function definitions. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Declarations** — compact reference for declarators, what a declaration introduces, and when a declaration is also a definition. https://en.cppreference.com/c/language/declarations
- **cppreference — Storage-class specifiers** — the practical map of `extern`, `static`, storage duration, and external/internal/no linkage. https://en.cppreference.com/c/language/storage_class_specifiers
- **cppreference — External and tentative definitions** — focused reference for `extern int x;`, tentative definitions, and one-definition constraints in C. https://en.cppreference.com/c/language/extern
- **GNU binutils — `nm` manual** — what symbol letters such as `T`, `U`, and lowercase local symbols mean when inspecting object files. https://sourceware.org/binutils/docs/binutils/nm.html
- **Jens Gustedt — *Modern C*** — modern explanation of headers, translation units, linkage, `inline`, and interface design in C. https://gustedt.gitlabpages.inria.fr/modern-c/
