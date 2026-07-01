---
title: "Object files and what's inside them"
description: A relocatable object file is the linker's input: machine code, data, symbol tables, relocation records, and metadata that are real but not yet a runnable program.
tags: [toolchain, object-files, symbols, relocations, linker]
order: 2
updated: 2026-07-01
---
# Object files and what's inside them

A `.o` file is not "half an executable." It is a **relocatable object file**: a container
with machine code, data, symbol tables, relocation records, and metadata that the linker can
combine with other objects. The previous note showed where `.o` appears in the pipeline;
this note opens the box. The key reset is that an object file can contain real instructions
and still contain holes, because some addresses and definitions are intentionally left for
the linker.

> The reset: compiling with `-c` produces a linkable artifact, not a runnable artifact. The
> CPU cannot execute a random `.o` directly; the linker still has to resolve symbols, apply
> relocations, and choose a final layout.

## How it really works

An object file is organized into named regions plus tables that describe those regions.
The exact format is platform-specific: Linux uses ELF, macOS uses Mach-O, Windows uses
COFF/PE. The model is shared enough that `nm`, `objdump`, `readelf`, and `otool` all answer
the same questions:

| Part | What it contains | Why the linker cares |
|---|---|---|
| Text/code section | encoded machine instructions | bytes to place in executable code |
| Data section | initialized writable globals | bytes to place in writable memory |
| BSS-like section | zero-initialized globals | size to reserve without storing zeros in the file |
| Read-only constants | string literals, `const` objects, jump tables | bytes that should not be writable at runtime |
| Symbol table | names defined here and names referenced elsewhere | the linker's global matching table |
| Relocation records | places whose final value is not known yet | patches the linker must apply after layout |
| Metadata | debug info, unwind info, comments, build attributes | tools, debuggers, stack unwinding, diagnostics |

The assembler emits section-relative addresses because the final address is not known yet.
If `main.o` contains a call to `scale`, the instruction bytes cannot contain the final
address of `scale`; that address depends on where `support.o` lands in the final output.
So the object file records a relocation: "at this offset in this section, write the final
address or displacement for this symbol using this relocation kind."

That is the central difference between **bytes** and **meaning**. The `.text` section
contains bytes that the CPU can eventually execute. The symbol table says which names those
bytes define. The relocation table says which bytes are placeholders. The linker needs all
three: raw contents, names, and patch instructions.

## Sections, symbols, relocations

Think of a relocatable object as three linked views:

| View | Question it answers | Tooling |
|---|---|---|
| Sections | "What buckets of bytes are in this file?" | `objdump -h`, `readelf -S`, `otool -l` |
| Symbols | "Which names are defined here or still missing?" | `nm`, `objdump -t`, `readelf -s` |
| Relocations | "Which offsets must the linker patch?" | `objdump -r`, `readelf -r`, `otool -r` |

Sections are not the same thing as source-language concepts. A single C file may produce
multiple sections: code in `__text`/`.text`, initialized globals in `__data`/`.data`,
zero-initialized storage in common/BSS-like areas, string literals in `__cstring` or
`.rodata`, unwind metadata in `__eh_frame`, and debug sections when you compile with `-g`.
The names vary by object format; the job is the same.

Symbols are names attached to sections or unresolved names that point outside the file.
With `nm`, uppercase letters usually mean external/global symbols, lowercase letters mean
local symbols, and `U` means undefined. In the example below, `_compute` and
`_global_counter` are provided by `main.o`; `_scale` and `_printf` are holes that the link
step must fill.

Relocations are why object files are movable. A function can be compiled before the linker
knows where the function, a global, or a string literal will live. The relocation table
keeps enough information for the linker to patch calls, loads, and addresses after it has
chosen the final layout.

## Executable artifact: inspect a relocatable object

The example lives in
`examples/toolchain-and-linking/object-files-and-whats-inside-them/`. It compiles two C
files into two object files, opens `main.o`, then links and runs the final program.

`main.c` deliberately contains code, initialized data, zero-initialized data, a string
constant, a private helper, and an external call:

```c
#include <stdio.h>

extern int scale(int value);

int global_counter = 7;
int zero_counter;
const char banner[] = "object";

static int local_bias(void) {
    return 3;
}

int compute(void) {
    zero_counter = scale(global_counter + local_bias());
    return zero_counter;
}

int main(void) {
    printf("%s result: %d\n", banner, compute());
    return 0;
}
```

Run it:

```bash
cd examples/toolchain-and-linking/object-files-and-whats-inside-them
./run.sh
```

Real output from this machine:

```text
== compile to relocatable object files ==
== file main.o ==
main.o: Mach-O 64-bit object x86_64
== sections in main.o ==

main.o:	file format mach-o 64-bit x86-64

Sections:
Idx Name             Size     VMA              Type
  0 __text           00000083 0000000000000000 TEXT
  1 __data           00000004 0000000000000084 DATA
  2 __const          00000007 0000000000000088 DATA
  3 __cstring        0000000f 000000000000008f DATA
  4 __compact_unwind 00000060 00000000000000a0 DATA
  5 __eh_frame       00000090 0000000000000100 DATA
== symbols in main.o ==
0000000000000088 S _banner
0000000000000000 T _compute
0000000000000084 D _global_counter
0000000000000040 t _local_bias
0000000000000050 T _main
                 U _printf
                 U _scale
0000000000000004 C _zero_counter
== relocations in main.o ==

main.o:	file format mach-o 64-bit x86-64

RELOCATION RECORDS FOR [__text]:
OFFSET           TYPE                     VALUE
0000000000000077 X86_64_RELOC_BRANCH      _printf
0000000000000070 X86_64_RELOC_SIGNED      _banner
0000000000000069 X86_64_RELOC_SIGNED      __cstring
0000000000000060 X86_64_RELOC_BRANCH      _compute
000000000000002e X86_64_RELOC_GOT_LOAD    _zero_counter@GOTPCREL
0000000000000025 X86_64_RELOC_GOT_LOAD    _zero_counter@GOTPCREL
000000000000001c X86_64_RELOC_BRANCH      _scale
0000000000000012 X86_64_RELOC_BRANCH      _local_bias
000000000000000a X86_64_RELOC_SIGNED      _global_counter

RELOCATION RECORDS FOR [__compact_unwind]:
OFFSET           TYPE                     VALUE
0000000000000040 X86_64_RELOC_UNSIGNED    __text
0000000000000020 X86_64_RELOC_UNSIGNED    __text
0000000000000000 X86_64_RELOC_UNSIGNED    __text
== link and run ==
object result: 20
```

This run is on macOS, so the object format is Mach-O and C symbols have leading
underscores. On Linux you would see ELF section names such as `.text`, `.data`, `.bss`,
`.rodata`, `.symtab`, `.strtab`, and relocation sections such as `.rela.text`. The
inspection questions are identical: what bytes exist, what names exist, and which bytes
still need linker patches.

## Failure modes & trade-offs

- **A `.o` is not runnable.** It has no process entry contract, no final layout, and often
  unresolved references. Run the linked executable, not the relocatable input.
- **Architecture and ABI must match.** A Mach-O x86-64 object cannot be linked into an ELF
  x86-64 executable, and an ARM64 object cannot be linked into an x86-64 program. The
  machine code, relocation types, calling convention, and container format all matter.
- **Stale objects create fake bugs.** If the header changed but one `.o` was not rebuilt,
  the linker may combine incompatible assumptions. Build systems exist largely to avoid
  stale object files.
- **Debug and unwind metadata are not noise.** Stripping them can shrink output, but it can
  also remove debugger visibility or hurt stack traces. Treat size and observability as a
  trade-off.
- **Relocation type errors are real target errors.** "Relocation truncated" or "unsupported
  relocation" means the code model, target, or link layout cannot represent the address in
  the requested form.

## In practice

- **Start with `file`.** It tells you whether you are holding ELF, Mach-O, COFF, the right
  architecture, and a relocatable object vs executable.
- **Use `nm` before reading raw bytes.** Defined vs undefined symbols usually explains link
  failures faster than disassembly does.
- **Use section headers to orient yourself.** If `.text` is tiny but debug sections are
  huge, you are looking at metadata. If `.bss` is large but file size is small, that is
  zero-filled storage doing its job.
- **Use relocations to understand "holes."** They are the exact list of places the linker
  must patch; unresolved symbols are not vague, they have offsets and relocation kinds.
- **Keep the object-format boundary clear.** This note is about relocatable object files in
  general. The next note zooms into [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|ELF]] specifically.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link|The pipeline: preprocess → compile → assemble → link]] · [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|The ELF format]] · [[lowlevel/toolchain-and-linking/symbols-definition-reference-and-resolution|Symbols: definition, reference, and resolution]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declarations vs definitions, and linkage]]

## Sources

- **GCC Manual — "Overall Options" (`-c`)** — object files as the output when compilation/assembly stops before linking. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
- **GNU binutils — `objdump` manual** — section headers, symbol tables, and relocation inspection from object files. https://sourceware.org/binutils/docs/binutils/objdump.html
- **GNU binutils — `nm` manual** — symbol class letters such as `T`, `D`, `C`, lowercase locals, and `U`. https://sourceware.org/binutils/docs/binutils/nm.html
- **System V ABI — ELF specification** — the canonical model for ELF object files, sections, symbols, and relocations. https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html
- **Ian Lance Taylor — *Linkers* series** — linker-oriented explanation of why object files exist and what linkers consume. https://www.airs.com/blog/archives/38
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 7 — object files, relocations, and linking as a programmer-visible system. https://csapp.cs.cmu.edu/
