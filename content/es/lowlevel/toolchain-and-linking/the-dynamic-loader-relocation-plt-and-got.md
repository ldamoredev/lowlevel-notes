---
title: "El dynamic loader, relocation, PLT y GOT"
description: El dynamic loader mapea shared libraries, aplica relocations y usa estructuras como GOT y PLT para que position-independent code pueda llamar y acceder a símbolos externos.
tags: [toolchain, dynamic-loader, relocation, plt, got, pie]
order: 7
updated: 2026-06-22
---
# El dynamic loader, relocation, PLT y GOT

La nota anterior de ejecución mostró la historia amplia load → run. Esta hace zoom en la
maquinaria de dynamic linking: el loader mapea shared libraries, arregla direcciones que no
se conocían en static link time y deja tablas que el código puede usar sin saber dónde cayó
la biblioteca. En ELF, los dos nombres que conviene separar son la **GOT** (Global Offset
Table, tabla global de offsets) para direcciones de datos/funciones externas y la **PLT**
(Procedure Linkage Table, tabla de enlace de procedimientos) para llamadas que pueden
resolverse mediante el dynamic linker. PIE y ASLR vuelven esta maquinaria normal: el código
se construye para moverse, y después se relocaliza dentro de un layout real de proceso.

> El reset: el static linker escribe "lugares a arreglar" dentro del binario. El dynamic
> loader elige direcciones de runtime, mapea objects y aplica esos arreglos antes o durante
> las llamadas.

## Cómo funciona realmente

Un ejecutable o shared library puede contener referencias cuyo valor final es imposible de
conocer en static link time. Un shared object puede mapearse en una dirección virtual
distinta en cada proceso. Una función puede venir de otro `.so`. Una variable puede ser
interposable. Por eso el binario carga entradas de relocation: instrucciones tipadas que
dicen "escribí la dirección o desplazamiento de runtime para este símbolo en este offset."

Position-independent code hace manejables esos parches de runtime. En vez de hornear una
dirección absoluta en cada instrucción, el código suele usar direccionamiento PC-relative y
tablas. En ELF, la GOT contiene slots para direcciones que el loader puede llenar. El código
carga a través de un slot de GOT cuando necesita un objeto externo o un function pointer. La
PLT es un conjunto de stubs llamables usados para llamadas a funciones dinámicamente
linkeadas; una entrada PLT puede rebotar por el dynamic linker la primera vez que se llama a
una función y después saltar directo por el slot GOT resuelto.

| Mecanismo | Trabajo |
|---|---|
| Entrada de relocation | describe un parche de runtime que el loader o linker debe aplicar |
| GOT | guarda direcciones de runtime usadas por position-independent code |
| PLT | provee stubs de llamada para funciones dinámicamente linkeadas |
| Lazy binding | resuelve una función en la primera llamada en vez de al startup |
| PIE | hace que el ejecutable principal también sea position-independent |
| ASLR | deja que el OS randomice direcciones de mapping entre launches |

No toda relocation es lazy. Relocations de datos y algunos modos de seguridad requieren
trabajo eager en startup. Linux puede forzar binding eager de funciones con `LD_BIND_NOW=1`,
y builds hardened suelen combinar binding eager con RELRO para que partes de la GOT queden
read-only después de relocation. Los detalles cambian entre loaders, pero la forma es la
misma: mapear objects, resolver nombres, parchear direcciones y después correr código de
usuario.

## Loader sequence

En Linux/ELF, el kernel mapea el ejecutable principal y arranca el interpreter nombrado en
los program headers ELF, comúnmente `ld-linux-x86-64.so.2`. El dynamic loader lee
dependencias `DT_NEEDED`, las encuentra y mapea, calcula direcciones de carga, procesa
relocations, corre initializers y finalmente transfiere control al camino de entry del
programa. En macOS, `dyld` hace el trabajo análogo para imágenes Mach-O, install names,
opcodes de bind/rebase y resolución de `@rpath`.

Esto está separado del camino amplio source-to-execution de
[[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]].
Esa nota nombra el pipeline y el launch de proceso. Esta nombra las estructuras internas
del paso de dynamic link: registros de relocation, slots de GOT, stubs PLT, search paths del
loader y runtime binding.

## Artefacto ejecutable: corré e inspeccioná relocations

El ejemplo vive en
`examples/toolchain-and-linking/the-dynamic-loader-relocation-plt-and-got/`. Primero
construye y corre una `.dylib` nativa para que el loader path sea real en esta máquina.
Después emite un object PIC Linux/ELF con cross target y usa `objdump` para mostrar registros
de relocation que nombran trabajo estilo GOT/PLT.

`main.c` llama una función desde una shared library:

```c
#include <stdio.h>

#include "plugin.h"

int main(void) {
    printf("loader result=%d\n", plugin_scale(5));
    return 0;
}
```

`elf_pic.c` evita libc a propósito para que Clang pueda emitir Linux/ELF en este host macOS:

```c
extern int external_data;
extern int external_call(int value);

int use_external(void) {
    return external_data + external_call(7);
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/the-dynamic-loader-relocation-plt-and-got
./run.sh
```

Salida real de esta máquina:

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

La tabla de relocation te dice qué significan los placeholders en cero del disassembly. La
carga a través de `(%rip)` tiene una relocation `R_X86_64_REX_GOTPCRELX external_data`: el
linker/loader va a preparar una dirección vía GOT para datos externos. El placeholder de
`callq` tiene `R_X86_64_PLT32 external_call`: la llamada es candidata a binding estilo PLT
hacia una función externa. En un ejecutable o shared library completamente linkeado, esos
registros se vuelven parte de la historia de relocations y binding dinámico que consume el
loader.

## Modos de falla y trade-offs

- **Shared object faltante.** El static link tuvo éxito, pero el loader no encuentra una
  biblioteca `DT_NEEDED` o install name Mach-O en launch.
- **Relocation imposible de representar.** El code model, tipo de relocation o rango de
  direcciones es incompatible con el layout elegido.
- **Text relocations dañan seguridad y sharing.** Parchear páginas de código ejecutable
  puede requerir text writable e impedir compartir páginas limpiamente. PIC existe en gran
  parte para evitar eso.
- **Lazy binding mueve la falla más tarde.** El programa puede arrancar y fallar recién
  cuando se llama por primera vez una función faltante. Binding eager encuentra más problemas
  en startup.
- **Interposition cambia significado.** Un símbolo puede resolverse a una definición
  distinta de la esperada porque las reglas de dynamic lookup permiten overrides.
- **ASLR mejora seguridad pero quita direcciones estables.** Nunca construyas supuestos de
  debugging o serialización alrededor de direcciones exactas de código entre corridas.

## En la práctica

- **Usá `readelf -r`, `objdump -R` y `objdump -d` en ELF.** Las relocations explican los
  huecos que ves en el disassembly.
- **Usá `otool -L`, `otool -l` y diagnostics de dyld en macOS.** Mach-O tiene nombres
  distintos, pero las preguntas sobre loader son las mismas.
- **Compilá código de shared libraries como PIC.** Mantiene relocations en tablas de datos
  en vez de forzar parches de código.
- **Activá binding eager al debuggear fallas de loader.** `LD_BIND_NOW=1` en Linux hace que
  bugs de resolución lazy de funciones aparezcan en startup.
- **Mantené PLT/GOT como mecanismos, no magia.** PLT es para llamadas, GOT es para
  direcciones de runtime, y relocations son la lista de parches que vuelve ambas concretas.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/toolchain-and-linking/dynamic-linking-and-shared-libraries|Dynamic linking y shared libraries]] · [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|El formato ELF]] · [[lowlevel/toolchain-and-linking/object-files-and-whats-inside-them|Object files y qué tienen adentro]] · [[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]]

## Fuentes

- **System V ABI — especificación ELF** — entradas de relocation, program headers, estructuras de dynamic linking y tablas de símbolos. https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html
- **Ulrich Drepper — *How To Write Shared Libraries*** — PIC, GOT, PLT, symbol lookup, relocation y trade-offs de performance/seguridad del loader. https://akkadia.org/drepper/dsohowto.pdf
- **Linux `ld.so(8)` manual page** — comportamiento del dynamic loader de runtime, search paths, controles de binding lazy/eager y environment variables. https://man7.org/linux/man-pages/man8/ld.so.8.html
- **GNU binutils — manual de `objdump`** — opciones de disassembly e inspección de relocations usadas para conectar bytes con registros de relocation. https://sourceware.org/binutils/docs/binutils/objdump.html
- **Ian Lance Taylor — serie *Linkers*** — explicaciones de implementación de linker/loader para relocation y dynamic linking. https://www.airs.com/blog/archives/38
- **Apple `dyld` manual page** — comportamiento del dynamic loader Mach-O, expansión de paths y vocabulario de runtime binding. https://keith.github.io/xcode-man-pages/dyld.1.html
