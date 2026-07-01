---
title: "El pipeline: preprocess → compile → assemble → link"
description: "gcc main.c es un comando driver, no una acción mágica de compilador: orquesta preprocessing, compilación, assembly y linking, y cada etapa deja un artefacto inspeccionable."
tags: [toolchain, gcc, clang, preprocessor, assembler, linker]
order: 1
updated: 2026-06-22
---
# El pipeline: preprocess → compile → assemble → link

Ya viste el panorama en
[[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]]:
el fuente se vuelve ejecutable con *preprocess → compile → assemble → link*, y después el
OS lo carga y lo corre. Esta nota hace zoom en la mitad de build. El modelo mental es que
`gcc main.c` es un comando **driver**: elige e invoca las herramientas reales, pasa los
defaults correctos del target y esconde un conjunto de archivos intermedios que podés
inspeccionar. Una vez que podés nombrar la etapa, el artefacto y la herramienta, las fallas
de build dejan de sonar como un único "problema del compilador."

> El reset: un driver es orquestación. `gcc`/`clang` leen tu línea de comandos, deciden qué
> fases hacen falta y después corren preprocessing, compilación, assembly y linking con los
> defaults correctos para tu host o cross target.

## Cómo funciona realmente

Las cuatro etapas son separadas incluso cuando el driver pasa temporarios entre ellas. Si
le pedís que frene temprano, los artefactos escondidos se vuelven archivos comunes:

| Etapa | Herramienta usual | In → Out | Qué cambia | Frená en |
|---|---|---|---|---|
| Preprocess | `cpp` / frontend del driver | `.c` → `.i` | expande `#include`, `#define`, `#if` | `gcc -E` |
| Compile | `cc1` / `clang -cc1` | `.i` → `.s` | baja C a assembly del target | `gcc -S` |
| Assemble | `as` / assembler integrado | `.s` → `.o` | codifica instrucciones y emite símbolos/relocations | `gcc -c` |
| Link | `collect2` → `ld` / linker | `.o` + startup objects + libs → ejecutable | resuelve símbolos externos y dispone el archivo de salida | comando final |

El preprocessing todavía es trabajo a nivel texto. No sabe si `printf("%d", x)` es
correcto de tipos; expande headers, macros y conditional compilation dentro de la
translation unit que el compilador va a parsear.

La compilación es donde C se vuelve assembly específico de una arquitectura. El frontend
del compilador y el optimizer chequean sintaxis y tipos, aplican las reglas del lenguaje,
eligen instrucciones y registros, y emiten un `.s` para la ISA target. `-O0` y `-O2` pueden
producir assembly muy distinto desde el mismo C, pero ambos siguen siendo salidas de la
etapa de compile.

El assembly es un paso de codificación casi mecánico con contabilidad. El assembler vuelve
mnemónicos como `call _printf` bytes de machine code dentro de un object file relocatable,
y después registra símbolos: definiciones que este object aporta y referencias que todavía
necesitan una definición de otro lado.

El linking combina objects y bibliotecas en el archivo ejecutable. El linker ve tablas de
símbolos, no fuente C. Matchea referencias undefined contra definiciones, reporta
definiciones faltantes o duplicadas, dispone el archivo de salida final e incluye las piezas
de startup/runtime que pidió el driver. Internals de object files, ELF, linking
estático/dinámico, relocation y el loader son las próximas notas de esta rama; acá lo
importante es dónde empieza la etapa de link y de qué es responsable.

## El modelo de driver

`gcc` es el programa que llamás; no es todo el pipeline de compilación por sí solo. Con GCC,
el driver normalmente invoca `cpp`/lógica de preprocessing, `cc1` para compilar C, `as`
para assembly y después `collect2`, que envuelve o invoca `ld` para el link final. Con
Clang, el frontend suele aparecer como `clang -cc1`, y el assembler puede estar integrado
en vez de ser un proceso `as` separado. El modelo de etapas es el mismo.

Usá `-v` cuando quieras ver que el driver deje de esconder la maquinaria:

```bash
gcc -v demo.c -o demo
gcc -v -c demo.c -o demo.o
```

La salida es intencionalmente dependiente del host: include paths, target triple, path del
assembler, path del linker, startup objects como `crt1.o`/`Scrt1.o`, y bibliotecas por
default como `libc` o las runtime libraries del compilador. Por eso los comandos directos
con `ld` suelen fallar con errores raros de entry point o bibliotecas faltantes: el driver
normalmente agrega por vos el entry del runtime de C y las bibliotecas por default. Más
adelante, cuando construyas un kernel freestanding, vas a apagar parte de eso a propósito
con flags como `-ffreestanding`, `-nostdlib` y un cross-compiler; para C hosted de
user-space, dejá que el driver haga el trabajo aburrido.

Los sufijos de archivo también guían al driver. Una entrada `.c` arranca antes de
preprocessing. Una entrada `.i` ya está preprocesada. Una entrada `.s` puede ir directo al
assembler. Una entrada `.o` salta al paso de link. Por eso `gcc main.o helper.o -o app`
sigue siendo un comando de link válido aunque no compile nada de C.

## Artefacto ejecutable: frená en cada etapa

El ejemplo vive en
`examples/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link/`. Su fuente
es deliberadamente chico: dos funciones definidas en el object file y una llamada a libc.

```c
#include <stdio.h>

int build_number(void) {
    return 4;
}

const char *build_label(void) {
    return "pipeline";
}

int main(void) {
    printf("%s stages: %d\n", build_label(), build_number());
    return 0;
}
```

Primero, verificá el build normal de "un comando":

```bash
cd examples/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link
gcc -O0 -Wall -Wextra demo.c -o demo_direct
./demo_direct
```

Salida real:

```
pipeline stages: 4
```

Ahora corré el pipeline con script:

```bash
./run.sh
```

Salida real de esta máquina:

```
== 1) preprocess (-E): .c -> .i ==
     578 demo.i
== 2) compile (-S): .i -> .s ==
wrote demo.s
== 3) assemble (-c): .s -> .o ==
0000000000000010 T _build_label
0000000000000000 T _build_number
0000000000000020 T _main
                 U _printf
== 4) link: .o + libc -> executable ==
demo: Mach-O 64-bit executable x86_64
== run ==
pipeline stages: 4
```

La sección `nm demo.o` es el punto clave de inspección. `T _build_label`,
`T _build_number` y `T _main` son símbolos de text/código definidos por este object file.
`U _printf` es una referencia externa undefined: el object file registra la llamada, pero
no contiene la implementación de libc. En Linux/ELF normalmente vas a ver nombres de símbolo
sin el guion bajo inicial y `file demo` va a decir ELF; en macOS esta corrida produjo
Mach-O y `nm` imprime el guion bajo inicial. Las etapas son idénticas.

Usá las herramientas de object files de la plataforma en la profundidad correcta. `nm`
alcanza para esta nota porque muestra símbolos definidos vs undefined. En Linux, `readelf`
y `objdump` son las herramientas diarias para ELF; en macOS, `otool` y `nm` son las
herramientas de Mach-O. Las próximas notas de object files y ELF van a usar esas
herramientas más a fondo en vez de fingir que todos los hosts emiten el mismo formato
binario.

Esto conecta directamente con el modelo desde C de
[[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declaraciones vs definiciones y linkage]]:
una declaración deja compilar una translation unit, pero el object file sigue cargando
símbolos `U` hasta que otro object file o biblioteca provee las definiciones.

## Modos de falla y trade-offs

- **Las fallas de preprocess son sobre input textual.** Headers faltantes, include paths
  malos, expansiones de macros rotas y ramas `#if` que exponen tokens inválidos fallan antes
  de que el compilador pueda type-checkear C.
- **Las fallas de compile son sobre sintaxis, tipos y reglas del lenguaje C.** Parse errors,
  tipos incompatibles, lvalues inválidos y muchos diagnostics sobre undefined behavior
  pertenecen acá. No se produce object file.
- **Las fallas de assemble son sobre assembly del target.** Mnemónicos malos, instrucciones
  no soportadas, modo de target incorrecto o inline assembly mal formado pueden fallar
  después de compilar C pero antes de que exista un object file.
- **Las fallas de link son sobre símbolos y bibliotecas de todo el programa.** "Undefined
  reference," "duplicate symbol" y "multiple definition" significan que los object files se
  produjeron, pero el linker no pudo construir un ejecutable coherente.
- **Las fallas de loader pasan después de un link exitoso.** "Cannot open shared object
  file" en Linux o "Library not loaded" / "image not found" en macOS es el dynamic loader en
  launch time, no el compilador ni el static linker.
- **El driver cambia control por defaults razonables.** Para programas hosted normales,
  que `gcc demo.c` agregue startup objects y libc es exactamente lo que querés. Para
  kernels, bootloaders y código freestanding, esos defaults se vuelven los defaults
  equivocados, así que usás una cross-toolchain y flags explícitos.

## En la práctica

- **Empezá nombrando la etapa que falló.** Ruido de macros/includes apunta a preprocess;
  mensajes de sintaxis y tipos apuntan a compile; "undefined reference" apunta a link;
  bibliotecas compartidas faltantes apuntan a load.
- **Usá `-E`, `-S` y `-c` como sondas.** `-E` responde "¿qué fuente vio realmente el
  compilador?", `-S` responde "¿qué assembly emitió?", y `-c` responde "¿qué símbolos
  aporta o requiere esta translation unit?"
- **Usá `nm` antes de adivinar.** Si una función es `U` en un object y nunca es `T` en
  ningún object o biblioteca linkeada, el error del linker no es misterioso; la definición
  está ausente del link.
- **Usá `gcc -v` cuando importan los defaults.** Muestra include paths, paths de
  herramientas, decisiones de target, startup objects y bibliotecas. Suele ser el camino más
  corto entre "¿por qué linkeó esto?" y la línea de comandos real que generó el driver.
- **Mantené esta nota en el borde del pipeline.** Object layout, section headers,
  resolución de símbolos, static archives, shared libraries, PLT/GOT, relocation y el
  dynamic loader construyen sobre este mapa, pero cada uno necesita su propia nota más
  filosa.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declaraciones vs definiciones y linkage]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]

## Fuentes

- **GCC Manual — "Overall Options" (`-E`, `-S`, `-c`)** — los flags del driver que frenan después de preprocessing, compilación o assembly. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
- **GCC Internals — `collect2`** — por qué GCC puede envolver el linker del sistema en vez de invocar `ld` como un paso final desnudo. https://gcc.gnu.org/onlinedocs/gccint/Collect2.html
- **GNU binutils — manual de `nm`** — el significado de clases de símbolo como `T` y `U` al inspeccionar object files. https://sourceware.org/binutils/docs/binutils/nm.html
- **GNU binutils — manuales de `as` y `ld`** — las herramientas de assembler y linker detrás del driver en toolchains estilo GNU. https://sourceware.org/binutils/docs/as/ · https://sourceware.org/binutils/docs/ld/
- **David Drysdale — *Beginner's Guide to Linkers*** — explicación concreta de object files, símbolos unresolved y por qué linking está separado de compilación. https://www.lurklurk.org/linkers/linkers.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 7 — object files, linking, resolución de símbolos y loading desde el lado de systems programming. https://csapp.cs.cmu.edu/
