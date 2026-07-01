---
title: "Dynamic linking and shared libraries (`.so`)"
description: Shared libraries keep library code outside the executable: the linker records dependencies, and the dynamic loader maps and resolves them at runtime.
tags: [toolchain, linker, dynamic-linking, shared-libraries, loader]
order: 6
updated: 2026-06-22
---
# Dynamic linking and shared libraries (`.so`)

Dynamic linking moves part of linking from build time to load time. The executable does not
contain a private copy of every library function; it records that it needs a shared library,
and the dynamic loader maps that library into the process when the program starts. On
Linux/ELF the file is usually a `.so`; on macOS/Mach-O it is a `.dylib`. The benefit is
shared code, smaller executables, and update flexibility. The cost is a runtime dependency
graph that can fail after the link already succeeded.

> The reset: static linking copies selected code into the executable. Dynamic linking
> records a contract: "at runtime, load this shared object and bind these symbols."

## How it really works

A shared library is a loadable binary designed to be mapped into many processes. It exports
dynamic symbols, contains relocation metadata, and is built so its code can run no matter
where the loader maps it. That is why C objects for shared libraries are usually compiled
with `-fPIC`: position-independent code avoids hard-coded absolute addresses and uses
relocation-friendly addressing through tables such as the GOT on ELF.

On Linux, the common build shape is:

```bash
gcc -O2 -Wall -Wextra -fPIC -c greeting.c -o greeting.o
gcc -shared -Wl,-soname,libgreeting.so.1 greeting.o -o libgreeting.so.1.0
gcc main.c -L. -lgreeting -Wl,-rpath,'$ORIGIN' -o demo
```

The static linker still runs. It verifies that the executable's undefined references can be
satisfied by the shared library's exported symbols, then writes dynamic-linking metadata
into the executable instead of copying all library code. In ELF, that metadata includes
entries such as `DT_NEEDED`, symbol tables for dynamic lookup, relocation records, and often
an interpreter path such as `/lib64/ld-linux-x86-64.so.2`.

The shared library also has an identity. On ELF, the **SONAME** is the compatibility name
recorded by consumers, such as `libgreeting.so.1`, while the real file may be
`libgreeting.so.1.0.3`. On macOS, the analogous idea is the install name, visible with
`otool -L`, often using `@rpath`, `@loader_path`, or `@executable_path`.

## Runtime resolution

At program launch, the kernel loads the executable and hands control to the dynamic loader
named in the binary. The loader reads the dependency list, finds each shared library through
configured search paths, maps segments with correct permissions, applies required
relocations, and resolves symbols either immediately or lazily depending on platform and
binding mode. The next note goes deeper into relocation, PLT, GOT, and lazy binding; here
the important boundary is that the library dependency survives into runtime.

Dynamic linking also enables sharing. If ten processes map the same read-only code pages
from `libc.so`, the OS can keep one physical copy of those pages and map them into each
process. Writable data remains per process because each process needs its own state.

## Executable artifact: build a shared library

The example lives in
`examples/toolchain-and-linking/dynamic-linking-and-shared-libraries/`. It builds a native
macOS `.dylib` because this machine can run Mach-O binaries, but the source and flags map
directly to the Linux `.so` shape above.

`greeting.c` becomes the shared library:

```c
#include "greeting.h"

int greeting_value(void) {
    return 42;
}

const char *greeting_label(void) {
    return "shared-library";
}
```

`main.c` links against it:

```c
#include <stdio.h>

#include "greeting.h"

int main(void) {
    printf("%s value=%d\n", greeting_label(), greeting_value());
    return 0;
}
```

Run it:

```bash
cd examples/toolchain-and-linking/dynamic-linking-and-shared-libraries
./run.sh
```

Real output from this machine:

```text
== compile position-independent object ==
== build shared library ==
libgreeting.dylib: Mach-O 64-bit dynamically linked shared library x86_64
== link executable with runtime search path ==
== dynamic dependencies ==
demo:
	@rpath/libgreeting.dylib (compatibility version 0.0.0, current version 0.0.0)
	/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1345.120.2)
== run ==
shared-library value=42
```

The important line is `@rpath/libgreeting.dylib`: the executable records a runtime
dependency, not a copied implementation. The link command adds `-Wl,-rpath,@loader_path`,
so the loader can find the library next to the executable. On Linux/ELF the equivalent
inspection is usually `ldd ./demo` or `readelf -d ./demo`, and the equivalent search-token
pattern is often `$ORIGIN`.

## Failure modes & trade-offs

- **Library found at link time, missing at runtime.** The executable can link successfully
  and later fail with `cannot open shared object file` on Linux or `Library not loaded` on
  macOS if the loader cannot find the library.
- **ABI drift.** If a shared library changes exported functions, struct layout, calling
  convention assumptions, or symbol versions without preserving compatibility, old
  executables can break.
- **PIC has a small cost.** Position-independent code may require extra indirection, though
  modern x86-64 makes the cost much lower than old absolute-address models.
- **Updates are centralized.** Patching one shared library can fix many programs, but a bad
  update can also break many programs.
- **Symbol interposition is power and danger.** Dynamic linking can allow override hooks,
  preload mechanisms, and accidental collisions. Visibility controls matter.
- **Deployment becomes a graph problem.** You ship an executable plus the right compatible
  libraries and loader paths, not one sealed file.

## In practice

- **Use shared libraries for common, updateable code.** libc, GUI frameworks, crypto
  libraries, and platform runtimes are natural dynamic dependencies.
- **Use `-fPIC` for library objects.** Treat it as the default for code that may become a
  `.so` or `.dylib`.
- **Inspect dynamic dependencies.** Use `readelf -d`, `ldd`, `objdump -p`, or `otool -L`
  before guessing why a program will not launch.
- **Name compatibility deliberately.** On ELF, set a SONAME for versioned shared libraries.
  On macOS, understand install names and `@rpath`.
- **Keep static and dynamic mental models separate.** Static archives are searched and
  extracted by the build-time linker; shared libraries are mapped and resolved by the
  runtime loader.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/toolchain-and-linking/static-linking-and-archives|Static linking and archives]] · [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|The ELF format]] · [[lowlevel/toolchain-and-linking/symbols-definition-reference-and-resolution|Symbols: definition, reference, and resolution]] · [[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]]

## Sources

- **Ulrich Drepper — *How To Write Shared Libraries*** — deep ELF/Linux treatment of PIC, symbol visibility, relocation, and shared-library design. https://akkadia.org/drepper/dsohowto.pdf
- **GNU `ld` manual** — `-shared`, `-soname`, dynamic sections, rpath, and linker behavior. https://sourceware.org/binutils/docs/ld/
- **GCC Manual — code generation options** — `-fpic`/`-fPIC` and position-independent code. https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html
- **Linux `ld.so(8)` manual page** — runtime dynamic loader behavior and search paths. https://man7.org/linux/man-pages/man8/ld.so.8.html
- **Apple `dyld` manual page** — macOS dynamic loader, install names, and path tokens. https://keith.github.io/xcode-man-pages/dyld.1.html
- **Ian Lance Taylor — *Linkers* series** — dynamic linking and symbol resolution from a linker implementation perspective. https://www.airs.com/blog/archives/38
