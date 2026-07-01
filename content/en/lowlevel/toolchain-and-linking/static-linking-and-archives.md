---
title: "Static linking and archives (`.a`)"
description: Static archives are libraries of relocatable object files; the linker pulls only the members needed to resolve unresolved symbols, and library order matters.
tags: [toolchain, linker, static-linking, archives, libraries]
order: 5
updated: 2026-06-22
---
# Static linking and archives (`.a`)

A static library is not a special binary format with one merged body of code. It is an
**archive**: a table of relocatable `.o` members plus an index of symbols those members
define. During a static link, the linker scans objects and archives, pulls in only archive
members that satisfy unresolved symbols it has already seen, then treats those extracted
members like ordinary object files. That extraction rule is why the order of `main.o
-lthing` vs `-lthing main.o` can decide whether the link succeeds.

> The reset: an archive is searched, not linked whole by default. Object files are always
> included when named; archive members are included only when they answer a current need.

## How it really works

Static linking consumes relocatable objects and produces an executable whose required
library code has been copied into the output. For a plain object file, the rule is simple:
if you name `helper.o` on the command line, the linker includes it. For an archive such as
`libcalc.a`, the linker opens the archive index and extracts only members whose definitions
resolve currently undefined external symbols.

The usual Unix naming convention is `libNAME.a`, used from the driver as `-lNAME`. The
driver passes the library search path with `-Ldir`; the linker looks for `libNAME.so` or
`libNAME.a` depending on platform defaults and flags. When the static archive is selected,
the symbol index lets the linker jump to the member that defines a needed symbol instead of
reading every object blindly.

The left-to-right rule is the part that bites. If the linker sees `main.o` first, it records
undefined references such as `calc_add` and `calc_mul`. When it later scans `libcalc.a`, it
extracts `add.o` and `mul.o` because those members define the waiting symbols. If it sees
`-lcalc` before `main.o`, there are no waiting references yet, so the archive contributes
nothing; after `main.o` appears, the needed symbols are unresolved and the archive is not
automatically rescanned.

| Input kind | Linker behavior |
|---|---|
| `main.o` | always included when named |
| `libcalc.a(add.o)` | extracted only if it resolves a current undefined symbol |
| `libcalc.a(mul.o)` | same archive-member rule |
| `libc` | normally added by the compiler driver for hosted C |

Static linking gives you a self-contained copy of the selected library code, but it does
not erase ABI rules. The objects must still match the target architecture, object format,
calling convention, relocation model, and runtime assumptions. A Linux/ELF archive cannot
be linked into a macOS/Mach-O executable just because both contain x86-64 machine code.

## Executable artifact: archive extraction and order

The example lives in
`examples/toolchain-and-linking/static-linking-and-archives/`. It builds two library
objects, packs them into `libcalc.a`, shows the symbols in the archive, and runs the native
program. The script also checks the non-portable `-lcalc main.o` order on this host so the
note can be honest about platform behavior.

`main.c` uses declarations from `calc.h`:

```c
#include <stdio.h>

#include "calc.h"

int main(void) {
    int sum = calc_add(2, 5);
    int product = calc_mul(sum, 3);
    printf("static archive result: %d\n", product);
    return 0;
}
```

The archive members define the symbols:

```c
#include "calc.h"

int calc_add(int left, int right) {
    return left + right;
}
```

```c
#include "calc.h"

int calc_mul(int left, int right) {
    return left * right;
}
```

Run it:

```bash
cd examples/toolchain-and-linking/static-linking-and-archives
./run.sh
```

Real output from this machine:

```text
== native archive and run ==
__.SYMDEF SORTED
add.o
mul.o
== symbols inside archive members ==

add.o:
0000000000000000 T _calc_add

mul.o:
0000000000000000 T _calc_mul
== library before object on this Apple ld ==
static archive result: 21
== portable order: object before library ==
static archive result: 21
== portability note ==
GNU/ELF linkers commonly require the portable order above for static archives.
```

This run is on macOS/Mach-O, so C symbols have a leading underscore and Apple `ld` accepts
this small `-lcalc main.o` case. Do not generalize that to Linux/ELF. GNU-style archive
search is the stricter rule described above: put objects that create unresolved references
before the archives that satisfy them. On Linux/ELF, the same `nm` output usually prints
`calc_add` and `calc_mul` without the underscore, and the non-portable order commonly fails
with an undefined-reference diagnostic.

## Failure modes & trade-offs

- **Wrong library order.** Static archive extraction is order-sensitive. Put objects that
  create references before the archives that satisfy them.
- **Circular archive dependencies.** If `liba.a` and `libb.a` need each other, one scan may
  not be enough. GNU linkers provide grouping options such as `--start-group`/`--end-group`
  at a cost; another fix is to design cleaner archive boundaries.
- **Duplicate strong definitions.** If two included objects define the same external
  symbol, the link fails. Archives can hide this until a member is extracted.
- **Larger executables.** Static linking copies selected library code into each program.
  That can help deployment, but it can duplicate code across many processes.
- **Update friction.** A dynamically linked library can be patched in one place; a statically
  linked program must be relinked and redistributed to pick up library fixes.
- **License and ABI constraints.** Some libraries impose distribution obligations or expect
  dynamic linking. Static does not mean "free of policy."

## In practice

- **Read link lines left to right.** When a static link fails, move `-lfoo` after the
  objects that reference `foo`, then inspect with `nm` before changing source.
- **Use `ar -t` and `nm libx.a`.** They tell you which members exist and which symbols they
  actually define.
- **Prefer explicit object ownership.** Headers declare; one `.c` owns each definition;
  archives package objects without changing that rule.
- **Use static linking deliberately.** It is excellent for freestanding runtimes, small
  deployment envelopes, rescue tools, and OS-project support libraries. It is less pleasant
  for fast security updates and shared system libraries.
- **Let the compiler driver invoke the linker.** `gcc main.o -L. -lcalc` adds the hosted C
  runtime pieces; direct `ld` means you must supply them yourself.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/toolchain-and-linking/symbols-definition-reference-and-resolution|Symbols: definition, reference, and resolution]] · [[lowlevel/toolchain-and-linking/object-files-and-whats-inside-them|Object files and what's inside them]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declarations vs definitions, and linkage]]

## Sources

- **GNU binutils — `ar` manual** — archive creation, member listing, and symbol-index behavior for static libraries. https://sourceware.org/binutils/docs/binutils/ar.html
- **GNU `ld` manual — archive search** — how archive members are pulled to resolve undefined symbols and why order matters. https://sourceware.org/binutils/docs/ld/
- **Ian Lance Taylor — *Linkers* series** — linker implementation view of archive scanning and symbol resolution. https://www.airs.com/blog/archives/38
- **David Drysdale — *Beginner's Guide to Linkers*** — practical explanation of static libraries and the left-to-right link rule. https://www.lurklurk.org/linkers/linkers.html
- **John R. Levine — *Linkers and Loaders*** — static linking, libraries, symbol resolution, and loader context. https://linker.iecc.com/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 7 — archive files and static linking from the programmer-visible side. https://csapp.cs.cmu.edu/
