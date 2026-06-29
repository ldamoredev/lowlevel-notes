---
title: "Arrays y array-to-pointer decay"
description: Los arrays de C son objetos contiguos, no punteros, pero en la mayoría de las expresiones un nombre de array se convierte a un puntero a su primer elemento. Las excepciones, la trampa de sizeof, arr vs &arr y por qué los multidimensionales no son T**.
tags: [c, arrays, pointers, sizeof, decay]
order: 7
updated: 2026-06-29
---
# Arrays y array-to-pointer decay

Un array en C es un objeto real: `N` elementos contiguos de tipo `T`. Pero en la mayoría de
los contextos de expresión, la expresión de array **decae** (se convierte) a un `T *` que
apunta al primer elemento, `&arr[0]`. Por eso el indexing de arrays y la aritmética de
punteros parecen intercambiables, y por eso los arrays se vuelven filosos en cuanto los
pasás a una función. El array no se volvió puntero en todos lados; un contexto de expresión
lo convirtió.

> El reset: array-to-pointer decay no es "los arrays son punteros." Es "una expresión de
> array normalmente se convierte a un puntero al elemento cero, salvo en algunos contextos
> importantes."

## La regla de decay

Para un objeto array:

```c
int a[5] = {10, 20, 30, 40, 50};
```

La expresión `a` normalmente se convierte a `int *`, con el valor `&a[0]`. La aritmética de
punteros escala por `sizeof *a`, así que `a + 2` significa "dos elementos `int` después de
`a[0]`." El indexing se define en términos de esa aritmética:

```c
a[i]      == *(a + i)
i[a]      == *(i + a)
```

La segunda forma es legal porque la suma es conmutativa a nivel expresión: un operando es
el puntero y el otro es el entero. También es un buen recordatorio de que `[]` no hace
bounds checking. Es aritmética de punteros más dereferencia.

El decay **no** pasa en estos contextos clave:

| Contexto | Resultado |
|---|---|
| `sizeof arr` | tamaño del objeto array completo |
| `&arr` | puntero al array completo, tipo `T (*)[N]` |
| string literal que inicializa un array | `char s[] = "hi";` crea un array con bytes copiados |

Esa lista de excepciones es chica y esencial. `sizeof arr` sirve solo mientras `arr` sigue
siendo un objeto array en el scope actual. `&arr` tiene el mismo valor de dirección que
`arr` cuando lo imprimís como `void *`, pero otro tipo: puntero a array, no puntero al
primer elemento.

## Cómo funciona realmente

El compilador conoce el bound del array cuando la expresión todavía tiene tipo array. En
`main`, `sizeof arr` es `5 * sizeof(int)`, así que en la máquina usada para este demo son
20 bytes. Pero cuando pasás `arr` a una función, la expresión argumento decae antes de la
llamada. Un parámetro escrito como `int values[]` se ajusta por el lenguaje a `int *values`;
la función recibe un puntero, no un objeto array.

Ese es el bug clásico de perder longitud:

```c
void inspect(int values[]) {
    sizeof values;  // sizeof(int *), no el array del caller
}
```

Los compiladores suelen advertir sobre esto porque casi siempre es un error. La regla es
simple: pasá la longitud con el puntero. `sizeof(arr) / sizeof(arr[0])` vale solo en el
scope donde `arr` sigue siendo un array real, no después de cruzar una frontera de función.

`arr` y `&arr` también explican por qué el tipo importa más que la dirección impresa.
Convertidos a `void *`, ambos apuntan al mismo byte. Pero `arr + 1` avanza un elemento,
mientras que `&arr + 1` avanza el array entero:

```c
arr + 1   // int *: un int despues
&arr + 1  // int (*)[5]: un array-de-5-int despues
```

Los arrays multidimensionales siguen la misma regla, sin volverse `T **`. Para
`int m[2][3]`, la expresión `m` decae a `int (*)[3]`: un puntero a una fila de tres `int`.
La cantidad de columnas es parte del tipo apuntado, así que la aritmética de punteros puede
saltar exactamente una fila. Un "array de punteros a filas" asignado por separado es otro
layout y otro tipo.

Los VLAs, variable length arrays, llevan un bound de runtime en su tipo de array mientras
están en scope, pero igual decaen en contextos de expresión y llamada a función. Array no
es puntero, aunque las expresiones de array muchas veces se comporten parecido a punteros.

## Artefacto ejecutable: mirá desaparecer el tipo

El demo vive en `examples/c-from-the-metal/arrays-and-array-to-pointer-decay/demo.c`.
Calcula `sizeof` intencionalmente sobre un parámetro ajustado de función, así que el fuente
desactiva localmente ese único warning pedagógico y mantiene limpio el build con
`-Wall -Wextra`.

```c
#include <stddef.h>
#include <stdio.h>

/* Se silencia solo el warning que este demo provoca a proposito. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsizeof-array-argument"
#endif
static void inspect_parameter(int values[], size_t count) {
    /* En parametros de funcion, int values[] se ajusta a int *. */
    printf("sizeof parameter      = %zu bytes\n", sizeof values);
    printf("count passed in       = %zu elements\n", count);
}
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static void inspect_subscript(const int *values, size_t index) {
    int via_index = values[index];
    int via_pointer = *(values + index);
    int via_reversed = index[values];

    printf("a[2], *(a + 2), 2[a] = %d, %d, %d\n",
           via_index, via_pointer, via_reversed);
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Salida real:

```
sizeof arr in main    = 20 bytes
elements in main      = 5
sizeof parameter      = 8 bytes
count passed in       = 5 elements
a[2], *(a + 2), 2[a] = 30, 30, 30
(void*)arr == (void*)&arr = true
arr + 1 byte delta    = 4 bytes
&arr + 1 byte delta   = 20 bytes
matrix row stride     = 12 bytes
```

El primer `sizeof` ve el array real. El segundo ve un parámetro puntero. La línea de
subscript muestra la definición de `[]`. La comparación de direcciones da true después de
convertir ambas expresiones a `void *`, pero los deltas en bytes revelan la diferencia de
tipo: `arr + 1` mueve un `int`; `&arr + 1` mueve el `int[5]` completo. El stride de matriz
es 12 bytes porque `int matrix[2][3]` decae a un puntero a una fila de tres `int`.

## Modos de falla y trade-offs

- **Longitud perdida.** Un parámetro `T a[]` no lleva el conteo de elementos del caller. Si
  la función necesita bounds, pasá `size_t count` al lado del puntero.
- **Falsa confianza en `ARRAY_LEN`.** `sizeof(a) / sizeof(a[0])` sirve para arrays reales y
  está mal para punteros decaídos. Los helpers `ARRAY_LEN` basados en macros no pueden
  rescatar un puntero.
- **Confusión con `T **`.** Un array bidimensional es storage contiguo row-major. Decae a un
  puntero a fila, no a un puntero a puntero. Las APIs tienen que escribir la cantidad de
  columnas: `void f(size_t rows, size_t cols, int m[rows][cols])` o `int (*m)[COLS]`.
- **One-past no se lee.** `arr + count` es un puntero one-past válido para comparación o
  terminación de loop. Dereferenciarlo es undefined behavior.
- **Ergonomía de API vs seguridad.** La convención puntero-más-longitud de C es explícita y
  barata, pero es fácil separar el par. Abstracciones más fuertes envuelven puntero y
  longitud en un `struct`.

## En la práctica

- **Pasá puntero y longitud juntos.** Preferí `void process(int *items, size_t count)` o un
  pequeño `struct` de slice antes que un puntero desnudo.
- **Calculá la longitud del array antes del decay.** En el mismo scope que `int items[N]`,
  usá `sizeof items / sizeof items[0]`; después de una frontera de llamada, confiá en el
  `count` explícito.
- **Leé honestamente los parámetros de función.** `int a[]`, `int a[10]` e `int *a` son el
  mismo tipo de parámetro para un parámetro de array unidimensional.
- **Usá `&arr` cuando querés el tipo del array completo.** Es raro, pero útil para APIs que
  requieren un bound exacto, como `int (*p)[5]`.
- **Escribí APIs de arrays multidimensionales con la dimensión final.** `int m[][3]` o
  `int (*m)[3]` conserva el stride de fila; `int **` describe otra estructura.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declaraciones y linkage]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|El sistema de tipos de C es débil]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: el contrato]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre declarators de array, array-to-pointer conversion, `sizeof`, subscripting y ajuste de parámetros de función. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Array declaration** — referencia compacta para tipos array, array-to-pointer conversion, arrays multidimensionales y ajuste de parámetros de función. https://en.cppreference.com/c/language/array
- **cppreference — operador `sizeof`** — casos exactos donde `sizeof` evalúa tamaño de objeto y donde no pasa array-to-pointer conversion. https://en.cppreference.com/c/language/sizeof
- **cppreference — Operator precedence and member access** — referencia para subscripting como expresión de operador y su relación con aritmética de punteros. https://en.cppreference.com/c/language/operator_precedence
- **Jens Gustedt — *Modern C*** — tratamiento moderno de arrays, parámetros puntero, VLAs y diseño de APIs con arrays. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Beej's Guide to C — Arrays** — ejemplos accesibles de storage de arrays, indexing, parámetros de función y por qué importa pasar longitud. https://beej.us/guide/bgc/html/split/arrays.html
