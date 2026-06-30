---
title: "Struct layout: alineación y padding"
description: Los structs de C se disponen en orden de declaración, pero la alineación inserta padding entre campos y al final. El orden de campos afecta tamaño, stride de array, cache y compatibilidad binaria.
tags: [structs, alignment, padding, layout, offsetof]
order: 9
updated: 2026-06-30
---
# Struct layout: alineación y padding

Un `struct` de C es una secuencia de campos en orden de declaración, con padding invisible
insertado para que cada campo satisfaga su requisito de alineación. El compilador también
puede agregar tail padding para que cada elemento en un array de ese struct quede bien
alineado. Eso significa que el orden de campos cambia `sizeof`, stride de array,
cache footprint y compatibilidad binaria. Los bytes que no escribiste siguen siendo parte
de la representación del objeto.

> El reset: los structs no son solo "bolsas de campos." Son layouts de bytes elegidos por
> la implementación de C bajo reglas de alineación. `sizeof(struct T)` incluye padding.

## La alineación crea huecos

La mayoría de las máquinas prefieren o exigen que objetos de ciertos tipos empiecen en
direcciones múltiplo de alguna alineación. Un `double` comúnmente quiere alineación de 8
bytes; un `int`, de 4. Si un `char` está seguido por un `double`, el compilador inserta
padding entre ambos:

```c
struct BadOrder {
    char tag;
    double score;
    int count;
};
```

Un mejor orden de campos puede reducir padding:

```c
struct BetterOrder {
    double score;
    int count;
    char tag;
};
```

Esto no cambia la semántica si el código solo usa nombres de campos. Sí cambia ABI,
formatos de archivo, layouts de red y cualquier código que asuma offsets.

## Cómo funciona realmente

El estándar C garantiza que los miembros no-bit-field de un struct aparecen en orden de
declaración y que un puntero al objeto struct, convertido apropiadamente, apunta a su
miembro inicial. No promete que no haya padding. Tampoco promete el mismo layout entre
compiladores, targets, settings de ABI, packing pragmas o definiciones distintas.

Usá `offsetof(type, member)` de `<stddef.h>` para pedirle al compilador el offset de un
campo:

```c
offsetof(struct BetterOrder, count)
```

Usá `_Alignof(type)` para pedir alineación en C moderno. Junto con `sizeof`, esas son las
herramientas para auditar layout sin adivinar.

El tail padding importa en arrays. Si `sizeof(struct BetterOrder)` es 16, entonces
`items[i + 1]` empieza 16 bytes después de `items[i]` aunque los campos nombrados solo
sumen 13 bytes. Los bytes extra mantienen alineado el `double` del próximo elemento.

Los padding bytes tienen valores unspecified después de asignaciones comunes a campos.
Comparar structs enteros con `memcmp`, hashear bytes crudos de struct o escribir structs
directamente a disco puede incluir padding no inicializado. Para formatos binarios
estables, serializá campo por campo con anchos y byte order explícitos.

Los packed structs reducen padding pero pueden crear accesos desalineados. En algunos
targets eso es lento; en otros, trapea. Packing es para layouts binarios externos y
protocolos de hardware, no para ahorrar memoria rutinariamente sin medir.

## Artefacto ejecutable: imprimí offsets y stride

El demo vive en `examples/pointers-and-memory/struct-layout-alignment-and-padding/demo.c`.

```c
// demo.c — muestra `sizeof`, `_Alignof`, `offsetof`, padding por orden de campos
// y stride de arrays de structs.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>

struct BadOrder {
    char tag;
    double score;
    int count;
};

struct BetterOrder {
    double score;
    int count;
    char tag;
};

struct Header {
    unsigned char kind;
    unsigned char flags;
    unsigned short length;
};

int main(void) {
    printf("BadOrder sizeof       = %zu align=%zu\n",
           sizeof(struct BadOrder), _Alignof(struct BadOrder));
    printf("  tag offset          = %zu\n", offsetof(struct BadOrder, tag));
    printf("  score offset        = %zu\n", offsetof(struct BadOrder, score));
    printf("  count offset        = %zu\n", offsetof(struct BadOrder, count));

    printf("BetterOrder sizeof    = %zu align=%zu\n",
           sizeof(struct BetterOrder), _Alignof(struct BetterOrder));
    printf("  score offset        = %zu\n", offsetof(struct BetterOrder, score));
    printf("  count offset        = %zu\n", offsetof(struct BetterOrder, count));
    printf("  tag offset          = %zu\n", offsetof(struct BetterOrder, tag));

    struct BetterOrder items[2] = {
        {.score = 1.5, .count = 2, .tag = 'a'},
        {.score = 2.5, .count = 3, .tag = 'b'},
    };

    ptrdiff_t stride = (const char *)(const void *)&items[1] -
                       (const char *)(const void *)&items[0];
    printf("array stride          = %td\n", stride);

    printf("Header sizeof         = %zu align=%zu\n",
           sizeof(struct Header), _Alignof(struct Header));

    return 0;
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Salida real en esta máquina:

```
BadOrder sizeof       = 24 align=8
  tag offset          = 0
  score offset        = 8
  count offset        = 16
BetterOrder sizeof    = 16 align=8
  score offset        = 0
  count offset        = 8
  tag offset          = 12
array stride          = 16
Header sizeof         = 4 align=2
```

Ambos structs contienen los mismos tipos de campo. El mal orden gasta espacio alineando
`score` después de `tag` y padding al final. El mejor orden empaqueta los campos chicos
después del `double`, así el stride de array baja de 24 bytes a 16 bytes.

## Modos de falla y trade-offs

- **Asumir que no hay padding.** `sizeof` puede superar la suma de tamaños de campos. Esa
  diferencia es storage real y afecta arrays.
- **Serialización cruda.** Escribir un struct con `fwrite(&s, sizeof s, 1, f)` hornea
  padding, endianness, anchos de tipos y layout de ABI.
- **`memcmp` sobre structs.** Dos structs con campos iguales pueden diferir en padding
  bytes. Compará campos, no bytes crudos, salvo que el tipo esté diseñado explícitamente
  para comparación byte-a-byte.
- **Cambiar orden de campos en ABI público.** Reordenar campos puede romper usuarios
  binarios aunque el código fuente siga compilando.
- **Abusar de packing.** Packed structs pueden ahorrar bytes pero causar loads/stores
  desalineados. Usalos en fronteras y copiá a structs internos alineados si hace falta.
- **Ahorro falso de memoria.** Reordenar un struct importa solo si existen muchas
  instancias o si está en un hot path. Medí antes de torcer tipos legibles.

## En la práctica

- **Ordená campos de alineación más estricta a más laxa cuando importa tamaño.**
  Comúnmente: punteros/doubles, después enteros, después shorts/chars/flags.
- **Usá `offsetof` en asserts de layout.** Si un layout es parte de un ABI, testeá los
  offsets que requerís.
- **Separá wire layout de runtime layout.** Parseá bytes en campos explícitos; no hagas que
  el layout de struct del compilador sea tu protocolo.
- **Mirá el stride de array.** El costo del padding se multiplica por cantidad de
  elementos.
- **Agrupá campos hot.** Layout no es solo tamaño; también es qué datos llegan juntos en
  una cache line.

**Conecta con:** [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Aritmética de punteros y stride]] · [[lowlevel/pointers-and-memory/data-layout-and-cache-friendliness|Data layout y cache-friendliness]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]] · [[lowlevel/machine-model/cache-lines-and-locality|Cache lines y localidad]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — orden de miembros de struct, padding, alineación, object representation, `offsetof` y `_Alignof`. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Structure declaration** — reglas de struct layout, padding, flexible array members e inicialización. https://en.cppreference.com/w/c/language/struct
- **cppreference — `_Alignof` / alignment** — consultas de alineación en C y requisitos de alineación. https://en.cppreference.com/w/c/language/_Alignof
- **System V AMD64 ABI** — reglas concretas de data layout y alineación para el target x86-64 Unix-like usado en el atlas. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — representación de datos, alineación y memory layout en programas de sistemas. https://csapp.cs.cmu.edu/
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — alineación, comportamiento de cache y por qué layout afecta performance. https://www.akkadia.org/drepper/cpumemory.pdf
