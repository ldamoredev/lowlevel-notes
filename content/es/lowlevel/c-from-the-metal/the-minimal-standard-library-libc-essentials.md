---
title: "La biblioteca estándar mínima (esenciales de libc)"
description: La biblioteca estándar de C es una caja de herramientas hosted chica, no el lenguaje en sí. Los headers declaran funciones y tipos para I/O, memoria, strings, allocation, parsing, diagnósticos y utilidades, muchas veces respaldadas por el OS mediante libc.
tags: [c, libc, standard-library, errno, malloc, stdio]
order: 11
updated: 2026-06-29
---
# La biblioteca estándar mínima (esenciales de libc)

C como lenguaje es chico, pero los programas C útiles normalmente viven en una **hosted
implementation** (implementación alojada) con la biblioteca estándar disponible. La
biblioteca te da declaraciones para I/O, allocation, operaciones de strings y bytes,
parsing, math, diagnósticos, tiempo y algoritmos utilitarios. No es magia y no es el OS. Es
una interfaz especificada, normalmente implementada por libc, que puede llamar al kernel
por debajo.

> El reset: C te da expresiones, objetos y control flow. libc te da `printf`, `malloc`,
> `memmove`, `strtol`, `qsort`, `errno` y compañía. Sabé en qué contrato te estás apoyando.

## Hosted vs freestanding

El estándar de C distingue **hosted** y **freestanding** implementations. Hosted es lo que
normalmente usás en Linux, macOS, BSD y Windows: existe `main`, se espera la biblioteca
estándar completa, y headers como `<stdio.h>`, `<stdlib.h>` y `<string.h>` están
disponibles. Freestanding es de donde suelen partir kernels, bootloaders y firmware
embebido: solo un subconjunto chico está garantizado.

Eso importa para este atlas porque el camino de OS-from-scratch es freestanding. El código
temprano de kernel no puede asumir `printf`, `malloc`, archivos, procesos ni un sistema
operativo por debajo. En un programa hosted, libc se sienta entre vos y servicios del OS
como archivos, terminales, memory mapping y process exit.

## Cómo funciona realmente

Los headers declaran la superficie de la biblioteca; las definiciones vienen de la C
library linkeada. Eso conecta directo con
[[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|translation units y linkage]]:
`#include <stdlib.h>` le permite al compilador type-checkear `malloc`, pero el linking y el
loading proveen la implementación real.

Los headers centrales que conviene reconocer temprano:

| Header | Qué saber primero |
|---|---|
| `<stddef.h>` | `size_t`, `ptrdiff_t`, `NULL`, `offsetof` |
| `<stdint.h>` | tipos enteros de ancho fijo como `uint32_t` |
| `<stdbool.h>` | `bool`, `true`, `false` antes de cambios de estilo en C23 |
| `<stdio.h>` | `printf`, `fprintf`, `snprintf`, `FILE` |
| `<stdlib.h>` | `malloc`, `free`, `strtol`, `qsort`, `exit` |
| `<string.h>` | `memcpy`, `memmove`, `memset`, `strlen`, `strcmp` |
| `<errno.h>` | indicador de error medio thread-local usado por muchas APIs |
| `<assert.h>` | `assert` para chequeos de contrato en debug |
| `<ctype.h>` | clasificación de bytes como `isdigit`, con caveats de unsigned-char |
| `<limits.h>` | límites de implementación como `CHAR_BIT`, `INT_MAX` |

Varias APIs de biblioteca codifican trade-offs viejos de C. `malloc` devuelve storage
crudo; vos poseés el lifetime. `strlen` escanea hasta NUL; no conoce capacidad. `strtol`
reporta errores usando tanto un end pointer como `errno`. `memcpy` requiere rangos no
solapados; `memmove` maneja solapamiento. `qsort` borra tipos mediante `void *`, así que el
comparador debe restaurar el tipo correcto.

## Artefacto ejecutable: un tour chico de libc

El demo vive en
`examples/c-from-the-metal/the-minimal-standard-library-libc-essentials/demo.c`. Usa
`strtol`, `errno`, `malloc`, `qsort`, `memmove` y `free`.

```c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_ints(const void *left, const void *right) {
    int a = *(const int *)left;
    int b = *(const int *)right;

    return (a > b) - (a < b);
}

static void print_ints(const int *values, size_t count) {
    for (size_t i = 0; i < count; i++) {
        printf("%s%d", i == 0 ? "" : " ", values[i]);
    }

    printf("\n");
}

int main(void) {
    char *end = NULL;
    errno = 0;
    long parsed = strtol("42kb", &end, 10);

    printf("strtol value          = %ld\n", parsed);
    printf("strtol rest           = %s\n", end);

    errno = 0;
    (void)strtol("999999999999999999999999999999", NULL, 10);
    printf("strtol ERANGE         = %s\n", errno == ERANGE ? "yes" : "no");

    int *values = malloc(4 * sizeof *values);
    if (values == NULL) {
        perror("malloc");
        return 1;
    }

    values[0] = 4;
    values[1] = 1;
    values[2] = 3;
    values[3] = 2;

    qsort(values, 4, sizeof values[0], compare_ints);
    printf("sorted ints           = ");
    print_ints(values, 4);

    /* memmove acepta rangos solapados; memcpy no promete eso. */
    char text[8] = "abcde";
    memmove(text + 2, text, 3);
    printf("memmove text          = %s\n", text);

    free(values);
    return 0;
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Salida real:

```
strtol value          = 42
strtol rest           = kb
strtol ERANGE         = yes
sorted ints           = 1 2 3 4
memmove text          = ababc
```

Lo interesante es la forma de los contratos. `strtol` te da tanto el número parseado como
el sufijo no parseado. Los range errors usan `errno`, así que el código lo limpia antes de
la llamada. `malloc` puede fallar y debe emparejarse con `free`. `qsort` es genérico
borrando tipos a `void *`. `memmove` es la función correcta para rangos de bytes solapados.

## Modos de falla y trade-offs

- **Olvidar qué APIs necesitan capacidad.** `snprintf`, `fgets` y `memmove` reciben tamaños;
  APIs viejas como `strcpy` no conocen la capacidad del destino.
- **Usar mal `errno`.** `errno` solo significa algo cuando la función específica dice que
  lo usa. Ponelo en cero antes de llamadas como `strtol` cuando necesitás distinguir "sin
  error" de un valor viejo.
- **Ignorar fallas de allocation.** Los sistemas hosted a veces hacen overcommit, pero
  `malloc` igual devuelve `NULL` por contrato. El código crítico maneja ese camino.
- **Confundir funciones de bytes y funciones de string.** `memcpy` copia bytes con una
  longitud. `strcpy` copia hasta NUL. Los datos binarios necesitan APIs de bytes.
- **Asumir que libc existe en código freestanding.** Kernels y bootloaders evitan libc,
  proveen su propio subconjunto chico o linkean una biblioteca compatible con freestanding.

## En la práctica

- **Aprendé los contratos, no solo los nombres.** Para cada función, preguntá: quién posee
  el storage, dónde está la longitud, cómo se reportan errores y qué precondiciones son UB
  si se rompen.
- **Preferí APIs con tamaño.** Usá `snprintf`, `fgets`, `memmove` e interfaces explícitas
  puntero-más-longitud.
- **Envolvé APIs ásperas localmente.** Un helper de proyecto para parsing, allocation o
  copia de strings puede codificar una regla de casa y reducir errores repetidos.
- **Mantené separados conceptualmente `stdlib` y syscalls.** `fopen` es libc; `open` es
  POSIX. `printf` es libc; `write` es un wrapper de syscall. Capas distintas, garantías
  distintas.
- **En trabajo freestanding, construí tu propio piso deliberadamente.** Decidí qué
  subconjunto chico de helpers de memoria, strings y formatting posee tu kernel.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/strings-are-just-char-pointers|Los strings son solo `char*`]] · [[lowlevel/c-from-the-metal/arrays-and-array-to-pointer-decay|Arrays y array-to-pointer decay]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declaraciones y linkage]] · [[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]] · [[lowlevel/systems-programming/index|Systems Programming]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre hosted vs freestanding implementations y contratos de la biblioteca estándar. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — C standard library headers** — índice compacto de headers estándar y las declaraciones que expone cada uno. https://en.cppreference.com/w/c/header
- **cppreference — Null-terminated byte strings** — referencia para `strlen`, `strcmp`, `strcpy` y por qué las APIs de C string dependen de un terminador NUL. https://en.cppreference.com/w/c/string/byte
- **cppreference — `strtol`** — contrato de parsing, end pointer, manejo de base, `errno` y range errors. https://en.cppreference.com/w/c/string/byte/strtol
- **cppreference — `qsort`** — sorting genérico mediante `void *`, tamaño de elemento y contrato del comparador. https://en.cppreference.com/w/c/algorithm/qsort
- **cppreference — `memmove`** — contrato de copia de bytes para rangos solapados, contrastado con `memcpy`. https://en.cppreference.com/w/c/string/byte/memmove
