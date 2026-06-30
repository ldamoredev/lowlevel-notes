---
title: "Qué es realmente un puntero"
description: Un puntero de C es un valor tipado que localiza un objeto. La dirección es la parte visible; el tipo, el lifetime, la alineación y las reglas de límites de array deciden si usarlo es válido o undefined behavior.
tags: [pointers, memory, addresses, c, types]
order: 1
updated: 2026-06-30
---
# Qué es realmente un puntero

Un puntero es un valor cuyo trabajo es localizar otro objeto. En el target x86-64 que usa
este atlas, ese valor normalmente se imprime como una dirección virtual de 64 bits; en C,
su significado portable es más estrecho: apunta a un objeto de un tipo particular, apunta
al one-past del último elemento de un objeto array, no apunta a nada (`NULL`), o no es un
valor de puntero válido en absoluto. El tipo no es decoración — le dice al compilador
cuántos bytes toca una dereferencia, qué alineación se requiere y cómo avanza la
aritmética de punteros. Un puntero, entonces, no es "solo un entero"; es un valor con
forma de dirección bajo reglas que el compilador puede explotar.

> El reset: `int *p = &x` significa "`p` guarda dónde vive un `int`." `p` es el puntero,
> `*p` es el objeto alcanzado a través de él, y `p + 1` significa "un `int` después", no
> "un byte después."

## La dirección no es toda la historia

La dirección es la parte que podés imprimir. El tipo y el lifetime son las partes que
deciden si usarla es válido.

| Pieza | Qué significa |
|---|---|
| `&x` | dirección del objeto `x`; si `x` tiene tipo `T`, el resultado tiene tipo `T *` |
| `p` | el valor de puntero en sí; copiarlo copia la ubicación, no el objeto |
| `*p` | dereferencia: el lvalue para el objeto alcanzado a través de `p` |
| `p + n` | aritmética de punteros en unidades de `sizeof *p`, válida solo dentro de un mismo objeto array o one-past |
| `NULL` | un valor de puntero nulo; compara distinto de todo puntero válido a objeto o función |

El tipo de puntero no lleva ownership, longitud ni tag de runtime. `int *p` no te dice si
el `int` apuntado está en el stack, en storage global, adentro de un bloque de `malloc` o
ya muerto. Solo dice que si `p` es válido, está alineado y apunta a un `int` vivo,
entonces `*p` puede usarse como lvalue de `int`.

Por eso la misma dirección impresa puede comportarse distinto a través de distintos tipos
de puntero. La aritmética de `int *` avanza por `sizeof(int)`. La aritmética de `char *`
avanza de a un byte, y `unsigned char *` es la forma estándar de inspeccionar la
representación cruda de un objeto. `void *` es un puntero genérico a objeto: cualquier
puntero a objeto puede hacer round-trip a través de él, pero no podés dereferenciar un
`void *` ni hacer aritmética portable sobre él hasta elegir de nuevo un tipo destino real.

Los function pointers son separados de los object pointers. En máquinas de escritorio
muchas veces parecen direcciones, pero las reglas de C para `void *`, representación de
objetos y aritmética no aplican sobre ellos. La primera pasada de esta rama trata punteros
a objeto; los function pointers tienen su propia nota más adelante.

## Cómo funciona realmente

Arrancá con un objeto real:

```c
int x = 7;
int *p = &x;
```

`x` ocupa `sizeof(int)` bytes en algún lugar del address space virtual del proceso. `p` es
otro objeto, con su propio storage, cuyo valor localiza a `x`. En un proceso x86-64 LP64
típico, `sizeof p` es 8 y `sizeof *p` es 4. El primer número es el tamaño del objeto
puntero; el segundo es el tamaño del objeto que obtenés después de seguirlo.

Cuando el compilador ve `*p = 42`, emite un store a través de la dirección guardada en
`p`, usando el tipo destino para elegir ancho de acceso y alineación. Para `int *`, eso
significa un store de `int`. Para `unsigned char *`, significa un store de un byte. El
tipo no vive en memoria al lado del objeto; es parte de la expresión que el compilador
está compilando.

Asignar punteros crea alias, no clones:

```c
int *q = p;
*q = 99;  // escribe x, porque q y p localizan el mismo int
```

Esta es la base de los out-parameters, las estructuras enlazadas y cada bug de memoria que
empieza con "dos nombres para el mismo storage." El lenguaje te deja llevar ubicaciones de
forma barata; no recuerda por vos quién las posee.

La aritmética de punteros también es tipada. Si `p` apunta a `a[0]` en un `int a[3]`,
entonces `p + 1` apunta a `a[1]`, no al byte siguiente. Los únicos destinos portables para
la aritmética son elementos del mismo objeto array y el puntero one-past usado para
terminar loops. Caminar afuera de ese límite es undefined behavior (comportamiento
indefinido), aunque la dirección numérica impresa parezca plausible.

Por último, un valor de puntero de C es local al proceso. Address Space Layout
Randomization (ASLR) cambia dónde caen los objetos entre corridas, y las direcciones
virtuales tienen sentido solo dentro del proceso en ejecución que posee ese address space.
Usá `uintptr_t` de `<stdint.h>` para diagnósticos, hashing o interfaces de bajo nivel
cuando la implementación lo provee; no serialices valores crudos de puntero en formatos de
archivo ni protocolos de red.

## Artefacto ejecutable: inspeccioná un puntero

El demo vive en `examples/pointers-and-memory/what-a-pointer-really-is/demo.c`.

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static void add_ten(int *slot) {
    if (slot != NULL) {
        *slot += 10;
    }
}

static void print_first_int_bytes(const int *value) {
    const unsigned char *bytes = (const unsigned char *)(const void *)value;

    printf("first int bytes       =");
    for (size_t i = 0; i < sizeof *value; i++) {
        printf(" %02x", bytes[i]);
    }
    printf("\n");
}

int main(void) {
    int cells[3] = {11, 22, 33};
    int *p = &cells[0];
    int *alias = p;
    void *generic = p;
    int *from_void = generic;
    int *none = NULL;

    ptrdiff_t element_delta = &cells[1] - &cells[0];
    ptrdiff_t byte_delta = (const unsigned char *)(const void *)&cells[1] -
                           (const unsigned char *)(const void *)&cells[0];

    printf("sizeof p              = %zu bytes\n", sizeof p);
    printf("sizeof *p             = %zu bytes\n", sizeof *p);
    printf("&cells[0]             = %p\n", (void *)&cells[0]);
    printf("&cells[1]             = %p\n", (void *)&cells[1]);
    printf("element delta         = %td element\n", element_delta);
    printf("byte delta            = %td bytes\n", byte_delta);
    printf("*p before             = %d\n", *p);

    *alias = 41;
    add_ten(p);

    printf("cells[0] after alias  = %d\n", cells[0]);
    printf("*(p + 1)              = %d\n", *(p + 1));
    printf("void* round trip      = %s\n",
           from_void == p ? "same pointer" : "different pointer");

#if defined(UINTPTR_MAX)
    uintptr_t bits = (uintptr_t)(void *)p;
    printf("uintptr_t round trip  = %s\n",
           (void *)bits == (void *)p ? "same bits" : "changed bits");
#endif

    print_first_int_bytes(p);
    printf("null comparison       = %s\n", none == NULL ? "NULL" : "not NULL");

    return 0;
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Una salida real del demo en una máquina de 64 bits little-endian:

```
sizeof p              = 8 bytes
sizeof *p             = 4 bytes
&cells[0]             = 0x7ff7b7c1729c
&cells[1]             = 0x7ff7b7c172a0
element delta         = 1 element
byte delta            = 4 bytes
*p before             = 11
cells[0] after alias  = 51
*(p + 1)              = 22
void* round trip      = same pointer
uintptr_t round trip  = same bits
first int bytes       = 33 00 00 00
null comparison       = NULL
```

Las dos direcciones difieren por 4 bytes porque `cells` es un `int[]` y el `int` de esta
máquina mide 4 bytes. El delta de elementos es 1 porque la resta de punteros reporta
elementos, no bytes. `alias` y `p` localizan el mismo `int`, así que escribir a través de
cualquiera cambia `cells[0]`. El primer byte es `0x33` porque `51` decimal es `0x33`, y
esta corrida es little-endian.

## Modos de falla y trade-offs

- **Puntero no inicializado.** `int *p; *p = 1;` usa un valor de puntero indeterminado. No
  es "algún lugar"; es undefined behavior.
- **Puntero colgado.** Un puntero puede sobrevivir al objeto que antes localizaba. Devolver
  la dirección de un local, usar un puntero después de `free` o conservar un puntero a una
  allocation redimensionada te deja con un valor que todavía puede imprimirse lindo y aun
  así ser inválido.
- **Dereferencia de null pointer.** `NULL` es un valor centinela real, no un objetito en la
  dirección cero. Sirve para "no apunta a nada"; dereferenciarlo es undefined behavior.
- **Aritmética afuera de un array.** `p + n` es portable solo dentro del mismo objeto array
  o one-past. Tratar la memoria como una calle gigante de bytes caminables es un hábito de
  máquina, no una garantía de C.
- **Tipo destino equivocado.** Castear una dirección a un tipo de puntero incompatible y
  dereferenciarla puede violar reglas de alineación y aliasing. `unsigned char *` es la
  vía de escape segura para inspección por bytes; el type punning arbitrario no.
- **Pensar puntero-como-entero.** `uintptr_t` sirve cuando está disponible, pero el entero
  no es estable entre procesos, no es un formato de archivo portable y no te da permiso
  para inventar punteros válidos desde números arbitrarios.

## En la práctica

- **Leé `T *p` como "`p` apunta a `T`."** El asterisco se pega al declarator, no al tipo
  base; `int *a, b;` declara un puntero y un `int` común.
- **Trackeá longitud y ownership junto al puntero.** Un puntero desnudo no sabe cuántos
  elementos puede acceder ni quién debe liberar el storage. Las APIs deberían llevar esa
  información explícitamente.
- **Casteá a `void *` para `%p`.** `printf("%p", (void *)p)` es la forma portable de
  imprimir punteros a objeto.
- **Usá `const` para decir "no voy a escribir a través de este puntero."** `const int *p`
  protege el objeto apuntado por ese camino de acceso; no vuelve inmortal la dirección ni
  prueba ownership.
- **Debugueá bugs de punteros con lifetimes, no solo direcciones.** El número puede parecer
  correcto mientras el objeto está muerto, fuera de bounds, desalineado o accedido a través
  del tipo equivocado.

**Conecta con:** [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/process-address-space-and-virtual-memory|Address space de proceso y memoria virtual]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre tipos de puntero, address-of, indirección, null pointers, conversiones de punteros, aritmética de punteros y límites de undefined behavior. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Pointer declaration** — referencia compacta para object pointers, function pointers, `void *`, null pointers y sintaxis de puntero a puntero. https://en.cppreference.com/w/c/language/pointer
- **cppreference — Operator arithmetic** — las reglas exactas para suma, resta, punteros one-past y límites de array. https://en.cppreference.com/w/c/language/operator_arithmetic
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, caps. 2, 3 y 9 — bytes, direcciones, código máquina x86-64 y address spaces virtuales. https://csapp.cs.cmu.edu/
- **System V AMD64 ABI** — el modelo de datos LP64 y los supuestos de tamaño de punteros a objeto para el target x86-64 Unix-like usado en todo el atlas. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Jens Gustedt — *Modern C*** — tratamiento moderno de tipos de puntero, lifetimes de objetos, `const` y disciplina de APIs puntero-más-tamaño. https://gustedt.gitlabpages.inria.fr/modern-c/
