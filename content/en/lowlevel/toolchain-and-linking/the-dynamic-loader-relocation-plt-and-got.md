---
title: "The dynamic loader, relocation, PLT and GOT"
description: The dynamic loader maps shared libraries, applies relocations, and uses structures such as the GOT and PLT so position-independent code can call and access external symbols.
tags: [toolchain, dynamic-loader, relocation, plt, got, pie]
order: 7
updated: 2026-06-22
---
# The dynamic loader, relocation, PLT and GOT

The earlier execution note showed the broad load → run story. This one zooms into the
dynamic-linking machinery: the loader maps shared libraries, fixes addresses that were not
known at static link time, and leaves behind tables that code can use without knowing where
the library landed. On ELF, the two names to keep straight are the **GOT** (Global Offset
Table) for addresses of external data/functions and the **PLT** (Procedure Linkage Table)
for calls that may be resolved through the dynamic linker. PIE and ASLR make this machinery
ordinary: code is built to move, then relocated into one actual process layout.

> The reset: the static linker writes "places to fix" into the binary. The dynamic loader
> chooses runtime addresses, maps objects, and applies those fixes before or during calls.

## How it really works

An executable or shared library can contain references whose final value is unknowable at
static link time. A shared object might be mapped at a different virtual address in every
process. A function might come from another `.so`. A variable might be interposable. The
binary therefore carries relocation entries: typed instructions that say "write the runtime
address or displacement for this symbol at this offset."

Position-independent code makes those runtime patches manageable. Instead of baking an
absolute address into every instruction, code often uses PC-relative addressing and tables.
In ELF, the GOT contains slots for addresses that the loader can fill. Code loads through a
GOT slot when it needs an external object or a function pointer. The PLT is a set of
callable stubs used for external function calls; a PLT entry can bounce through the dynamic
linker the first time a function is called, then jump directly through the resolved GOT slot
afterward.

| Mechanism | Job |
|---|---|
| Relocation entry | describes one runtime patch the loader or linker must apply |
| GOT | stores runtime addresses used by position-independent code |
| PLT | provides call stubs for dynamically linked functions |
| Lazy binding | resolves a function on first call instead of program startup |
| PIE | makes the main executable position-independent too |
| ASLR | lets the OS randomize mapping addresses between launches |

Not every relocation is lazy. Data relocations and some security modes require eager work at
startup. Linux can force eager function binding with `LD_BIND_NOW=1`, and hardened builds
often combine eager binding with RELRO so parts of the GOT become read-only after relocation.
The details differ across loaders, but the shape is the same: map objects, resolve names,
patch addresses, then run user code.

## Loader sequence

On Linux/ELF, the kernel maps the main executable and starts the interpreter named in the
ELF program headers, commonly `ld-linux-x86-64.so.2`. The dynamic loader reads `DT_NEEDED`
dependencies, finds and maps them, computes load addresses, processes relocations, runs
initializers, and finally transfers control to the program entry path. On macOS, `dyld`
does the analogous work for Mach-O images, install names, bind/rebase opcodes, and
`@rpath` resolution.

This is separate from the broad source-to-execution path in
[[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]]. That
note names the pipeline and process launch. This note names the data structures inside the
dynamic link step: relocation records, GOT slots, PLT stubs, loader search paths, and
runtime binding.

## Executable artifact: run and inspect relocations

The example lives in
`examples/toolchain-and-linking/the-dynamic-loader-relocation-plt-and-got/`. It first builds
and runs a native `.dylib` example so the loader path is real on this machine. Then it
cross-emits a Linux/ELF PIC object and uses `objdump` to show relocation records that name
GOT/PLT-style work.

`main.c` calls a function from a shared library:

```c
#include <stdio.h>

#include "plugin.h"

int main(void) {
    printf("loader result=%d\n", plugin_scale(5));
    return 0;
}
```

`elf_pic.c` is intentionally libc-free so Clang can emit Linux/ELF on this macOS host:

```c
extern int external_data;
extern int external_call(int value);

int use_external(void) {
    return external_data + external_call(7);
}
```

Run it:

```bash
cd examples/toolchain-and-linking/the-dynamic-loader-relocation-plt-and-got
./run.sh
```

Real output from this machine:

```text
== native dynamic loader path ==
demo:
	@rpath/libplugin.dylib (compatibility version 0.0.0, current version 0.0.0)
	/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1345.120.2)
loader result=42
== Linux/ELF PIC relocations ==
elf_pic.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped

elf_pic.o:	file format elf64-x86-64

RELOCATION RECORDS FOR [.text]:
OFFSET           TYPE                     VALUE
000000000000000b R_X86_64_REX_GOTPCRELX   external_data-0x4
000000000000001a R_X86_64_PLT32           external_call-0x4
== disassembly around unresolved accesses ==

elf_pic.o:	file format elf64-x86-64

Disassembly of section .text:

0000000000000000 <use_external>:
       0: 55                           	pushq	%rbp
       1: 48 89 e5                     	movq	%rsp, %rbp
       4: 48 83 ec 10                  	subq	$16, %rsp
       8: 48 8b 05 00 00 00 00         	movq	(%rip), %rax            # 0xf <use_external+0xf>
       f: 8b 00                        	movl	(%rax), %eax
      11: 89 45 fc                     	movl	%eax, -4(%rbp)
      14: bf 07 00 00 00               	movl	$7, %edi
      19: e8 00 00 00 00               	callq	0x1e <use_external+0x1e>
      1e: 89 c1                        	movl	%eax, %ecx
      20: 8b 45 fc                     	movl	-4(%rbp), %eax
      23: 01 c8                        	addl	%ecx, %eax
      25: 48 83 c4 10                  	addq	$16, %rsp
      29: 5d                           	popq	%rbp
      2a: c3                           	retq
```

The relocation table tells you what the zero placeholders in the disassembly mean. The load
through `(%rip)` has a `R_X86_64_REX_GOTPCRELX external_data` relocation: the linker/loader
will arrange a GOT-based address for external data. The `callq` placeholder has
`R_X86_64_PLT32 external_call`: the call is eligible for PLT-style binding to an external
function. In a fully linked executable or shared library, those records become part of the
dynamic relocation and binding story the loader consumes.

## Failure modes & trade-offs

- **Missing shared object.** The static link succeeded, but the loader cannot find a
  `DT_NEEDED` library or Mach-O install name at launch.
- **Relocation cannot be represented.** The code model, relocation type, or address range is
  incompatible with the chosen layout.
- **Text relocations hurt security and sharing.** Patching executable code pages can require
  writable text and prevent clean page sharing. PIC exists largely to avoid that.
- **Lazy binding shifts failure later.** The program may start and fail only when a missing
  function is first called. Eager binding finds more problems at startup.
- **Interposition changes meaning.** A symbol can resolve to a different definition than
  you expected because dynamic lookup rules allow overrides.
- **ASLR improves security but removes stable addresses.** Never build debugging or
  serialization assumptions around exact code addresses across runs.

## In practice

- **Use `readelf -r`, `objdump -R`, and `objdump -d` on ELF.** Relocations explain the holes
  you see in disassembly.
- **Use `otool -L`, `otool -l`, and dyld diagnostics on macOS.** Mach-O has different names,
  but the loader questions are the same.
- **Compile shared-library code as PIC.** It keeps relocations in data tables instead of
  forcing code patches.
- **Turn on eager binding when debugging loader failures.** `LD_BIND_NOW=1` on Linux makes
  lazy function-resolution bugs appear at startup.
- **Keep PLT/GOT as mechanisms, not magic.** PLT is for calls, GOT is for runtime addresses,
  relocations are the patch list that makes both concrete.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/toolchain-and-linking/dynamic-linking-and-shared-libraries|Dynamic linking and shared libraries]] · [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|The ELF format]] · [[lowlevel/toolchain-and-linking/object-files-and-whats-inside-them|Object files and what's inside them]] · [[lowlevel/machine-model/how-source-becomes-execution|How source becomes execution]]

## Sources

- **System V ABI — ELF specification** — relocation entries, program headers, dynamic linking structures, and symbol tables. https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html
- **Ulrich Drepper — *How To Write Shared Libraries*** — PIC, GOT, PLT, symbol lookup, relocation, and loader performance/security trade-offs. https://akkadia.org/drepper/dsohowto.pdf
- **Linux `ld.so(8)` manual page** — runtime dynamic loader behavior, search paths, lazy/eager binding controls, and environment variables. https://man7.org/linux/man-pages/man8/ld.so.8.html
- **GNU binutils — `objdump` manual** — disassembly and relocation inspection options used to connect bytes to relocation records. https://sourceware.org/binutils/docs/binutils/objdump.html
- **Ian Lance Taylor — *Linkers* series** — linker/loader implementation explanations for relocation and dynamic linking. https://www.airs.com/blog/archives/38
- **Apple `dyld` manual page** — Mach-O dynamic loader behavior, path expansion, and runtime binding vocabulary. https://keith.github.io/xcode-man-pages/dyld.1.html
