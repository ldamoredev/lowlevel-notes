---
title: "Bits, bytes, words y direcciones"
description: El vocabulario de la memoria cruda — un bit es el átomo, un byte (8 bits) es la unidad direccionable más chica, una word es el chunk natural de la CPU, y una dirección es solo un índice en un único array gigante de bytes. Por qué los tamaños de tipos no están garantizados y cuándo recurrir a stdint.
tags: [machine-model, bits, bytes, words, addresses, stdint]
order: 7
updated: 2026-06-22
---
# Bits, bytes, words y direcciones

Sacale toda abstracción y la memoria es un único array enorme de **bytes**, cada uno con
una **dirección** numérica — su índice. Un **bit** es el átomo: un solo 0 o 1. Ocho bits
hacen un **byte**, la unidad más chica que la máquina puede direccionar individualmente.
Una **word** es el chunk que la CPU prefiere mover y computar de una — 64 bits en x86-64.
Todo lo demás en este atlas — punteros, structs, enteros, strings — es una forma
particular de interpretar bytes en direcciones. Esta nota clava ese vocabulario, porque
las intuiciones flojas acá (un `int` es "un número", una dirección es "una cosa") causan
bugs reales después.

> El reset: no hay "variables" en la memoria, solo bytes en direcciones. El sistema de
> tipos es un cuento que el compilador cuenta sobre qué bytes significan qué. Por debajo,
> es `byte[2^64]`.

## La escalera: bit → byte → word

| Unidad | Tamaño | Qué es |
|---|---|---|
| bit | 1 bit | el átomo: 0 o 1 |
| nibble | 4 bits | un dígito hex (`0x0`–`0xf`); medio byte |
| **byte** | 8 bits | la unidad *direccionable* más chica; contiene 0–255 |
| **word** | depende de la máquina | el ancho de operando natural de la CPU (64-bit en x86-64) |

Dos hechos clavados. **Un byte es 8 bits** en cualquier entorno que vayas a tocar — C lo
expone como `CHAR_BIT`, y aunque el estándar solo exige *al menos* 8, toda máquina moderna
es exactamente 8. Y **"word" está sobrecargado**, lo que confunde a la gente:

- Una **machine word** = el ancho de registro/natural = **64 bits** en x86-64 y ARM64.
- En el **assembler de x86**, `word` significa **16 bits** por razones históricas (el 8086
  de 16 bits), con `dword` = 32 y `qword` = 64. La misma sílaba, distinto tamaño — leelo
  siempre en contexto.

## Las direcciones nombran bytes

La memoria es **byte-addressable**: cada byte individual tiene su propia dirección, y una
dirección es simplemente un índice entero en ese array plano. Un **puntero** es un valor
que contiene una dirección de esas; su ancho es igual al ancho de dirección, así que en una
máquina de 64 bits `sizeof(void*) == 8` y el address space es, en principio, 2⁶⁴ bytes (el
hardware real cablea muchos menos bits).

Como las direcciones cuentan *bytes*, los elementos consecutivos de un `int[]` están a
`sizeof(int)` bytes de distancia — 4 en una máquina típica:

```
&a[0] = 0x...970
&a[1] = 0x...974   <- +4 bytes
&a[2] = 0x...978
&a[3] = 0x...97c
```

Esto es justamente por qué la aritmética de punteros escala por el tamaño del elemento y no
por 1 — el tema de [[lowlevel/pointers-and-memory/index|punteros y memoria]]. La idea clave
ahora: una dirección es un índice de byte, y un tipo le dice al compilador cuántos bytes
leer ahí y cómo interpretarlos.

## Los tamaños no están garantizados

Una trampa para quien llega de tipos administrados de tamaño fijo: **C no fija `int` en 32
bits.** El estándar solo garantiza rangos mínimos y un orden
`sizeof(char) ≤ sizeof(short) ≤ sizeof(int) ≤ sizeof(long) ≤ sizeof(long long)`. Los
tamaños reales son implementation-defined. Los layouts comunes de 64 bits:

| Tipo | LP64 (Linux/macOS) | LLP64 (Windows) |
|---|---|---|
| `char` | 1 | 1 |
| `short` | 2 | 2 |
| `int` | 4 | 4 |
| `long` | **8** | **4** |
| `long long` | 8 | 8 |
| `void*` | 8 | 8 |

Fijate que `long` es 8 bytes en Linux/macOS pero 4 en Windows — un bug de portabilidad
clásico. Cuando el ancho exacto importa (formatos de archivo, protocolos de red, registros
de hardware, un kernel de OS), no uses `int`/`long`; usá los **fixed-width types** de
`<stdint.h>`: `int8_t`, `uint16_t`, `int32_t`, `uint64_t`, y `uintptr_t` para "un entero lo
suficientemente grande como para contener un puntero". Estos significan lo mismo en toda
plataforma.

## Una word es solo bytes (un vistazo a endianness)

Tomá el valor de 32 bits `0x11223344` y mirá sus cuatro bytes *en orden de memoria* con un
`char*`:

```
the 4 bytes of 0x11223344 in memory order:
  44 33 22 11
```

El byte menos significativo (`44`) se guarda primero. Ese orden de bytes es **endianness**
(orden de bytes) — x86-64 es little-endian — y tiene su propia nota más adelante. El punto
acá: un "número" no es atómico; es una secuencia de bytes en direcciones consecutivas, y
podés inspeccionar o reinterpretar esos bytes directamente — por eso el *orden* de bytes
(endianness) muerde cuando los bytes cruzan máquinas o cables.

## Miralo

```c
// bytes.c — tamaños, byte addressing y los bytes adentro de una word.
// gcc -O0 -Wall -Wextra bytes.c -o bytes && ./bytes
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

int main(void) {
    printf("CHAR_BIT (bits per byte) = %d\n\n", CHAR_BIT);

    printf("type    bytes  bits\n");
    printf("char    %5zu  %4zu\n", sizeof(char),  sizeof(char)  * CHAR_BIT);
    printf("int     %5zu  %4zu\n", sizeof(int),   sizeof(int)   * CHAR_BIT);
    printf("long    %5zu  %4zu\n", sizeof(long),  sizeof(long)  * CHAR_BIT);
    printf("void*   %5zu  %4zu\n", sizeof(void*), sizeof(void*) * CHAR_BIT);

    int a[4];                                   // byte addressing
    printf("\neach int is %zu bytes apart:\n", sizeof(int));
    for (int i = 0; i < 4; i++)
        printf("  &a[%d] = %p\n", i, (void*)&a[i]);

    uint32_t w = 0x11223344u;                   // una word es solo bytes
    unsigned char *p = (unsigned char*)&w;
    printf("\nthe 4 bytes of 0x11223344 in memory order:\n  ");
    for (size_t i = 0; i < sizeof w; i++) printf("%02x ", p[i]);
    printf("\n");
    return 0;
}
```

`sizeof` devuelve una cuenta de **bytes**; multiplicá por `CHAR_BIT` para bits. El loop de
direcciones muestra el byte addressing (`int` consecutivos a 4 de distancia), y el cast a
`char*` te deja leer una word de a un byte — la base de cómo todo tipo más grande se
construye a partir de bytes.

## Modos de falla y trade-offs

- **Asumir que `int` es de 32 bits / `long` de 64.** Cierto en LP64, falso en Windows LLP64
  y en targets embebidos chicos. El código que hardcodea anchos se rompe al portarlo.
- **Asumir `sizeof(void*) == sizeof(int)`.** En máquinas de 64 bits un puntero es de 8
  bytes e `int` es de 4 — guardar un puntero en un `int` lo trunca. Una fuente de manual de
  crashes.
- **`sizeof` es bytes, no bits.** `sizeof(int)` es 4, no 32. Mezclarlos corrompe la
  matemática a nivel de bits.
- **KB vs KiB.** "Kilobyte" es ambiguo; 1 KiB = 1024 bytes, 1 KB = 1000. Los tamaños de
  memoria y cache son potencias de dos (KiB/MiB); sé explícito cuando importa.

## En la práctica

- **Por defecto usá `int` para contadores comunes; recurrí a `<stdint.h>` cuando el layout
  importa.** Los fixed-width types (`uint32_t`, `int64_t`, `uintptr_t`) hacen portables y
  no ambiguos los formatos on-the-wire y on-disk.
- **Pensá "dirección = índice de byte, tipo = cómo leerlo".** Este solo reencuadre
  desmitifica punteros, arrays y struct layout antes incluso de llegar a ellos.
- **Usá `size_t` para tamaños e índices.** Es el tipo sin signo que devuelve `sizeof` y está
  garantizado lo suficientemente ancho como para indexar cualquier objeto — el tipo correcto
  para longitudes y loops sobre memoria.
- **Ante la duda, imprimí `sizeof` y los bytes.** El demo de arriba zanja discusiones sobre
  ancho y layout en segundos; nunca adivines qué eligió el compilador.

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/registers-and-the-isa|Registros y la ISA]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 2 — representación de la información: bits, bytes, words, byte ordering y codificación de enteros; la columna de esta nota. https://csapp.cs.cmu.edu/
- **Jens Gustedt — *Modern C*** — los tipos enteros de C, `sizeof`, `size_t` y por qué existen los fixed-width types. https://gustedt.gitlabpages.inria.fr/modern-c/
- **cppreference — Fixed-width integer types (`<stdint.h>`)** — `int32_t`, `uint64_t`, `uintptr_t` y sus garantías. https://en.cppreference.com/w/c/types/integer
- **cppreference — operador `sizeof` y `CHAR_BIT`** — qué mide `sizeof` y el macro de bits-por-byte. https://en.cppreference.com/w/c/language/sizeof
- **ISO/IEC 9899 (estándar C), §5.2.4.2 y §6.2.5** — los rangos enteros mínimos y la naturaleza implementation-defined de los tamaños de tipos. https://www.open-std.org/jtc1/sc22/wg14/
