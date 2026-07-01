---
title: "Dynamic linking y shared libraries (`.so`)"
description: Las shared libraries mantienen código de biblioteca fuera del ejecutable: el linker registra dependencias, y el dynamic loader las mapea y resuelve en runtime.
tags: [toolchain, linker, dynamic-linking, shared-libraries, loader]
order: 6
updated: 2026-06-22
---
# Dynamic linking y shared libraries (`.so`)

El dynamic linking (linking dinámico) mueve parte del linking desde build time a load time.
El ejecutable no contiene una copia privada de cada función de biblioteca; registra que
necesita una shared library, y el dynamic loader mapea esa biblioteca dentro del proceso
cuando el programa arranca. En Linux/ELF el archivo suele ser un `.so`; en macOS/Mach-O es
un `.dylib`. El beneficio es código compartido, ejecutables más chicos y flexibilidad de
updates. El costo es un grafo de dependencias de runtime que puede fallar después de que el
link ya tuvo éxito.

> El reset: static linking copia código seleccionado dentro del ejecutable. Dynamic linking
> registra un contrato: "en runtime, cargá este shared object y bindeá estos símbolos."

## Cómo funciona realmente

Una shared library es un binario loadable diseñado para mapearse dentro de muchos procesos.
Exporta dynamic symbols, contiene metadata de relocation y se construye para que su código
pueda correr sin importar dónde lo mapee el loader. Por eso los objects C para shared
libraries normalmente se compilan con `-fPIC`: position-independent code (código
independiente de posición) evita direcciones absolutas hardcodeadas y usa direccionamiento
amigable a relocations mediante tablas como la GOT en ELF.

En Linux, la forma común de build es:

```bash
gcc -O2 -Wall -Wextra -fPIC -c greeting.c -o greeting.o
gcc -shared -Wl,-soname,libgreeting.so.1 greeting.o -o libgreeting.so.1.0
gcc main.c -L. -lgreeting -Wl,-rpath,'$ORIGIN' -o demo
```

El static linker igual corre. Verifica que las referencias undefined del ejecutable puedan
satisfacerse con símbolos exportados por la shared library, y después escribe metadata de
dynamic linking en el ejecutable en vez de copiar todo el código de biblioteca. En ELF, esa
metadata incluye entradas como `DT_NEEDED`, tablas de símbolos para lookup dinámico,
registros de relocation y muchas veces un path de interpreter como
`/lib64/ld-linux-x86-64.so.2`.

La shared library también tiene identidad. En ELF, el **SONAME** es el nombre de
compatibilidad que registran los consumidores, como `libgreeting.so.1`, mientras que el
archivo real puede ser `libgreeting.so.1.0.3`. En macOS, la idea análoga es el install name,
visible con `otool -L`, a menudo usando `@rpath`, `@loader_path` o `@executable_path`.

## Runtime resolution

En el launch del programa, el kernel carga el ejecutable y le pasa el control al dynamic
loader nombrado en el binario. El loader lee la lista de dependencias, encuentra cada shared
library mediante search paths configurados, mapea segments con permisos correctos, aplica
relocations requeridas y resuelve símbolos de forma inmediata o lazy según plataforma y
modo de binding. La próxima nota baja más a relocation, PLT, GOT y lazy binding; acá el
borde importante es que la dependencia de biblioteca sobrevive hasta runtime.

Dynamic linking también habilita sharing. Si diez procesos mapean las mismas páginas
read-only de código desde `libc.so`, el OS puede mantener una copia física de esas páginas
y mapearla en cada proceso. Los datos writables siguen siendo por proceso porque cada
proceso necesita su propio estado.

## Artefacto ejecutable: construí una shared library

El ejemplo vive en
`examples/toolchain-and-linking/dynamic-linking-and-shared-libraries/`. Construye una
`.dylib` nativa de macOS porque esta máquina puede correr binarios Mach-O, pero el fuente y
los flags mapean directamente a la forma Linux `.so` de arriba.

`greeting.c` se vuelve la shared library:

```c
#include "greeting.h"

int greeting_value(void) {
    return 42;
}

const char *greeting_label(void) {
    return "shared-library";
}
```

`main.c` linkea contra ella:

```c
#include <stdio.h>

#include "greeting.h"

int main(void) {
    printf("%s value=%d\n", greeting_label(), greeting_value());
    return 0;
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/dynamic-linking-and-shared-libraries
./run.sh
```

Salida real de esta máquina:

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

La línea importante es `@rpath/libgreeting.dylib`: el ejecutable registra una dependencia
de runtime, no una implementación copiada. El comando de link agrega
`-Wl,-rpath,@loader_path`, así el loader puede encontrar la biblioteca junto al ejecutable.
En Linux/ELF, la inspección equivalente suele ser `ldd ./demo` o `readelf -d ./demo`, y el
patrón equivalente de token de búsqueda suele ser `$ORIGIN`.

## Modos de falla y trade-offs

- **Biblioteca encontrada en link time, faltante en runtime.** El ejecutable puede linkear
  bien y después fallar con `cannot open shared object file` en Linux o `Library not loaded`
  en macOS si el loader no encuentra la biblioteca.
- **ABI drift.** Si una shared library cambia funciones exportadas, layout de structs,
  supuestos de calling convention o symbol versions sin preservar compatibilidad, los
  ejecutables viejos pueden romperse.
- **PIC tiene un costo chico.** Position-independent code puede requerir indirección extra,
  aunque x86-64 moderno hace que el costo sea mucho menor que en modelos viejos con
  direcciones absolutas.
- **Los updates son centralizados.** Parchear una shared library puede arreglar muchos
  programas, pero un mal update también puede romper muchos programas.
- **Symbol interposition es poder y peligro.** Dynamic linking puede permitir override
  hooks, mecanismos de preload y colisiones accidentales. Los controles de visibility
  importan.
- **Deploy se vuelve un problema de grafo.** Enviás un ejecutable más las bibliotecas
  compatibles y loader paths correctos, no un archivo sellado.

## En la práctica

- **Usá shared libraries para código común y actualizable.** libc, frameworks GUI,
  bibliotecas de crypto y runtimes de plataforma son dependencias dinámicas naturales.
- **Usá `-fPIC` para objects de biblioteca.** Tratálo como default para código que puede
  volverse `.so` o `.dylib`.
- **Inspeccioná dependencias dinámicas.** Usá `readelf -d`, `ldd`, `objdump -p` u
  `otool -L` antes de adivinar por qué un programa no lanza.
- **Nombrá compatibilidad deliberadamente.** En ELF, seteá un SONAME para shared libraries
  versionadas. En macOS, entendé install names y `@rpath`.
- **Separá los modelos static y dynamic.** Los static archives se buscan y extraen por el
  linker de build time; las shared libraries se mapean y resuelven por el runtime loader.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/toolchain-and-linking/static-linking-and-archives|Static linking y archives]] · [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|El formato ELF]] · [[lowlevel/toolchain-and-linking/symbols-definition-reference-and-resolution|Símbolos: definición, referencia y resolución]] · [[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]]

## Fuentes

- **Ulrich Drepper — *How To Write Shared Libraries*** — tratamiento profundo de ELF/Linux sobre PIC, symbol visibility, relocation y diseño de shared libraries. https://akkadia.org/drepper/dsohowto.pdf
- **Manual de GNU `ld`** — `-shared`, `-soname`, dynamic sections, rpath y comportamiento del linker. https://sourceware.org/binutils/docs/ld/
- **GCC Manual — code generation options** — `-fpic`/`-fPIC` y position-independent code. https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html
- **Linux `ld.so(8)` manual page** — comportamiento del dynamic loader de runtime y search paths. https://man7.org/linux/man-pages/man8/ld.so.8.html
- **Apple `dyld` manual page** — dynamic loader de macOS, install names y path tokens. https://keith.github.io/xcode-man-pages/dyld.1.html
- **Ian Lance Taylor — serie *Linkers*** — dynamic linking y resolución de símbolos desde la perspectiva de implementación de un linker. https://www.airs.com/blog/archives/38
