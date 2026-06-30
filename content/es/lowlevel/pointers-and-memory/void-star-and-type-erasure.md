---
title: "`void*` y type erasure"
description: `void*` es el puntero genérico a objeto de C: puede transportar una dirección sin transportar el tipo apuntado. Eso habilita APIs genéricas como qsort y allocators, pero el caller debe preservar tamaño, alineación y significado.
tags: [void-pointer, type-erasure, generic-programming, qsort, bytes]
order: 6
updated: 2026-06-30
---
# `void*` y type erasure

`void*` es el puntero genérico a **objeto** de C. Puede contener la dirección de cualquier
objeto, pero no recuerda el tipo, tamaño, ownership ni lifetime del objeto. Eso es type
erasure: el programa conserva "dónde" mientras descarta "qué." Así APIs como `malloc`,
`memcpy`, `qsort`, allocators y contextos de callback se mantienen genéricas en C. El
poder es real, y la factura también: cada puntero borrado tiene que viajar con suficiente
información externa para recuperar su significado de forma segura.

> El reset: `void*` significa "hay algún objeto ahí." No significa "puedo dereferenciar
> esto." Antes de leer o escribir, tenés que elegir el tipo destino correcto o tratar el
> storage explícitamente como bytes.

## El contrato del puntero borrado

`void` es un tipo incompleto sin tamaño de objeto, así que un `void*` no puede
dereferenciarse en C portable y la aritmética sobre él no está definida por el lenguaje.
Podés convertir punteros a objeto a `void*` y volver:

```c
int x = 42;
void *erased = &x;
int *restored = erased;
```

El puntero restaurado sirve solo si el tipo original, la alineación y el lifetime siguen
siendo ciertos. `void*` no transporta esos hechos. Por eso las buenas APIs genéricas pasan
los hechos faltantes al lado del puntero borrado:

| Forma de API | Hecho faltante provisto afuera |
|---|---|
| `malloc(size_t bytes)` | el caller elige tipo destino y count |
| `memcpy(void *dst, const void *src, size_t n)` | el caller provee byte count |
| `qsort(void *base, size_t count, size_t size, cmp)` | el caller provee element count, element size y comparator |
| callbacks con `void *ctx` | caller y callee acuerdan el tipo del contexto |

Hay un camino especial a nivel byte: la representación de cualquier objeto puede
inspeccionarse a través de un character type, comúnmente `unsigned char*`. Usá `void*`
para borrar tipo de objeto; usá `unsigned char*` cuando querés bytes de verdad.

## Cómo funciona realmente

En runtime, `void*` es solo un valor de puntero. El compilador deja de conocer el tipo del
objeto apuntado en esa expresión. Eso vuelve ilegales o imposibles las operaciones que
requieren información de tipo:

```c
void *p = ...;
/* *p;      // inválido: ¿qué tamaño y tipo debería leerse? */
/* p + 1;   // no es C portable: void no tiene tamaño */
```

La información de tipo se mueve al contrato de la API. `qsort` recibe un `void *base`, un
count, un element size y un comparator. Cuando se llama al comparator, sus dos argumentos
son punteros `const void*` a elementos. El comparator debe castearlos de vuelta al tipo
real antes de leer:

```c
static int compare_ints(const void *left, const void *right) {
    const int *a = left;
    const int *b = right;
    return (*a > *b) - (*a < *b);
}
```

El lado del allocator es parecido. `malloc` devuelve `void*` porque provee storage crudo,
no un objeto tipado. La asignación a `int *`, `struct Node *` u otro puntero a objeto es
donde el caller elige la interpretación. Por eso la expresión de tamaño tiene que coincidir
con el tipo de puntero final: `malloc(count * sizeof *items)`.

No extiendas esta regla a function pointers. C separa object pointers y function pointers.
Un `void*` es un puntero genérico a objeto, no un contenedor portable para un puntero a
función. Muchas máquinas representan ambos como direcciones, pero el lenguaje no promete
que las conversiones de punteros a objeto apliquen a punteros a código.

## Artefacto ejecutable: borrar, restaurar, ordenar

El demo vive en `examples/pointers-and-memory/void-star-and-type-erasure/demo.c`.

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Salida real:

```
int view              = 305419896
string view           = atlas
raw bytes             = 78 56 34 12
qsort result          = 10 20 30 40
restored from void*   = 0x12345678
```

`AnyView` empareja un puntero borrado con una función de impresión que sabe recuperar el
tipo. `dump_bytes` cambia deliberadamente de `void*` a `unsigned char*`, así que el objeto
ahora se mira como bytes. `qsort` usa `void*` más element size y comparator para ordenar
sin conocer el tipo del elemento.

## Modos de falla y trade-offs

- **Tamaño faltante.** Un `void*` solo no sabe cuánto storage es válido. Las APIs genéricas
  necesitan un `size_t` count, byte count o centinela.
- **Cast equivocado al restaurar.** Castear `void*` al tipo de puntero incorrecto y
  dereferenciar puede violar alineación, effective type o reglas de lifetime.
- **Asumir aritmética de bytes.** GNU C acepta aritmética sobre `void*` como extensión. C
  portable no. Casteá a `unsigned char*` para caminar bytes.
- **Perder `const`.** Un `const void *` preserva acceso read-only. Castearlo a `void *` y
  escribir rompe la promesa y puede ser undefined behavior si el objeto original es const.
- **Borrar ownership.** `void*` no dice quién libera el objeto. Contextos de callback y
  contenedores deben documentar si prestan o poseen.
- **Confundir function pointers.** No guardes function pointers en `void*` salvo que una
  API específica de plataforma lo garantice explícitamente.

## En la práctica

- **Pasá puntero borrado más metadata.** APIs con type erasure deberían llevar tamaño,
  count, supuestos de alineación y reglas de ownership en parámetros o un wrapper struct.
- **Usá `const void *` para input genérico read-only.** Comunica que el callee puede
  inspeccionar pero no mutar el objeto apuntado.
- **Usá `unsigned char *` para bytes crudos.** Es la herramienta portable de inspección y
  copia byte-a-byte.
- **Mantené los casts cerca de la frontera.** Casteá una vez al borde del callback o
  función genérica, después usá punteros tipados adentro.
- **Preferí wrappers tipados chicos para patrones repetidos de `void*`.** Un `Buffer`,
  `Slice` o `AnyView` vuelve visibles los contratos.

**Conecta con:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Aritmética de punteros y stride]] · [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|El heap: malloc/free y el allocator por debajo]] · [[lowlevel/pointers-and-memory/function-pointers|Function pointers]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — conversiones de object pointers, acceso con character types a representaciones de objetos, `void` y límites de undefined behavior. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Pointer declaration** — `void*`, conversiones de object pointers, null pointers y sintaxis de punteros multinivel. https://en.cppreference.com/w/c/language/pointer
- **cppreference — `qsort`** — ejemplo canónico de biblioteca estándar de sorting con type erasure vía `void*` y comparator callback. https://en.cppreference.com/w/c/algorithm/qsort
- **cppreference — `memcpy`** — copia orientada a bytes con parámetros `void*` y byte counts explícitos. https://en.cppreference.com/w/c/string/byte/memcpy
- **Jens Gustedt — *Modern C*** — tratamiento moderno de APIs genéricas en C, representaciones de objetos y disciplina de punteros. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Richard Reese — *Understanding and Using C Pointers*** — discusión práctica de `void*`, callbacks y contenedores genéricos en C. https://www.oreilly.com/library/view/understanding-and-using/9781449344535/
