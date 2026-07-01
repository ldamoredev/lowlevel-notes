---
title: "Symbols: definition, reference, and resolution"
description: Linkers work over symbols: definitions provide code or storage, references ask for them, and resolution decides whether the final program is coherent.
tags: [toolchain, linker, symbols, linkage, object-files]
order: 4
updated: 2026-07-01
---
# Symbols: definition, reference, and resolution

A linker does not understand your program as C modules, classes, or architecture diagrams.
It sees **symbols**. A symbol is a named thing in an object file: a function, global object,
local helper, section marker, or unresolved external name. Linking is the act of matching
references to definitions, choosing addresses, and reporting the cases where the graph does
not make sense. Once you can read symbols, "undefined reference" becomes a precise fact
instead of a mood.

> The reset: a declaration is enough for the compiler; a definition is what the linker can
> bind to. Symbol resolution is where that distinction becomes mechanical.

## How it really works

Each relocatable object contributes a symbol table. Some entries are **definitions**:
"this object file provides code or storage for this name." Some entries are **references**:
"this object file needs a definition for this name from somewhere else." The linker builds
a combined view and tries to resolve every external reference that must be resolved at link
time.

| Symbol idea | `nm` shape | Meaning |
|---|---|---|
| Defined function | `T name` | global text/code definition |
| Local function | `t name` | text/code definition private to this object |
| Initialized object | `D name` | global writable data definition |
| Zero/common object | `B` or `C name` | zero-filled or common storage |
| Undefined reference | `U name` | this object needs a definition elsewhere |
| Weak symbol | `W` / `V` / lowercase variants | definition or reference with weaker binding rules |

The exact letters vary by object format and tool, but the categories are stable: defined
here, local here, missing here, weakly bound, or attached to a section. ELF also records
binding (`LOCAL`, `GLOBAL`, `WEAK`), type (`FUNC`, `OBJECT`, `SECTION`, `FILE`), visibility,
size, value, and section index.

For ordinary C, the default rule is simple: file-scope non-`static` functions and objects
become external definitions; file-scope `static` names become local symbols; declarations
such as `extern int f(void);` let the compiler type-check a use but do not create a
definition. This is exactly the C-side split from
[[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declarations vs definitions, and linkage]].

## Resolution rules

The linker's common path is a left-to-right accumulation of object files and libraries. For
plain object files, it reads definitions and unresolved references, then matches names.
Static archives add an order-sensitive extraction rule, which gets its own later note; the
symbol model underneath is the same.

Strong definitions are the normal definitions you write in C: function bodies and initialized
global objects. A required external reference must resolve to one compatible definition by
the end of the link. If no definition exists, you get an undefined-reference error. If
multiple strong definitions of the same external name are present, you get a multiple-
definition error.

Weak symbols are a controlled exception. A weak definition can be overridden by a strong
definition with the same name, and an unresolved weak reference may be allowed to remain
zero/null depending on platform and relocation. Weak symbols are useful for runtime hooks,
optional features, and low-level library mechanisms, but they are not a substitute for clean
ownership of definitions.

Local symbols do not participate in global resolution. A `static int hidden_bias(void)` in
one object file can have the same local name as a helper in another object file because the
linker does not export those names as global candidates.

## Executable artifact: watch `U` become resolved

The example lives in
`examples/toolchain-and-linking/symbols-definition-reference-and-resolution/`. `main.c`
includes declarations and calls two functions, but their definitions live in `mathlib.c`:

```c
#include <stdio.h>

#include "api.h"

int main(void) {
    int sum = public_add(20, 1);
    printf("sum=%d twice=%d\n", sum, public_twice(sum));
    return 0;
}
```

`mathlib.c` exports two symbols and keeps one helper local:

```c
#include "api.h"

static int hidden_bias(void) {
    return 1;
}

int public_add(int left, int right) {
    return left + right + hidden_bias();
}

int public_twice(int value) {
    return value * 2;
}
```

Run it:

```bash
cd examples/toolchain-and-linking/symbols-definition-reference-and-resolution
./run.sh
```

Real output from this machine:

```text
== compile objects ==
== nm main.o: references waiting for definitions ==
0000000000000000 T _main
                 U _printf
                 U _public_add
                 U _public_twice
== nm mathlib.o: exported and internal definitions ==
0000000000000030 t _hidden_bias
0000000000000000 T _public_add
0000000000000040 T _public_twice
== link resolves the U symbols ==
sum=22 twice=44
```

Before the link, `main.o` has `U _public_add` and `U _public_twice`: the compiler accepted
the declarations, but the object file still needs definitions. `mathlib.o` provides
`T _public_add` and `T _public_twice`, so the linker can bind the references. The helper is
`t _hidden_bias`, lowercase local text, because `static` at file scope gave it internal
linkage. `_printf` is resolved from libc through the normal hosted C link.

On Linux/ELF you normally see names without leading underscores. In C++, the names may be
mangled to encode namespaces, overloads, and parameter types; `c++filt` demangles them. The
linker still resolves symbol names, but C++ makes those names less human-shaped.

## Failure modes & trade-offs

- **Undefined reference.** Some object refers to a global symbol, but the link inputs never
  provide a required definition. Common causes: missing `.o`, missing library flag, wrong
  library order for static archives, typo, or mismatched conditional compilation.
- **Multiple definition.** More than one strong external definition of the same symbol was
  linked. The classic C bug is defining a global object in a header instead of declaring it
  with `extern` and defining it once in a `.c`.
- **Hidden by `static`.** A file-scope `static` helper is intentionally local. That is good
  for encapsulation, but another object file cannot link against it.
- **ABI or language mismatch.** C++ name mangling, calling convention differences, or
  missing `extern "C"` can produce a symbol that looks absent even though a similar function
  exists.
- **Weak symbols can hide ownership bugs.** They are useful for low-level extension points,
  but they can also make the link succeed when you expected a hard missing-definition error.
- **Dynamic symbols are a later stage.** Shared libraries add exported dynamic symbol tables,
  interposition, versioning, and loader-time lookup. Do not debug those with only the
  static-link mental model.

## In practice

- **Read `nm` output as a checklist.** For every `U name`, find exactly one intended
  provider among your objects or libraries.
- **Prefer one owner per external symbol.** Headers declare the public surface; one source
  file owns each external definition.
- **Default private helpers to `static`.** If no other translation unit should call a
  helper, keep it out of the global symbol namespace.
- **When linking C++ from C, control the name.** Use `extern "C"` at the boundary so the C
  side looks for the symbol the C++ side actually exports.
- **Separate symbol resolution from relocation.** Resolution chooses which definition a
  reference means. Relocation patches the bytes after layout. They cooperate, but they are
  not the same step.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/toolchain-and-linking/object-files-and-whats-inside-them|Object files and what's inside them]] · [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|The ELF format]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declarations vs definitions, and linkage]] · [[lowlevel/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link|The pipeline: preprocess → compile → assemble → link]]

## Sources

- **GNU binutils — `nm` manual** — symbol letters, undefined symbols, weak symbols, and local/global symbol display. https://sourceware.org/binutils/docs/binutils/nm.html
- **System V ABI — ELF specification** — symbol table entries, binding, type, visibility, section index, and relocation records. https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html
- **David Drysdale — *Beginner's Guide to Linkers*** — concrete walkthrough of symbols, unresolved references, and linker errors. https://www.lurklurk.org/linkers/linkers.html
- **Ian Lance Taylor — *Linkers* series** — linker implementation view of symbol tables and resolution. https://www.airs.com/blog/archives/38
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 7 — strong/weak symbols, symbol resolution, and relocation. https://csapp.cs.cmu.edu/
- **cppreference — Declarations and definitions in C** — language-side distinction that feeds the linker-side symbol model. https://en.cppreference.com/w/c/language/declarations
