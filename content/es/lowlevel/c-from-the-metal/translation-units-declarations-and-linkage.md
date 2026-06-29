---
title: "Translation units, declaraciones vs definiciones y linkage"
description: C compila un archivo fuente preprocesado por vez. Los headers declaran nombres, los fuentes definen storage y cuerpos de función, y el linker resuelve símbolos externos entre object files.
tags: [c, translation-units, declarations, definitions, linkage, linker]
order: 6
updated: 2026-06-29
---
# Translation units, declaraciones vs definiciones y linkage

Un compilador de C no ve todo tu programa de una vez. Ve una **translation unit** (unidad
de traducción): un archivo `.c` después de que el preprocessing pegó los headers y expandió
macros. Cada translation unit se vuelve un object file, y el programa final aparece recién
cuando el linker cose esos object files. Eso significa que un header puede hacer que un
nombre sea conocido para el compilador, pero solo una definición en algún object file le da
al linker algo real para resolver.

> El reset: una declaración es "este nombre existe con este tipo." Una definición es "acá
> está el storage o el cuerpo de función." El linkage decide si otra translation unit puede
> referirse al mismo nombre.

## El compilador ve una translation unit

El preprocessor corre primero. Si `main.c` incluye `counter.h`, el compilador no trata eso
como una frontera de import; recibe un stream grande de tokens: `main.c` más las
declaraciones incluidas. Ese fuente preprocesado es la translation unit. El compilador la
parsea, chequea tipos adentro de ella y emite `main.o`.

El compilador puede confiar en declaraciones de headers:

```c
extern int counter_value;
int counter_next(void);
```

Esas líneas alcanzan para que `main.c` type-checkee una expresión como `counter_next()` o
`counter_value`. No alcanzan para construir un programa. El object file producido desde
`main.c` todavía contiene huecos para esos nombres. El linker llena esos huecos después
desde `counter.o`, el object file que contiene las definiciones.

Esta es la versión desde C de la tabla de símbolos que viste en
[[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]]:
`printf` aparecía como símbolo `U` sin resolver en un object file hasta que el paso de link
encontraba la definición de biblioteca.

## Cómo funciona realmente

Una **declaración** introduce un identifier y su tipo. Una **definición** es una
declaración que además provee el storage, el initializer o el cuerpo de función. Podés
repetir declaraciones compatibles; no debés proveer definiciones competidoras para el
mismo objeto o función externos.

| Fuente | Significado |
|---|---|
| `extern int x;` | declaración de un objeto definido en otro lado |
| `int x = 0;` | definición de un objeto con storage |
| `int next(void);` | declaración de función / prototype |
| `int next(void) { return 1; }` | definición de función |

Los headers normalmente contienen declaraciones: prototypes de función, declaraciones
`extern` para objetos compartidos, definiciones de tipos, macros y helpers `static inline`
cuando están elegidos con cuidado. Los archivos fuente normalmente contienen definiciones:
objetos reales a file scope y cuerpos de función. Esa separación es la razón por la que un
header puede incluirse desde muchos `.c` sin crear muchas copias de la misma variable
global.

El linkage controla si dos declaraciones en distintos scopes o translation units pueden
denotar la misma entidad:

| Linkage | Cómo lo obtenés normalmente | Quién puede nombrarlo |
|---|---|---|
| External linkage | funciones a file scope y objetos a file scope no `static` | otras translation units pueden referirse a él |
| Internal linkage | `static` a file scope | solo la translation unit actual puede referirse a él |
| Sin linkage | variables locales automáticas, labels, la mayoría de los nombres a block scope | solo ese scope tiene el nombre |

La palabra clave sobrecargada es `static`. A file scope, `static int step;` le da internal
linkage a `step`, haciendo el nombre privado de esa translation unit. Dentro de una
función, `static int calls;` es otra cosa: el objeto tiene static storage duration, pero el
nombre local sigue sin linkage. Misma palabra clave, regla distinta.

El linker trabaja sobre símbolos. Si `main.o` dice `U _counter_next`, significa "este
object file llama a una función llamada `_counter_next`, pero no está definida acá." Si
`counter.o` dice `T _counter_next`, contiene el cuerpo de la función. El linker matchea la
referencia con la definición y escribe el ejecutable final.

## Artefacto ejecutable: dos object files y un linker

El ejemplo vive en
`examples/c-from-the-metal/translation-units-declarations-and-linkage/`. Tiene un header,
una translation unit de implementación y una translation unit usuaria.

`counter.h` declara la superficie pública:

```c
#ifndef COUNTER_H
#define COUNTER_H

extern int counter_value;

void counter_reset(int value);
int counter_next(void);
int counter_read(void);

#endif
```

`counter.c` define los nombres públicos y mantiene estado helper privado con `static` a
file scope:

```c
#include "counter.h"

int counter_value = 0;

static int step = 1;

static int add_step(int value) {
    return value + step;
}

void counter_reset(int value) {
    counter_value = value;
}

int counter_next(void) {
    counter_value = add_step(counter_value);
    return counter_value;
}

int counter_read(void) {
    return counter_value;
}
```

`main.c` incluye las declaraciones y usa nombres definidos en otro lado:

```c
#include <stdio.h>

#include "counter.h"

int main(void) {
    counter_reset(40);

    printf("counter_read() = %d\n", counter_read());
    printf("counter_next() = %d\n", counter_next());
    printf("counter_value  = %d\n", counter_value);

    return 0;
}
```

Compilá y linkeá directo:

```bash
gcc -O0 -Wall -Wextra counter.c main.c -o demo
./demo
```

Salida real:

```
counter_read() = 40
counter_next() = 41
counter_value  = 41
```

Ahora frená en object files, linkeá explícitamente e inspeccioná símbolos:

```bash
./run.sh
```

Salida real:

```
== compile each translation unit to an object file ==
== link the objects into one executable ==
== run ==
counter_read() = 40
counter_next() = 41
counter_value  = 41
== nm counter.o ==
0000000000000040 t _add_step
0000000000000020 T _counter_next
0000000000000060 T _counter_read
0000000000000000 T _counter_reset
00000000000001a8 S _counter_value
000000000000006c d _step
== nm main.o ==
                 U _counter_next
                 U _counter_read
                 U _counter_reset
                 U _counter_value
0000000000000000 T _main
                 U _printf
```

El prefijo exacto de símbolo y las letras de sección de datos son detalles del formato de
object file. Esta corrida es en macOS/Mach-O, así que los nombres C aparecen con guiones
bajos iniciales y `counter_value` aparece como `S`; en ELF/Linux podés ver nombres sin `_`
y datos como `D` o `B`. La lección es la misma: `T` es código externamente visible definido
acá, `U` es una referencia externa sin resolver, y símbolos en minúscula como `t` y `d` son
locales al object file. `add_step` y `step` existen, pero no se exportan para que `main.o`
pueda bindear contra ellos.

## Modos de falla y trade-offs

- **Undefined reference.** El compilador aceptó una declaración, pero el linker nunca
  encontró una definición compatible. Causas comunes: te olvidaste de compilar un `.c`, te
  olvidaste de linkear una biblioteca como `-lm`, escribiste mal un símbolo, o pusiste la
  definición detrás de un build flag.
- **Multiple definition.** Dos object files definen el mismo objeto o función externos. El
  bug principiante clásico es escribir `int global = 0;` en un header e incluirlo desde
  varios `.c`. Poné `extern int global;` en el header y `int global = 0;` en exactamente un
  source file.
- **Las tentative definitions son un borde histórico.** Un `int x;` a file scope sin
  `extern` ni initializer es una tentative definition. Varias tentative definitions en una
  translation unit colapsan a una definición zero-initialized, pero entre translation units
  esto interactúa con defaults del compilador y todavía puede producir sorpresas de link.
- **`static` en headers puede duplicar estado.** Un `static int cache;` a file scope dentro
  de un header crea un objeto interno separado en cada translation unit que lo incluye. A
  veces eso es intencional; muchas veces es un bug disfrazado de privacidad.
- **El `inline` de C no es el `inline` de C++.** `inline`, `extern inline` y `static inline`
  en C tienen reglas de linkage que sorprenden; usá `static inline` en headers salvo que
  tengas un plan específico de definición externa.

## En la práctica

- **Los headers declaran; un `.c` define.** Esa regla evita la mayoría de los bugs de
  símbolos duplicados.
- **Mantené `static` a file scope como default para helpers privados.** Si ninguna otra
  translation unit debería llamarlo, no lo exportes.
- **Hacé aburridos los headers públicos.** Preferí declaraciones, nombres de tipos, macros
  y helpers `static inline` elegidos con cuidado. Evitá definiciones de objetos no
  `static` en headers.
- **Leé errores de link como errores de link.** Si el mensaje dice "undefined reference" o
  "duplicate symbol", la etapa de compilación ya terminó. Mirá object files, bibliotecas y
  nombres de símbolos.
- **Usá `nm` temprano.** Convierte ansiedad vaga de linking en hechos concretos: qué object
  define el símbolo, qué object lo referencia y si un helper quedó internal.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/the-preprocessor-macros-includes-conditional-compilation|El preprocessor]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|El sistema de tipos de C es débil]] · [[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]] · [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre translation units, declaraciones, definiciones, storage duration, linkage, tentative definitions y definiciones de función. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Declarations** — referencia compacta sobre declarators, qué introduce una declaración y cuándo una declaración también es una definición. https://en.cppreference.com/c/language/declarations
- **cppreference — Storage-class specifiers** — el mapa práctico de `extern`, `static`, storage duration y external/internal/no linkage. https://en.cppreference.com/c/language/storage_class_specifiers
- **cppreference — External and tentative definitions** — referencia enfocada para `extern int x;`, tentative definitions y restricciones de una definición en C. https://en.cppreference.com/c/language/extern
- **GNU binutils — manual de `nm`** — qué significan letras de símbolo como `T`, `U` y símbolos locales en minúscula al inspeccionar object files. https://sourceware.org/binutils/docs/binutils/nm.html
- **Jens Gustedt — *Modern C*** — explicación moderna de headers, translation units, linkage, `inline` y diseño de interfaces en C. https://gustedt.gitlabpages.inria.fr/modern-c/
