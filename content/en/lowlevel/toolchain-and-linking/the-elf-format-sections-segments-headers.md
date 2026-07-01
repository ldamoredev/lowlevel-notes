---
title: "The ELF format (sections, segments, headers)"
description: ELF is the Linux/Unix object format whose headers describe two views of a binary: sections for the linker and segments for the loader.
tags: [toolchain, elf, object-files, sections, segments, loader]
order: 3
updated: 2026-07-01
---
# The ELF format (sections, segments, headers)

ELF is not just "the Linux executable format." It is a family of file types for
relocatable objects (`.o`), executables, shared libraries (`.so`), and core dumps. Its
central idea is that one binary can expose two different maps: **sections** for link-time
tools and **segments** for the runtime loader. If you keep those two views separate, ELF
stops looking like a pile of headers and starts looking like a contract between compiler,
assembler, linker, loader, debugger, and OS.

> The reset: sections answer "what did the linker produce and need to combine?"; segments
> answer "what should the loader map into memory?" Relocatable `.o` files are mostly a
> section story. Executables and shared libraries add the segment story.

## How it really works

Every ELF file starts with an ELF header. That header identifies the file as ELF and tells
tools how to interpret the rest: 32-bit vs 64-bit, endian order, machine architecture, file
type, entry address when one exists, and the offsets of the section header table and program
header table.

The section header table is the linker's fine-grained view. Sections split the file into
named buckets such as `.text`, `.rodata`, `.data`, `.bss`, `.symtab`, `.strtab`,
`.rela.text`, `.debug_info`, and `.eh_frame`. A relocatable `.o` relies on sections because
the linker must merge, discard, relocate, and rewrite them.

The program header table is the loader's coarse-grained view. Program headers describe
segments such as `LOAD`, `DYNAMIC`, `INTERP`, `TLS`, and `GNU_STACK`. A segment may contain
many sections. The loader does not care that `.text` and `.rodata` were separate source
buckets; it cares which byte ranges to map, at which virtual addresses, with which
permissions.

| ELF view | Main table | Used by | Typical question |
|---|---|---|---|
| File identity | ELF header | every tool | "What kind of ELF file is this?" |
| Link-time view | Section headers | assembler, linker, debugger | "Where are `.text`, symbols, relocations, debug info?" |
| Runtime view | Program headers | kernel loader, dynamic linker | "Which ranges become memory mappings?" |

This is why `strip` can remove many section headers or debug sections from a final binary
without making it unlaunchable: the loader follows program headers. It is also why a
relocatable `.o` can have rich section tables but no meaningful program headers: it is not
ready to be loaded as a process image.

## Sections vs segments

Sections are precise and numerous. They preserve information that linkers and tooling need:
code, constants, writable data, zero-filled data, relocation records, symbol tables, string
tables, unwind info, and debug info. Section names are conventions, but the section type and
flags carry the mechanical meaning.

Segments are few and page-oriented. A typical executable has loadable segments such as
read/execute text and read/write data, plus dynamic-linking metadata. Segment permissions
map to virtual memory protection: executable code should not be writable; writable data
should not be executable.

The linker is the bridge. It takes many input sections from many relocatable objects,
resolves symbols and relocations, chooses a layout, and emits output sections. Then it
groups those output sections into loadable segments for the OS loader. That grouping is
where object-file organization becomes process memory layout.

## Executable artifact: emit an ELF object on any host with Clang

The example lives in
`examples/toolchain-and-linking/the-elf-format-sections-segments-headers/`. It avoids libc
and compiles only to a relocatable object, so Clang can emit Linux/ELF even on macOS:

```c
extern int external_scale(int value);

int initialized = 7;
int zeroed;
const char label[] = "ELF";

int answer(void) {
    return initialized + zeroed + label[0];
}

int call_external(void) {
    return external_scale(answer());
}
```

Run it:

```bash
cd examples/toolchain-and-linking/the-elf-format-sections-segments-headers
./run.sh
```

Real output from this machine:

```text
== compile a Linux/ELF relocatable object ==
== identify format ==
elf_demo.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
== section headers ==

elf_demo.o:	file format elf64-x86-64

Sections:
Idx Name            Size     VMA              Type
  0                 00000000 0000000000000000 
  1 .strtab         0000009e 0000000000000000 
  2 .text           00000032 0000000000000000 TEXT
  3 .rela.text      00000078 0000000000000000 
  4 .data           00000004 0000000000000000 DATA
  5 .rodata         00000004 0000000000000000 DATA
  6 .bss            00000004 0000000000000000 BSS
  7 .comment        0000002f 0000000000000000 
  8 .note.GNU-stack 00000000 0000000000000000 
  9 .llvm_addrsig   00000005 0000000000000000 
 10 .symtab         000000c0 0000000000000000 
== symbol table ==
SYMBOL TABLE:
0000000000000000 l    df *ABS*	0000000000000000 elf_demo.c
0000000000000000 g     F .text	000000000000001b answer
0000000000000000 g     O .data	0000000000000004 initialized
0000000000000000 g     O .bss	0000000000000004 zeroed
0000000000000000 g     O .rodata	0000000000000004 label
0000000000000020 g     F .text	0000000000000012 call_external
0000000000000000         *UND*	0000000000000000 external_scale
== relocations ==

elf_demo.o:	file format elf64-x86-64

RELOCATION RECORDS FOR [.text]:
OFFSET           TYPE                     VALUE
0000000000000006 R_X86_64_PC32            initialized-0x4
000000000000000c R_X86_64_PC32            zeroed-0x4
0000000000000013 R_X86_64_PC32            label-0x4
0000000000000025 R_X86_64_PLT32           answer-0x4
000000000000002c R_X86_64_PLT32           external_scale-0x4
== note ==
This is a relocatable ELF object. Executables and shared libraries add program headers/segments.
```

This object is ELF64, little-endian, x86-64, relocatable, and not stripped. Its section
headers show exactly the buckets you expect: `.text` for code, `.data` for initialized
writable storage, `.bss` for zero-filled storage, `.rodata` for constants, `.symtab` and
`.strtab` for symbols and names, and `.rela.text` for relocation records attached to code.

There are no runtime segments in this example because it is a relocatable input to the
linker, not an executable. Once linked into an ELF executable or shared library, `readelf -l`
or `objdump -p` would show program headers: the loader-facing map. That is the moment
where sections like `.text` and `.rodata` are grouped into page-aligned `LOAD` segments.

## Failure modes & trade-offs

- **Confusing sections with segments leads to wrong debugging.** If a binary launches, the
  loader had enough program-header information. Missing section names may hurt tools, not
  necessarily execution.
- **Relocatable objects are not process images.** A `.o` can have `.text`, `.data`, symbols,
  and relocations but no entry point and no loadable segment layout.
- **ELF is not universal.** macOS uses Mach-O and Windows uses PE/COFF. Concepts transfer;
  byte layout, header names, relocation types, and tool flags do not.
- **Stripping is a size vs observability trade-off.** Removing debug sections and symbol
  names can shrink distribution artifacts, but it makes postmortem debugging harder.
- **Permissions come from segment layout.** W^X discipline depends on code and writable
  data landing in segments with correct page permissions, not on C source-level intent.
- **Target triples matter.** `x86_64-unknown-linux-gnu` tells Clang to emit Linux-style ELF
  relocations and ABI choices; changing the target changes the object contract.

## In practice

- **Read the ELF header first.** `file`, `readelf -h`, or `objdump -f` tells you class,
  machine, type, and whether you are looking at a relocatable object, executable, or shared
  object.
- **Use sections for link problems.** If a symbol, relocation, or debug record is missing,
  inspect section and symbol tables.
- **Use segments for load problems.** If the executable maps strangely, fails at launch, or
  has suspicious permissions, inspect program headers.
- **Expect `.bss` to be size, not stored zeros.** The file records how much zero-filled
  memory to reserve; it does not waste disk bytes storing long runs of zero.
- **Keep ELF knowledge tied to the OS project.** A freestanding x86-64 kernel eventually
  needs to understand what its linker script emits and what QEMU/boot code will load.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/toolchain-and-linking/object-files-and-whats-inside-them|Object files and what's inside them]] · [[lowlevel/toolchain-and-linking/symbols-definition-reference-and-resolution|Symbols: definition, reference, and resolution]] · [[lowlevel/assembly-and-compiler-output/index|Assembly & Compiler Output]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]

## Sources

- **System V ABI — ELF specification** — canonical definition of ELF headers, sections, program headers, symbols, and relocations. https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html
- **Linux `elf(5)` manual page** — practical Linux view of ELF file types, headers, section headers, and program headers. https://man7.org/linux/man-pages/man5/elf.5.html
- **GNU binutils — `objdump` manual** — command reference for inspecting headers, sections, symbols, and relocations. https://sourceware.org/binutils/docs/binutils/objdump.html
- **Ian Lance Taylor — *Linkers* series** — linker author's explanation of object formats and why linkers need this structure. https://www.airs.com/blog/archives/38
- **John R. Levine — *Linkers and Loaders*** — the long-form treatment of object formats, linkers, loaders, and relocation. https://linker.iecc.com/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, ch. 7 — readable bridge between ELF files and process loading. https://csapp.cs.cmu.edu/
