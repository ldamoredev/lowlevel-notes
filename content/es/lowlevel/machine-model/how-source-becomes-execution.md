---
title: "Cómo el fuente se vuelve ejecución"
description: El camino de un archivo .c a un proceso corriendo — preprocess, compile, assemble, link para producir un ejecutable en disco, y después load y run para volver ese archivo un proceso vivo. Las cuatro etapas de build, qué emite cada herramienta y cómo el loader del OS le da vida al programa.
tags: [machine-model, compilation, linking, loader, toolchain]
order: 11
updated: 2026-06-22
---
# Cómo el fuente se vuelve ejecución

Un lenguaje administrado difuminaba "correr mi código" en un solo botón. Por debajo, llegar
de texto a un proceso vivo es un **pipeline de etapas distintas**, cada una con su propia
herramienta, entrada y salida inspeccionable. Primero, cuatro etapas de **build** vuelven el
fuente un ejecutable en disco: *preprocess → compile → assemble → link*. Después dos pasos de
**runtime** vuelven ese archivo un proceso corriendo: *load → run*. Nada de esto es magia —
podés frenar el pipeline en cualquier etapa y mirar exactamente qué produjo. Entender las
etapas es lo que te deja leer un error de build, un error de linker y un crash como problemas
*distintos* en puntos *distintos*.

> El reset: `gcc hello.c` no es una acción; son cuatro herramientas disfrazadas produciendo
> un archivo ELF/Mach-O, que el OS después mapea en memoria y al que salta. "Compila" y
> "linkea" y "corre" son tres victorias separadas.

## Las cuatro etapas de build

`gcc` (o `clang`) es un *driver* que corre cuatro programas en secuencia. Cada flag de abajo
frena el pipeline una etapa antes para que puedas ver el intermedio:

| Etapa | Herramienta | In → Out | Qué hace | Frená en |
|---|---|---|---|---|
| Preprocess | `cpp` | `.c` → `.i` | expande `#include`, `#define`, `#if` | `gcc -E` |
| Compile | `cc1` | `.i` → `.s` | traduce C a assembly para la ISA target | `gcc -S` |
| Assemble | `as` | `.s` → `.o` | codifica el assembly en object de machine-code | `gcc -c` |
| Link | `ld` | `.o` + libs → exe | resuelve símbolos, dispone los segmentos | (final) |

El preprocessing es pura sustitución de texto — un `hello.c` de 6 líneas se vuelve ~560
líneas una vez que se pega `<stdio.h>`. La compilación es la etapa pesada, donde C se vuelve
[[lowlevel/machine-model/registers-and-the-isa|assembly]] específico de la ISA. El assembling
es una codificación casi mecánica a bytes. El linking cose las piezas en un solo archivo
ejecutable.

## Los object files tienen agujeros; el linker los llena

La salida de *assemble* es un **object file** (`.o`): machine code real, pero con
**referencias sin resolver** a cualquier cosa definida en otro lado. Mirá los símbolos de un
`hello.o` que llama a `printf`:

```
0000000000000000 T _answer     <- definido acá (sección Text)
0000000000000010 T _main       <- definido acá
                 U _printf      <- Undefined: un agujero a llenar en link time
```

`_answer` y `_main` están **definidos** (`T` = text/código); `_printf` está **sin definir**
(`U`) — el object file sabe que se lo llama pero no dónde vive. El **linking** resuelve cada
agujero de esos contra otros object files y librerías (acá, la librería estándar de C),
parchea las direcciones, dispone los segmentos finales y emite un único ejecutable sin
agujeros. La mecánica — resolución de símbolos, linking estático vs dinámico, secciones ELF,
relocation — es todo el tema de [[lowlevel/toolchain-and-linking/index|toolchain y linking]];
acá el punto es solo que un `.o` está *incompleto* y el linker lo completa.

## Del archivo al proceso: load → run

Un ejecutable en disco es inerte. Correrlo es trabajo del OS:

- **`exec` y el loader.** Cuando lanzás el archivo, el loader del kernel mapea sus segmentos a
  un [[lowlevel/machine-model/stack-vs-heap|address space]] fresco — código (text) y globals
  (data/bss) desde el archivo, más una región fresca de stack y heap.
- **El dynamic linker.** Si el programa usa librerías compartidas (`libc.so`), el dynamic
  linker (`ld.so`) las carga y conecta las llamadas (vía la PLT/GOT) para que `printf`
  resuelva al código real de la librería — de forma lazy, en la primera llamada.
- **Startup y `main`.** El loader arma el stack con `argv`/`environ`, después salta al entry
  point del runtime de C (`_start`), que inicializa el runtime y llama a `main`. Cuando `main`
  retorna, el runtime llama a `exit` y el proceso se desarma.

Así que "load → run" es: mapear el archivo en memoria, resolver los agujeros (dinámicos)
restantes, construir el stack inicial y saltar adentro. El mapa del address space que viste en
la nota de stack-vs-heap es exactamente lo que el loader construye.

## Recorrelo vos mismo

```sh
# pipeline.sh — mirá un .c pasar por cada etapa. (gcc o clang)
cat > demo.c <<'EOF'
#include <stdio.h>
int answer(void) { return 42; }
int main(void) { printf("the answer is %d\n", answer()); return 0; }
EOF

gcc -E demo.c | wc -l                 # 1) preprocess: ~560 líneas desde 3
gcc -S -O2 -masm=intel demo.c         # 2) compile  -> demo.s (asm legible)
gcc -c demo.c -o demo.o               # 3) assemble -> demo.o (machine code)
nm demo.o                             #    inspeccioná símbolos: _printf aparece como 'U'
gcc demo.o -o demo                    # 4) link     -> demo (ejecutable, sin agujeros)
file demo                             #    "ELF 64-bit ... executable" (Mach-O en macOS)
./demo                                # run: the answer is 42
```

Cada comando frena una etapa más tarde que el anterior. El momento revelador es `nm demo.o`:
`_printf` es `U` (undefined) en el object file pero el programa igual corre después de
linkear, porque el linker llenó ese agujero. (En Linux vas a ver ELF y podés escarbar más con
`readelf`/`objdump`; macOS emite Mach-O y usa `otool` — las *etapas* son idénticas.)

## AOT nativo vs runtimes administrados

Este pipeline es compilación **ahead-of-time (AOT)** a machine code nativo: el trabajo pasa
una vez, en build time, y la CPU corre el resultado directamente. Contrastá con el piso
administrado del que venís — Java/Kotlin compilan a bytecode que una JVM interpreta y
**JIT**-compila en runtime; JavaScript se JIT-compila desde el fuente al vuelo. Esa es la
movida [[lowlevel/machine-model/you-are-the-runtime-now|"ahora el runtime sos vos"]] hecha
concreta: con C no hay VM entre tus bytes y el silicio, así que las etapas de arriba son la
historia *entera* de cómo tu código llega a la CPU.

## Modos de falla y trade-offs

- **Error de compile vs error de link son etapas distintas.** Un error de sintaxis/tipos
  frena en *compile*; un *"undefined reference to `foo`"* es una falla de *link* — el símbolo
  se declaró pero nunca se definió o no se linkeó su librería (`-lm`, etc.). El mismo "no
  buildea", distinto fix.
- **Declaración vs definición.** Una declaración en un header satisface al compilador; el
  linker igual necesita exactamente una definición. Si falta → undefined reference; si hay dos
  → símbolo duplicado.
- **Fallas en load time.** "error while loading shared libraries" / "image not found" no es ni
  compile ni link — es el *dynamic linker* en el lanzamiento, incapaz de encontrar un `.so`/
  `.dylib`. Otra etapa distinta.
- **La optimización cambia el assembly, no el significado.** `-O2` puede reordenar, inlinear y
  borrar código (nos apoyamos en esto en notas anteriores); el comportamiento observable debe
  coincidir con la máquina abstracta de C, pero el `.s` emitido puede no parecerse en nada al
  fuente.

## En la práctica

- **Sabé qué etapa falló.** Leé el vocabulario del error: parse/tipos → compile; "undefined
  reference" → link; "cannot open shared object" → load. Eso solo te ahorra la mitad del
  tiempo de debug.
- **Usá `-E`, `-S`, `-c` para mirar adentro.** Cuando un macro se porta mal, leé la salida de
  `-E`; cuando dudás de qué emitió el compilador, leé `-S`; esto es práctica cotidiana de bajo
  nivel.
- **Tené claras las declaraciones (headers) y las definiciones (un `.o`).** La mayoría de los
  errores de link de principiante son una definición que falta o una librería sin linkear, no
  un bug de código.
- **Este es el puente a la rama de toolchain.** Todo lo de acá — objects, símbolos, linking,
  el loader — tiene su tratamiento completo en
  [[lowlevel/toolchain-and-linking/index|toolchain y linking]].

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/you-are-the-runtime-now|Ahora el runtime sos vos]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/registers-and-the-isa|Registros y la ISA]] · [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]

## Fuentes

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 7 — linking, object files, resolución de símbolos y loading; la columna de esta nota. https://csapp.cs.cmu.edu/
- **John R. Levine — *Linkers and Loaders*** — el libro definitivo sobre lo que el linker y el loader realmente hacen. https://linker.iecc.com/
- **GCC Manual — "Overall Options" (`-E`, `-S`, `-c`)** — cómo el driver corre las cuatro etapas y dónde frenarlo. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
- **Jens Gustedt — *Modern C*** — translation units, declaraciones vs definiciones y linkage desde el lado de C. https://gustedt.gitlabpages.inria.fr/modern-c/
- **man pages — `cpp`, `as`, `ld`, `ld.so`, `nm`** — las herramientas individuales detrás de `gcc`, y cómo inspeccionar su salida. https://man7.org/linux/man-pages/man1/ld.1.html
