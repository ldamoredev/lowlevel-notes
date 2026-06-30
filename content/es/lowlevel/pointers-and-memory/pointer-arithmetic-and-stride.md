---
title: "Aritmética de punteros y stride"
description: La aritmética de punteros se mueve en elementos, no en bytes. El tipo apuntado define el stride, la resta de punteros reporta elementos y solo un objeto array más su posición one-past son territorio portable.
tags: [pointers, pointer-arithmetic, arrays, stride, c]
order: 2
updated: 2026-06-30
---
# Aritmética de punteros y stride

La aritmética de punteros es aritmética tipada sobre posiciones de array. Si `p` es un
`int *`, entonces `p + 1` significa "el próximo `int`", no "el próximo byte"; si `p` es
un `struct Packet *`, significa "el próximo packet completo." El stride sale de
`sizeof *p`, y C solo le da este significado dentro de un mismo objeto array, incluyendo
el puntero especial one-past usado para frenar iteración. Fuera de ese límite, la
dirección numérica puede parecer razonable, pero el programa C salió del comportamiento
definido.

> El reset: la aritmética de punteros responde "¿qué elemento?" La aritmética de bytes
> responde "¿qué dirección?" En C, `p + n` es matemática de elementos; casteá a
> `unsigned char *` solo cuando querés inspección byte-a-byte intencional.

## La regla de stride

Para un puntero a objeto `T *p`, sumar un entero `n` produce un puntero `n` elementos más
adelante:

| Expresión | Unidad | Significado |
|---|---|---|
| `p + 1` | `sizeof *p` bytes | próximo elemento del mismo tipo apuntado |
| `p - 1` | `sizeof *p` bytes | elemento anterior, si sigue dentro del mismo array |
| `q - p` | elementos | distancia entre dos punteros dentro del mismo array |
| `(unsigned char *)p + 1` | 1 byte | próximo byte de la representación del objeto |
| `end = a + count` | one-past | centinela válido, no válido para dereferenciar |

El indexing de arrays es esta regla más dereferencia:

```c
a[i] == *(a + i)
```

Esa igualdad es por qué arrays y punteros se sienten intercambiables en contextos de
expresión, pero el límite sigue importando. Las posiciones válidas son `&a[0]` hasta
`&a[count - 1]`, más `&a[count]` como one-past. Podés comparar el puntero one-past o
restarlo de otro puntero dentro del mismo array. No podés leer ni escribir a través de él.

## Cómo funciona realmente

El compilador escala el entero por el tipo apuntado. Conceptualmente, si `p` apunta a una
dirección de byte `A`, entonces `p + n` apunta a:

```text
A + n * sizeof *p
```

Esa fórmula es un modelo mental, no permiso para tratar cada puntero como un entero. La
operación de C está definida sobre elementos de array. Un `int[4]`, un `double[4]` y un
`struct Packet[4]` ocupan storage contiguo, pero sus pasos de puntero son distintos
porque sus tamaños de elemento son distintos.

La resta de punteros revierte el mismo escalado. `&a[3] - &a[0]` es `3`, no la cantidad
de bytes entre las dos direcciones. El tipo resultado es `ptrdiff_t`, un tipo entero con
signo de `<stddef.h>` pensado para diferencias de punteros. Restar punteros que no apuntan
dentro del mismo objeto array es undefined behavior (comportamiento indefinido), aunque
ambas direcciones vengan de locales cercanos en el stack.

`unsigned char *` es la vía de escape explícita a nivel byte. C te deja inspeccionar la
representación de cualquier objeto como una secuencia de character bytes, así que un
puntero a byte es la herramienta correcta para dumpear memoria cruda, parsear formatos
binarios o implementar allocators. `void *` no es esa herramienta: es un puntero genérico
a objeto, pero C portable no define aritmética sobre `void *` porque `void` no tiene
tamaño.

La regla también explica por qué importan los arrays de structs. La aritmética sobre
`struct Packet *` salta por el tamaño completo del struct, incluyendo cualquier padding.
No necesitás conocer a mano el offset en bytes; el tipo lleva el stride. Eso es poderoso
cuando el tipo es correcto, y peligroso cuando casteás un puntero al tipo destino
equivocado.

## Artefacto ejecutable: el stride es el tipo

El demo vive en `examples/pointers-and-memory/pointer-arithmetic-and-stride/demo.c`.

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct Packet {
    uint16_t length;
    uint8_t tag;
    uint8_t payload[5];
};

static ptrdiff_t byte_distance(const void *a, const void *b) {
    const unsigned char *left = a;
    const unsigned char *right = b;
    return right - left;
}

static void print_stride(const char *label,
                         size_t element_size,
                         const void *first,
                         const void *second) {
    printf("%-10s sizeof=%2zu  byte stride=%2td\n",
           label, element_size, byte_distance(first, second));
}

int main(void) {
    int numbers[4] = {10, 20, 30, 40};
    double weights[3] = {1.5, 2.5, 3.5};
    struct Packet packets[2] = {
        {.length = 5, .tag = 7, .payload = {1, 2, 3, 4, 5}},
        {.length = 3, .tag = 9, .payload = {6, 7, 8, 0, 0}},
    };

    print_stride("int", sizeof numbers[0], &numbers[0], &numbers[1]);
    print_stride("double", sizeof weights[0], &weights[0], &weights[1]);
    print_stride("Packet", sizeof packets[0], &packets[0], &packets[1]);

    printf("numbers[2] via *(numbers + 2) = %d\n", *(numbers + 2));
    printf("packets[1].tag via pointer    = %u\n",
           (unsigned int)(packets + 1)->tag);

    int *begin = numbers;
    int *end = numbers + (sizeof numbers / sizeof numbers[0]);
    int sum = 0;

    for (int *it = begin; it != end; it++) {
        sum += *it;
    }

    printf("one-past delta               = %td elements\n", end - begin);
    printf("sum walked by pointer        = %d\n", sum);

    const unsigned char *bytes = (const unsigned char *)(const void *)numbers;
    printf("first int raw bytes          =");
    for (size_t i = 0; i < sizeof numbers[0]; i++) {
        printf(" %02x", bytes[i]);
    }
    printf("\n");

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
int        sizeof= 4  byte stride= 4
double     sizeof= 8  byte stride= 8
Packet     sizeof= 8  byte stride= 8
numbers[2] via *(numbers + 2) = 30
packets[1].tag via pointer    = 9
one-past delta               = 4 elements
sum walked by pointer        = 100
first int raw bytes          = 0a 00 00 00
```

Las primeras tres líneas muestran el stride. `int *`, `double *` y `struct Packet *`
avanzan por cantidades distintas de bytes porque los tamaños de los elementos apuntados
son distintos. `end - begin` reporta `4` elementos, no 16 bytes. El dump de bytes cambia
deliberadamente a `unsigned char *`, así que la misma memoria ahora se recorre de a un
byte.

## Modos de falla y trade-offs

- **Dereferencia off-by-one.** `end = a + count` es un valor de puntero válido para
  comparación. `*end` es undefined behavior. El centinela no es un elemento.
- **Unidades equivocadas.** Sumar una cantidad de bytes a `T *` multiplica por
  `sizeof(T)`. Si tenés bytes, usá un puntero a byte; si tenés elementos, usá un puntero
  tipado.
- **Restar punteros no relacionados.** La diferencia de punteros está definida solo dentro
  del mismo objeto array. No es un operador general de distancia entre direcciones.
- **Castear y perder el stride.** Un cast puede hacer que la aritmética compile mientras
  vuelve falso el tipo destino. La próxima dereferencia puede violar alineación, aliasing
  o lifetime de objeto.
- **Aritmética sobre `void *`.** Algunos compiladores la aceptan como extensión que trata
  `void` como tamaño 1. C portable no; casteá a `unsigned char *` para caminar bytes.
- **Matemática de direcciones enteras.** Convertir a `uintptr_t` puede servir para
  diagnósticos, pero la aritmética entera no fabrica un puntero válido salvo que la
  implementación y la API alrededor lo digan explícitamente.

## En la práctica

- **Llevá counts en elementos, no en bytes, para arrays tipados.** `int *items,
  size_t count` hace que `items + count` sea el puntero one-past natural.
- **Nombrá fuerte los counts de bytes.** Usá nombres como `byte_count`, `byte_offset` y
  `stride` cuando la unidad es bytes; reservá `count` para elementos.
- **Preferí `for (T *it = begin; it != end; it++)` para loops C ajustados.** Hace
  explícito el centinela one-past y mantiene los bounds en el mismo sistema de tipos que
  el acceso.
- **Cambiá a `unsigned char *` en fronteras de API que tratan storage crudo.** Allocators,
  serializers, checksums y parsers binarios están orientados a bytes; los arrays comunes
  están orientados a elementos.
- **Dejá que el tipo lleve el stride del struct.** Arrays de structs, arrays de filas y
  buffers tipados se vuelven mucho más simples cuando el tipo de puntero coincide con el
  layout del storage.

**Conecta con:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]] · [[lowlevel/machine-model/endianness|Endianness]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad para operadores aditivos sobre punteros, resta de punteros, punteros one-past y límites de undefined behavior. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Operator arithmetic** — referencia compacta para suma y resta de punteros, `ptrdiff_t`, límites de array y reglas one-past. https://en.cppreference.com/w/c/language/operator_arithmetic
- **cppreference — Pointer declaration** — object pointers, `void *`, null pointers y conversiones de punteros en C. https://en.cppreference.com/w/c/language/pointer
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 2 — memoria byte-addressed y la vista de máquina debajo de la aritmética de punteros tipada. https://csapp.cs.cmu.edu/
- **Jens Gustedt — *Modern C*** — tratamiento moderno de arrays, APIs puntero-más-tamaño y por qué el código orientado a bytes debería ser explícito. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Richard Reese — *Understanding and Using C Pointers*** — ejemplos prácticos de aritmética de punteros y bugs causados por mezclar unidades de byte y elemento. https://www.oreilly.com/library/view/understanding-and-using/9781449344535/
