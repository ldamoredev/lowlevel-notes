---
title: "Bug clásico: buffer overflow y acceso out-of-bounds"
description: Los punteros C llevan direcciones, no bounds. Buffer overflows y lecturas out-of-bounds aparecen cuando el código olvida para qué objeto, array y longitud es válido un puntero.
tags: [buffer-overflow, out-of-bounds, arrays, bounds, undefined-behavior]
order: 13
updated: 2026-06-30
---
# Bug clásico: buffer overflow y acceso out-of-bounds

C te deja calcular direcciones dentro de un array, pero no adjunta un bound runtime a un
puntero. El programa debe recordar de qué objeto vino el puntero y cuántos elementos son
válidos. Un buffer overflow escribe fuera de ese rango; una lectura out-of-bounds carga
fuera de ese rango. Ambos bugs empiezan como mala aritmética y terminan como undefined
behavior.

> El reset: un puntero más una longitud es una abstracción distinta de un puntero solo. La
> mayoría de las APIs C robustas terminan redescubriendo slices.

## Cómo funciona realmente

Para un array de `N` elementos, la aritmética de punteros C está definida dentro del objeto
array y un elemento después del final. El puntero one-past sirve para terminación de loops
y comparación de rangos. No es un elemento válido para dereferenciar.

```c
int a[3] = {10, 20, 30};
int *begin = &a[0];
int *end = &a[3];      /* one past, válido como límite */
int bad = *end;        /* undefined behavior */
```

El indexing de arrays es solo sintaxis sobre aritmética de punteros: `a[i]` significa
`*(a + i)`. Si `i` está mal, la dereferencia está mal. El lenguaje no la chequea por vos.

El peligro se pone más filoso con byte buffers y strings C. Un string necesita lugar para
su `'\0'` final. Copiar `strlen(src)` bytes copia los caracteres pero no el terminador.
Copiar `strlen(src) + 1` bytes es correcto solo si el destino tiene esa cantidad de bytes
disponibles. `sizeof dst` funciona cuando `dst` es un array real en el mismo scope; no
funciona después de que el array decayó a un parámetro puntero.

Los writes out-of-bounds pueden corromper locals adyacentes en el stack, metadata del
allocator en el heap, otro campo del mismo struct o datos de control según layout y
plataforma. Las lecturas out-of-bounds pueden filtrar datos, mezclar valores indefinidos
en decisiones o parecer inocuas hasta que un optimizador usa la suposición de que
undefined behavior no ocurre.

La reparación local es simple y repetitiva: llevá el bound, chequeá antes de la aritmética
y hacé que el chequeo use la misma unidad que la operación. Los element counts chequean
índices de elementos. Los byte counts chequean copias de bytes. Las copias de strings
incluyen el terminador.

## Artefacto ejecutable: slices y copia chequeada de strings

El demo vive en `examples/pointers-and-memory/buffer-overflow-and-out-of-bounds-access/demo.c`.

```c
// demo.c — muestra una slice con bounds explicitos y copia de C strings que
// chequea espacio para el terminador antes de llamar a `memcpy`.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>
#include <string.h>

struct Slice {
    int *data;
    size_t count;
};

static int slice_get(struct Slice slice, size_t index, int *out) {
    if (index >= slice.count) {
        return -1;
    }

    *out = slice.data[index];
    return 0;
}

static int copy_c_string(char *dst, size_t dst_size, const char *src) {
    size_t needed = strlen(src) + 1;
    if (needed > dst_size) {
        return -1;
    }

    memcpy(dst, src, needed);
    return 0;
}

int main(void) {
    int values[] = {10, 20, 30};
    struct Slice slice = {values, sizeof values / sizeof values[0]};

    int value = 0;
    if (slice_get(slice, 2, &value) == 0) {
        printf("slice[2]              = %d\n", value);
    }

    printf("slice[3] status       = %s\n",
           slice_get(slice, 3, &value) == 0 ? "ok" : "out of bounds");

    char name[8];
    printf("copy short            = %s\n",
           copy_c_string(name, sizeof name, "atlas") == 0 ? name : "too long");
    printf("copy long status      = %s\n",
           copy_c_string(name, sizeof name, "low-level") == 0 ? "ok" : "too long");

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
slice[2]              = 30
slice[3] status       = out of bounds
copy short            = atlas
copy long status      = too long
```

El tipo `Slice` hace que el bound viaje con el puntero. `slice_get` chequea índices de
elementos antes de dereferenciar. `copy_c_string` chequea bytes, incluyendo el null
terminator, antes de llamar a `memcpy`. Nada de esto es glamoroso; por eso funciona.

## Modos de falla y trade-offs

- **Perder el bound en límites de API.** `void f(int a[])` recibe un `int *`, no la
  longitud del array.
- **Off-by-one en el terminador.** Los strings necesitan un byte más que su cantidad de
  caracteres visibles.
- **Unidades mezcladas.** Comparar un índice de elemento contra un tamaño en bytes o copiar
  elementos con un byte count abre huecos silenciosos.
- **Integer overflow antes de asignar o copiar.** `count * sizeof *p` puede wrappear si
  `count` viene de input hostil o simplemente es enorme.
- **Confiar demasiado en datos centinela.** Un `'\0'` faltante convierte código de strings
  en una lectura out-of-bounds.
- **Asumir que las lecturas son seguras.** Las lecturas out-of-bounds pueden ser
  explotables y también darle permiso al optimizador para transformar código de forma
  inesperada.

## En la práctica

- **Pasá puntero y count juntos.** Preferí `struct Slice { T *data; size_t count; }` cuando
  el par cruza varias llamadas.
- **Validá antes de derivar punteros.** Chequeá `index < count` antes de computar y usar
  `data + index`.
- **Separá APIs de bytes de APIs de elementos.** Nombrá parámetros `byte_count` o
  `element_count` para que los call sites expongan errores de unidad.
- **Preferí formatting acotado.** `snprintf` y `memcpy` explícito con tamaños chequeados
  ganan contra funciones de string sin bound.
- **Compilá con warnings y corré sanitizers.** `-Wall -Wextra` atrapa problemas de forma;
  ASan atrapa muchas violaciones de bounds en runtime.
- **Diseñá paths de input hostil como parsers.** Archivos, sockets y blobs de firmware
  merecen parsing length-first, no casts a structs e indexing esperanzado.

**Conecta con:** [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Aritmética de punteros y stride]] · [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alineación y padding]] · [[lowlevel/pointers-and-memory/use-after-free-and-double-free|Use-after-free y double-free]] · [[lowlevel/pointers-and-memory/void-star-and-type-erasure|`void*` y type erasure]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **ISO WG14 N1570 — draft de C11** — aritmética de punteros, array subscripting y undefined behavior alrededor de accesos inválidos. https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- **cppreference — Array declaration** — referencia práctica para objetos array, decay y comportamiento de parámetros array. https://en.cppreference.com/w/c/language/array
- **cppreference — `memcpy`** — documenta la primitiva de copia de bytes usada una vez chequeados los bounds. https://en.cppreference.com/w/c/string/byte/memcpy
- **Documentación de Clang AddressSanitizer** — detección runtime de heap, stack y global buffer overflows. https://clang.llvm.org/docs/AddressSanitizer.html
- **MITRE CWE-120: Classic Buffer Overflow** — taxonomía de seguridad para writes sin chequeo más allá de un buffer. https://cwe.mitre.org/data/definitions/120.html
- **MITRE CWE-125: Out-of-bounds Read** — taxonomía de seguridad para lecturas fuera de memoria válida. https://cwe.mitre.org/data/definitions/125.html
