---
title: "El formato ELF (sections, segments, headers)"
description: ELF es el formato de objetos de Linux/Unix cuyos headers describen dos vistas de un binario: sections para el linker y segments para el loader.
tags: [toolchain, elf, object-files, sections, segments, loader]
order: 3
updated: 2026-07-01
---
# El formato ELF (sections, segments, headers)

ELF no es solo "el formato de ejecutables de Linux." Es una familia de tipos de archivo para
objects relocatable (`.o`), ejecutables, shared libraries (`.so`) y core dumps. Su idea
central es que un binario puede exponer dos mapas distintos: **sections** para herramientas
de link time y **segments** para el loader de runtime. Si mantenés separadas esas dos vistas,
ELF deja de parecer una pila de headers y empieza a verse como un contrato entre compilador,
assembler, linker, loader, debugger y OS.

> El reset: sections responde "¿qué produjo y qué necesita combinar el linker?"; segments
> responde "¿qué debería mapear el loader en memoria?" Los `.o` relocatable son sobre todo
> una historia de sections. Ejecutables y shared libraries agregan la historia de segments.

## Cómo funciona realmente

Todo archivo ELF empieza con un ELF header. Ese header identifica el archivo como ELF y les
dice a las herramientas cómo interpretar el resto: 32-bit vs 64-bit, orden endian, arquitectura
de máquina, tipo de archivo, dirección de entry cuando existe, y offsets de la section header
table y la program header table.

La section header table es la vista fina del linker. Las sections dividen el archivo en
buckets nombrados como `.text`, `.rodata`, `.data`, `.bss`, `.symtab`, `.strtab`,
`.rela.text`, `.debug_info` y `.eh_frame`. Un `.o` relocatable depende de sections porque
el linker debe mergearlas, descartarlas, relocalizarlas y reescribirlas.

La program header table es la vista gruesa del loader. Los program headers describen
segments como `LOAD`, `DYNAMIC`, `INTERP`, `TLS` y `GNU_STACK`. Un segment puede contener
muchas sections. Al loader no le importa que `.text` y `.rodata` fueran buckets separados
de origen; le importa qué rangos de bytes mapear, en qué direcciones virtuales y con qué
permisos.

| Vista ELF | Tabla principal | Usada por | Pregunta típica |
|---|---|---|---|
| Identidad del archivo | ELF header | toda herramienta | "¿Qué clase de archivo ELF es este?" |
| Vista de link time | Section headers | assembler, linker, debugger | "¿Dónde están `.text`, símbolos, relocations, debug info?" |
| Vista de runtime | Program headers | loader del kernel, dynamic linker | "¿Qué rangos se vuelven memory mappings?" |

Por eso `strip` puede quitar muchas section headers o debug sections de un binario final sin
volverlo inlanzable: el loader sigue program headers. También por eso un `.o` relocatable
puede tener section tables ricas pero ningún program header significativo: no está listo
para cargarse como imagen de proceso.

## Sections vs segments

Las sections son precisas y numerosas. Preservan información que linkers y tooling
necesitan: código, constantes, datos writables, datos zero-filled, registros de relocation,
tablas de símbolos, string tables, unwind info y debug info. Los nombres de section son
convenciones, pero el tipo y los flags de la section cargan el significado mecánico.

Los segments son pocos y orientados a páginas. Un ejecutable típico tiene segments loadable
como text read/execute y data read/write, más metadata de dynamic linking. Los permisos del
segment se mapean a protección de memoria virtual: el código ejecutable no debería ser
writable; los datos writables no deberían ser executable.

El linker es el puente. Toma muchas input sections de muchos object files relocatable,
resuelve símbolos y relocations, elige un layout y emite output sections. Después agrupa
esas output sections en segments loadable para el loader del OS. Ese agrupamiento es donde
la organización de object files se vuelve layout de memoria de proceso.

## Artefacto ejecutable: emití un object ELF en cualquier host con Clang

El ejemplo vive en
`examples/toolchain-and-linking/the-elf-format-sections-segments-headers/`. Evita libc y
compila solo a object relocatable, así Clang puede emitir Linux/ELF incluso en macOS:

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

Corrélo:

```bash
cd examples/toolchain-and-linking/the-elf-format-sections-segments-headers
./run.sh
```

Salida real de esta máquina:

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

Este object es ELF64, little-endian, x86-64, relocatable y not stripped. Sus section
headers muestran exactamente los buckets esperados: `.text` para código, `.data` para
storage writable inicializado, `.bss` para storage zero-filled, `.rodata` para constantes,
`.symtab` y `.strtab` para símbolos y nombres, y `.rela.text` para registros de relocation
asociados al código.

No hay runtime segments en este ejemplo porque es una entrada relocatable al linker, no un
ejecutable. Una vez linkeado dentro de un ejecutable ELF o shared library, `readelf -l` u
`objdump -p` mostraría program headers: el mapa que mira el loader. Ese es el momento donde
sections como `.text` y `.rodata` se agrupan en segments `LOAD` alineados a páginas.

## Modos de falla y trade-offs

- **Confundir sections con segments lleva a debuggear mal.** Si un binario lanza, el loader
  tuvo suficiente información de program headers. Que falten nombres de section puede dañar
  herramientas, no necesariamente la ejecución.
- **Los objects relocatable no son process images.** Un `.o` puede tener `.text`, `.data`,
  símbolos y relocations pero no entry point ni layout de segments loadable.
- **ELF no es universal.** macOS usa Mach-O y Windows usa PE/COFF. Los conceptos se
  transfieren; byte layout, nombres de headers, tipos de relocation y flags de herramientas
  no.
- **Stripping es un trade-off entre tamaño y observabilidad.** Quitar debug sections y
  nombres de símbolos puede achicar artefactos de distribución, pero vuelve más difícil el
  debugging postmortem.
- **Los permisos vienen del layout de segments.** La disciplina W^X depende de que código y
  datos writables caigan en segments con permisos correctos de página, no de intención a
  nivel fuente C.
- **Los target triples importan.** `x86_64-unknown-linux-gnu` le dice a Clang que emita
  relocations ELF estilo Linux y decisiones de ABI; cambiar el target cambia el contrato del
  object.

## En la práctica

- **Leé primero el ELF header.** `file`, `readelf -h` u `objdump -f` te dice class, machine,
  type, y si estás mirando un object relocatable, ejecutable o shared object.
- **Usá sections para problemas de link.** Si falta un símbolo, relocation o debug record,
  inspeccioná section y symbol tables.
- **Usá segments para problemas de load.** Si el ejecutable mapea raro, falla al lanzar o
  tiene permisos sospechosos, inspeccioná program headers.
- **Esperá que `.bss` sea tamaño, no ceros guardados.** El archivo registra cuánta memoria
  zero-filled reservar; no gasta bytes de disco guardando largas tiras de ceros.
- **Atá ELF al proyecto de OS.** Un kernel freestanding x86-64 eventualmente necesita
  entender qué emite su linker script y qué van a cargar QEMU/boot code.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/toolchain-and-linking/object-files-and-whats-inside-them|Object files y qué tienen adentro]] · [[lowlevel/toolchain-and-linking/symbols-definition-reference-and-resolution|Símbolos: definición, referencia y resolución]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] · [[lowlevel/os-from-scratch/index|OS desde Cero]]

## Fuentes

- **System V ABI — especificación ELF** — definición canónica de ELF headers, sections, program headers, símbolos y relocations. https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html
- **Linux `elf(5)` manual page** — vista práctica de Linux sobre tipos de archivo ELF, headers, section headers y program headers. https://man7.org/linux/man-pages/man5/elf.5.html
- **GNU binutils — manual de `objdump`** — referencia de comandos para inspeccionar headers, sections, símbolos y relocations. https://sourceware.org/binutils/docs/binutils/objdump.html
- **Ian Lance Taylor — serie *Linkers*** — explicación desde un autor de linker sobre formatos de object y por qué los linkers necesitan esta estructura. https://www.airs.com/blog/archives/38
- **John R. Levine — *Linkers and Loaders*** — tratamiento largo de object formats, linkers, loaders y relocation. https://linker.iecc.com/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 7 — puente legible entre archivos ELF y process loading. https://csapp.cs.cmu.edu/
