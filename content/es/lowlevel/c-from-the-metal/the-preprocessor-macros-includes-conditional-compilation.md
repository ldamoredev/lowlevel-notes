---
title: "El preprocessor: macros, includes y compilación condicional"
description: El preprocessor de C corre antes del compilador: incluye archivos, expande macros y elimina o conserva código con compilación condicional. Es poderoso porque reescribe tokens, y peligroso por la misma razón.
tags: [c, preprocessor, macros, includes, conditional-compilation]
order: 5
updated: 2026-06-22
---
# El preprocessor: macros, includes y compilación condicional

El preprocessor de C es una fase de reescritura de tokens que corre antes del compilador
propiamente dicho. No conoce tipos, scopes, lifetimes ni expresiones como el compilador de
C; conoce directivas como `#include`, `#define`, `#if` y tokens textuales. Eso lo hace
esencial para el modelo de build de C: los headers se pegan dentro de translation units,
las constantes y pequeños idioms genéricos se expresan como macros, y el código específico
de plataforma o feature se selecciona antes del parsing. El costo es que los errores pasan
antes de que el type checker pueda ayudarte.

> El reset: los macros no son funciones, los includes no son imports y `#if` no es una rama
> de runtime. El preprocessor edita el stream de fuente que el compilador va a ver después.

## La fase antes de que C vea C

La traducción empieza con preprocessing. Los comentarios desaparecen, los backslash-newline
splices se manejan, las directivas corren, los macros se expanden y `#include` trae otros
archivos a la translation unit actual. Recién después de eso el compilador parsea
declaraciones, tipos, sentencias y expresiones.

| Feature del preprocessor | Qué hace | Qué no hace |
|---|---|---|
| `#include` | pega acá los tokens de otro archivo | crear una frontera de módulo |
| macro object-like | reemplaza un nombre por tokens | crear una constante tipada |
| macro function-like | reemplaza una secuencia de tokens con forma de llamada | evaluar argumentos una vez como una función |
| `#if` / `#ifdef` | conserva o elimina código antes de compilar | correr una condición de runtime |
| `#` / `##` | stringifica o pega tokens | inspeccionar valores o tipos |

Esto explica por qué los headers importan tanto en C. Un header no se "carga" en runtime.
Se inserta en cada fuente que lo incluye, normalmente protegido por `#ifndef` o
`#pragma once` para que las declaraciones no se repitan dentro de una translation unit. Las
notas posteriores sobre declaraciones, definiciones y linkage van a construir sobre esto:
el preprocessor arma el texto; el compilador y el linker hacen cumplir las reglas de C.

## Cómo funciona realmente

Un macro es una regla de reemplazo de tokens. `#define BUFFER_CAP 4` dice que los tokens
posteriores llamados `BUFFER_CAP` se expanden al token `4`. Un macro function-like como
`#define MAX(a, b) ((a) > (b) ? (a) : (b))` sustituye los tokens de los argumentos dentro
de la lista de reemplazo. No llama a una función, no type-checkea argumentos ni garantiza
que cada argumento se evalúe una sola vez.

Por eso existe el estilo defensivo de macros:

- **Poné paréntesis alrededor de parámetros y de toda la expresión.** `SQUARE(x)` debería
  ser `((x) * (x))`, no `x * x`, o la precedencia muerde a los callers.
- **Nunca pases side effects a macros que puedan usar un argumento más de una vez.**
  `MAX(i++, j++)` puede incrementar más de lo que quisiste.
- **Preferí funciones `static inline` cuando los tipos son conocidos.** Obedecen scoping y
  reglas de evaluación de C, y los compiladores modernos las inlinean.
- **Usá macros para cosas que C no puede expresar directo.** Compilación condicional,
  feature flags de compile time, idioms genéricos basados en `sizeof`, stringification,
  token pasting y declaraciones que deben variar por plataforma.

La compilación condicional es igual de literal. Si `ENABLE_TRACE` es `0`, un bloque `#if
ENABLE_TRACE` no se parsea como C en absoluto. Eso sirve para kernels, targets embebidos y
capas de portabilidad donde un símbolo, registro, syscall o header quizá no exista en cada
target. También es peligroso: el código escondido detrás de un `#if` poco usado puede
pudrirse silenciosamente hasta que se testee esa configuración de build.

## Artefacto ejecutable: mismo fuente, tokens distintos

Este demo usa `#include`, macros con defaults, stringification, un macro de tamaño y
compilación condicional. El build base compila el código de trace afuera. Un segundo build
pasa definiciones `-D` por línea de comandos, cambiando el stream de tokens antes de que el
compilador lo vea.

```c
// demo.c — preprocessor: includes, macros y compilacion condicional.
// gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>

#ifndef BUFFER_CAP
#define BUFFER_CAP 4
#endif

#ifndef ENABLE_TRACE
#define ENABLE_TRACE 0
#endif

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#if ENABLE_TRACE
#define TRACE(message) printf("trace: %s\n", (message))
#else
#define TRACE(message) ((void)0)
#endif

static int clamp_to_cap(int value) {
    return value > BUFFER_CAP ? BUFFER_CAP : value;
}

int main(void) {
    int samples[BUFFER_CAP] = {0};

    for (size_t i = 0; i < ARRAY_LEN(samples); i++) {
        samples[i] = (int)(i + 1) * 10;
    }

    printf("BUFFER_CAP value       = %d\n", BUFFER_CAP);
    printf("BUFFER_CAP as text     = %s\n", STRINGIFY(BUFFER_CAP));
    printf("ARRAY_LEN(samples)     = %zu\n", ARRAY_LEN(samples));
    printf("MAX(3, 7)              = %d\n", MAX(3, 7));
    printf("clamp_to_cap(9)        = %d\n", clamp_to_cap(9));

    TRACE("compiled only when ENABLE_TRACE is nonzero");

#if ENABLE_TRACE
    printf("trace mode             = compiled in\n");
#else
    printf("trace mode             = compiled out\n");
#endif

    return 0;
}
```

Compilá y corré el default:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
```

Salida real:

```
BUFFER_CAP value       = 4
BUFFER_CAP as text     = 4
ARRAY_LEN(samples)     = 4
MAX(3, 7)              = 7
clamp_to_cap(9)        = 4
trace mode             = compiled out
```

Ahora compilá con definiciones provistas por el comando de build:

```bash
gcc -O0 -Wall -Wextra -DENABLE_TRACE=1 -DBUFFER_CAP=6 demo.c -o demo && ./demo
```

Salida real:

```
BUFFER_CAP value       = 6
BUFFER_CAP as text     = 6
ARRAY_LEN(samples)     = 6
MAX(3, 7)              = 7
clamp_to_cap(9)        = 6
trace: compiled only when ENABLE_TRACE is nonzero
trace mode             = compiled in
```

El código C no leyó una variable de entorno. El comando de build cambió el programa antes
de compilar. `BUFFER_CAP` se volvió tokens distintos, `TRACE` se volvió un `printf` o
`((void)0)`, y un lado del `#if` nunca llegó al parser.

## Modos de falla y trade-offs

- **Doble evaluación.** `MAX(i++, j++)` puede incrementar dos veces el argumento elegido y
  una vez el otro. Los macros function-like sustituyen tokens; no evalúan como funciones.
- **Bugs de precedencia.** `#define SQUARE(x) x * x` hace que `SQUARE(1 + 2)` expanda a `1
  + 2 * 1 + 2`. Poné paréntesis alrededor de parámetros y de toda la expresión de
  reemplazo.
- **Sorpresas de scope.** Los macros ignoran el scope de C después de definirse. Un nombre
  de macro puede reescribir código posterior hasta `#undef` o el fin de la translation unit.
- **Ramas condicionales stale.** Código detrás de `#if TARGET_X` quizá ni siquiera parsea
  en otros builds. Cada configuración soportada necesita un build.
- **Duplicación de headers.** Los headers son texto pegado. Sin include guards, repetir
  declaraciones puede estar bien, pero repetir definiciones puede romper una translation
  unit.
- **Debuggear el fuente equivocado.** Errores del compilador después de expansión de macros
  pueden apuntar a código que no escribiste literalmente. Usá `gcc -E` o `clang -E` para
  inspeccionar la salida preprocesada.

## En la práctica

- **Usá include guards en cada header.** La forma clásica es `#ifndef NAME_H`, `#define
  NAME_H`, declaraciones, `#endif`.
- **Preferí `enum`, objetos `const` o funciones `static inline` cuando encajan.** Andá a
  macros cuando necesitás preprocessing, no por costumbre.
- **Mantené aburridos los argumentos de macros.** Pasá variables y valores, no `i++`,
  llamadas a función con side effects ni expresiones cuya cantidad de evaluaciones importa.
- **Nombrá feature flags centralmente.** El build system debería adueñarse de los flags
  `-D`; el fuente debería documentar defaults con `#ifndef`.
- **Inspeccioná la expansión cuando hay confusión.** `gcc -E demo.c` muestra el stream real
  de tokens que va al compilador. Suele ser el camino más corto para salir de la niebla de
  macros.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/why-c-still-matters|Por qué C todavía importa]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|El sistema de tipos de C es débil]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: el contrato]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] · [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre fases de preprocessing, directivas, reemplazo de macros, inclusión condicional y translation units. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Preprocessor** — referencia compacta para directivas, reemplazo de macros, inclusión condicional, stringification y token pasting. https://en.cppreference.com/w/c/preprocessor
- **GCC — Options Controlling the Preprocessor** — referencia práctica para `-D`, `-U`, `-I`, `-E`, include paths y diagnósticos de preprocessing. https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html
- **GCC — The C Preprocessor manual** — comportamiento detallado de expansión de macros, include guards, condicionales y pitfalls comunes. https://gcc.gnu.org/onlinedocs/cpp/
- **Jens Gustedt — *Modern C*** — guía moderna sobre macros, `_Generic`, funciones inline, headers y cómo evitar abuso del preprocessor. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Beej's Guide to C — The C Preprocessor** — ejemplos amigables de `#include`, `#define`, `#ifdef` y los flags del comando de build que los controlan. https://beej.us/guide/bgc/html/split/the-c-preprocessor.html
