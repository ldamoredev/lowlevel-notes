---
title: "Build y ejecución de un programa C desde cero (gcc/clang)"
description: Una primera línea de build práctica para C: elegí un compiler driver, seleccioná estándar, activá warnings, producí un ejecutable, corrélo y entendé qué cambia cada flag antes de profundizar en la rama de toolchain.
tags: [c, build, gcc, clang, compiler, warnings]
order: 12
updated: 2026-06-29
---
# Build y ejecución de un programa C desde cero (gcc/clang)

Correr C empieza con un compiler driver como `cc`, `gcc` o `clang`. El driver lee archivos
fuente, corre el pipeline de build, linkea el runtime y las bibliotecas necesarias, y
escribe un ejecutable. Para un primer comando serio, sé explícito: elegí un estándar de C,
elegí postura de optimización/debug, activá warnings, nombrá el output y después corré el
binario. "Compiló" significa que una etapa tuvo éxito; "linkeó" y "corrió" son victorias
separadas.

> El reset: `gcc demo.c` funciona, pero oculta elecciones. Un comando deliberado dice qué
> dialecto de C, qué warnings, qué nivel de optimización y qué archivo de salida quisiste.

## Un primer comando útil

Para un programa hosted chico:

```bash
cc -std=c17 -O0 -g -Wall -Wextra -pedantic demo.c -o demo
./demo
```

Qué significa cada parte:

| Parte | Significado |
|---|---|
| `cc` | compiler driver C genérico; muchas veces clang en macOS, gcc o clang en otros sistemas |
| `-std=c17` | compilar como C17, no con el default que eligió el driver |
| `-O0` | sin optimización; debugging y stepping por source más fáciles |
| `-g` | emitir información de debug para lldb/gdb |
| `-Wall -Wextra` | activar grupos útiles de warnings |
| `-pedantic` | advertir sobre extensiones no estándar para el dialecto elegido |
| `demo.c` | translation unit de entrada |
| `-o demo` | nombre del ejecutable de salida |

Usá `gcc` o `clang` directamente cuando te importe qué compilador se usa. Usá `cc` en
scripts cuando querés el compilador C default del host. Agregá `-Werror` solo cuando el
codebase esté listo para que los warnings fallen el build; es una buena política de CI y
una herramienta filosa de migración.

## Cómo funciona realmente

El driver igual corre las mismas etapas de
[[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]]:
preprocess, compile, assemble, link. Los atajos son flags:

| Objetivo | Forma del comando |
|---|---|
| Solo preprocess | `cc -E demo.c` |
| Compilar a assembly | `cc -S demo.c -o demo.s` |
| Compilar a object | `cc -c demo.c -o demo.o` |
| Linkear object a ejecutable | `cc demo.o -o demo` |
| Chequear sintaxis/tipos solamente | `cc -fsyntax-only demo.c` |

Para múltiples source files, pasalos todos o compilá por separado:

```bash
cc -std=c17 -Wall -Wextra main.c counter.c -o app
cc -std=c17 -Wall -Wextra -c main.c -o main.o
cc -std=c17 -Wall -Wextra -c counter.c -o counter.o
cc main.o counter.o -o app
```

Los headers no se compilan solos en el modelo normal. Se incluyen dentro de translation
units. Las bibliotecas se linkean después de los objects que las necesitan en muchos
linkers Unix: `cc main.o -lm -o app` para la math library cuando haga falta. Include paths
usan `-Ipath`; paths de búsqueda de bibliotecas usan `-Lpath`; bibliotecas específicas usan
`-lname`.

Los flags de optimización cambian performance y debuggability. `-O0 -g` es amigable
mientras aprendés. `-O2 -g` es común para builds debug optimizados. Sanitizers como
`-fsanitize=address,undefined` son herramientas de builds de test que atrapan temprano
muchos bugs de memoria y UB, pero no reemplazan respetar el contrato del lenguaje.

## Artefacto ejecutable: un run script repetible

El demo vive en `examples/c-from-the-metal/build-and-run-a-c-program-from-zero/`. El archivo
C parsea un argumento opcional e imprime la macro de estándar activa.

```c
#include <stdio.h>
#include <stdlib.h>

static long sum_to(long count) {
    long total = 0;

    for (long i = 1; i <= count; i++) {
        total += i;
    }

    return total;
}

int main(int argc, char **argv) {
    long count = 3;

    if (argc > 1) {
        char *end = NULL;
        count = strtol(argv[1], &end, 10);
        if (*end != '\0' || count < 0) {
            fprintf(stderr, "usage: %s [non-negative-count]\n", argv[0]);
            return 2;
        }
    }

#ifdef __STDC_VERSION__
    printf("__STDC_VERSION__      = %ld\n", (long)__STDC_VERSION__);
#else
    printf("__STDC_VERSION__      = pre-C90\n");
#endif
    printf("requested count       = %ld\n", count);
    printf("sum 1..count          = %ld\n", sum_to(count));

    return 0;
}
```

El script vuelve repetible el build:

```sh
#!/bin/sh
# run.sh — compila con flags explicitos, corre el programa y hace un chequeo sin link.
set -e
cd "$(dirname "$0")"
CC="${CC:-cc}"

echo "== compile =="
"$CC" -std=c17 -O0 -g -Wall -Wextra -pedantic demo.c -o demo

echo "== run =="
./demo 4

echo "== syntax-only check =="
"$CC" -std=c17 -Wall -Wextra -pedantic -fsyntax-only demo.c
echo "syntax ok"
```

Corrélo:

```bash
./run.sh
```

Salida real:

```
== compile ==
== run ==
__STDC_VERSION__      = 201710
requested count       = 4
sum 1..count          = 10
== syntax-only check ==
syntax ok
```

`201710` es el valor C17 de `__STDC_VERSION__`. El paso syntax-only sirve en editores y CI
cuando querés feedback rápido antes de producir un object o ejecutable.

## Modos de falla y trade-offs

- **Usar el dialecto default del compilador sin saberlo.** Los defaults cambian por versión
  de compilador. Pasá `-std=c17`, `-std=c11` o el dialecto que quieras.
- **Tratar warnings como decoración opcional.** C acepta muchos programas peligrosos. Los
  warnings son parte del feedback loop, especialmente para conversiones, prototypes
  faltantes y errores de format string.
- **Confundir fallas de compile y link.** Mensajes de sintaxis/tipos vienen de compilación.
  "Undefined reference" o "duplicate symbol" es linking. Otra etapa, otro fix.
- **Olvidar bibliotecas.** Algunas funciones requieren flags extra de link en algunos
  sistemas, como `-lm` para math en muchos Unix-like.
- **Tensión debug vs release.** `-O0 -g` es fácil de debuggear. `-O2` se parece más al
  comportamiento de producción y puede exponer undefined behavior antes. Testeá ambos
  cuando el bug huela raro.

## En la práctica

- **Empezá cada experimento chico con una línea real de warnings.** `cc -std=c17 -O0 -g
  -Wall -Wextra -pedantic demo.c -o demo` es un buen default.
- **Usá scripts cuando los comandos tengan más de un paso.** El historial del shell no es
  un build system. Un `run.sh` o Makefile captura intención.
- **Nombrá outputs.** `-o demo` evita archivos `a.out` misteriosos y hace legibles los
  scripts.
- **Mantené archivos generados fuera de git.** Source, headers, scripts y docs se versionan;
  object files y ejecutables son build artifacts.
- **Pasá a Make cuando importen dependencias.** Cuando aparecen headers, múltiples objects o
  builds repetidos, un Makefile es la siguiente abstracción honesta.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/the-preprocessor-macros-includes-conditional-compilation|El preprocessor]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declaraciones y linkage]] · [[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]] · [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]

## Fuentes

- **GCC manual — Overall Options** — flags del driver para frenar en preprocessing, assembly, generación de object o linking final. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
- **GCC manual — C dialect options** — `-std=...`, dialectos GNU y cómo GCC elige modos de lenguaje. https://gcc.gnu.org/onlinedocs/gcc/C-Dialect-Options.html
- **GCC manual — Warning Options** — flags prácticos de warnings incluyendo `-Wall`, `-Wextra`, `-Wpedantic` y warnings más estrictos de conversión. https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
- **Clang command guide** — opciones del driver clang, etapas de compilación, diagnósticos y compatibilidad con flags estilo GCC. https://clang.llvm.org/docs/CommandGuide/clang.html
- **cppreference — C standard revisions** — valores de `__STDC_VERSION__` y marcadores principales de revisión del lenguaje. https://en.cppreference.com/w/c
- **Jens Gustedt — *Modern C*** — disciplina práctica de build C hosted, invocación del compilador y desarrollo orientado por warnings. https://gustedt.gitlabpages.inria.fr/modern-c/
