---
title: "Const-correctness y qué promete realmente `const`"
description: `const` en un tipo de puntero dice qué camino de acceso no puede escribir. Es una promesa de compile time sobre mutación a través de esa expresión, no ownership, lifetime, inmutabilidad ni thread-safety.
tags: [const, pointers, qualifiers, api-design, c]
order: 7
updated: 2026-06-30
---
# Const-correctness y qué promete realmente `const`

`const` es una promesa sobre escrituras a través de un camino de acceso particular.
`const int *p` significa "no modifiques el `int` a través de `p`"; no significa que el
objeto sea globalmente inmutable, poseído por el callee, vivo para siempre o thread-safe.
`int * const p` significa que el objeto puntero en sí no puede reseatearse.
Const-correctness es la disciplina de poner esas promesas en fronteras de API para que el
código read-only no pueda volverse write-capable por accidente.

> El reset: `const` se pega a un nivel de indirección. Preguntá "qué objeto está
> protegido: lo apuntado, la variable puntero o ambos?"

## Leé la declaración por niveles

Los casos comunes:

| Declaración | ¿Puede reseatear el puntero? | ¿Puede escribir a través de él? |
|---|---:|---:|
| `int *p` | sí | sí |
| `const int *p` | sí | no |
| `int const *p` | sí | no |
| `int * const p` | no | sí |
| `const int * const p` | no | no |

`const int *` e `int const *` son el mismo tipo. Vuelven read-only al `int` apuntado a
través de ese puntero. `int * const` fija el objeto puntero pero todavía te deja modificar
el `int` al que apunta.

Para punteros multinivel, cada nivel importa:

```c
const char **a;        // puede cambiar *a; no puede escribir **a
char * const *b;       // no puede cambiar *b; puede escribir **b
const char * const *c; // no puede cambiar *c ni escribir **c
```

Acá el lenguaje vago tipo "puntero const" falla. Nombrá el nivel.

## Cómo funciona realmente

`const` es un type qualifier chequeado por el compilador. Puede prevenir escrituras
accidentales:

```c
int sum(const int *items, size_t count);
```

Esa firma dice que `sum` no va a mutar los elementos a través de `items`. Puede aceptar
arrays mutables y arrays const porque un objeto mutable puede verse a través de un puntero
read-only. Lo inverso no es seguro: un puntero a datos const no debe volverse
write-capable silenciosamente.

La promesa es local al camino de acceso. En este programa, el objeto cambia aunque exista
una vista const:

```c
int x = 1;
const int *view = &x;
x = 2;        // legal: acceso mutable directo
```

Eso no es una contradicción. `view` prometió no escribir; no congeló `x`.

Los objetos declarados `const` son más fuertes:

```c
const int limit = 10;
```

Escribir a `limit` a través de un puntero casteado es undefined behavior. Los compiladores
pueden ubicar objetos const en memoria read-only o asumir que no cambian. Castear `const`
solo es válido si el objeto original no fue declarado const y estás restaurando un camino
mutable que poseés legítimamente.

`const` tampoco maneja lifetime. Un `const char *` puede quedar dangling. Tampoco vuelve
thread-safe a datos compartidos; dos threads pueden racear sobre un objeto mutable aunque
una API lo vea a través de un puntero `const`.

## Artefacto ejecutable: const en distintos niveles

El demo vive en `examples/pointers-and-memory/const-correctness-and-what-const-really-promises/demo.c`.

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Salida real:

```
read through const    = 10
reseated const view   = 20
wrote via fixed ptr   = 11
b after reseat write  = 21
sum read-only array   = 10
name                  = low-level atlas
```

La vista `const int *` puede reseatearse de `a` a `b`, pero no puede escribir a través de
esa vista. El puntero `int * const` no puede reseatearse, pero todavía puede escribir en
`a`. La API de suma read-only acepta un array mutable común porque el acceso read-only es
una restricción segura.

## Modos de falla y trade-offs

- **Pensar que `const` significa inmutable.** Normalmente significa "no a través de esta
  expresión." Otros aliases todavía pueden mutar un objeto no-const.
- **Castear un const real.** Si el objeto original fue declarado `const`, escribir a través
  de un puntero casteado es undefined behavior.
- **Olvidar lifetime.** `const char *p` puede apuntar a un string literal, un buffer de
  stack, una allocation de heap o storage muerto. Const no dice nada sobre lifetime.
- **Sorpresas de const superficial.** `const struct Table *t` previene escrituras a campos
  a través de `t`, pero cualquier campo puntero interno puede apuntar a objetos mutables si
  sus tipos no están también const-qualified.
- **Mismatch de qualifiers en APIs.** Una función que solo lee pero acepta `T *` rechaza
  callers const y anuncia capacidad de escritura que no necesita.
- **Const como teatro de cleanup.** Repartir `const` en scopes locales mínimos vale menos
  que hacer read-only correctamente las fronteras de API.

## En la práctica

- **Poné `const` en buffers de input.** Preferí `void parse(const char *text)` e
  `int sum(const int *items, size_t count)` para acceso read-only.
- **Devolvé `const T *` para vistas prestadas read-only.** Dice que callers pueden
  inspeccionar pero no mutar a través de ese puntero.
- **Usá `T * const` con moderación.** Puede aclarar invariantes locales, pero la constness
  de API suele importar más.
- **No casteés `const` para satisfacer una mala API.** Arreglá la API cuando puedas; si no,
  aislá y documentá la frontera insegura.
- **Propagá const en datos anidados deliberadamente.** Si un grafo, tabla o array de
  strings debería ser read-only a través de una API, calificá también las capas apuntadas.

**Conecta con:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero]] · [[lowlevel/pointers-and-memory/pointers-to-pointers-double-indirection|Punteros a punteros]] · [[lowlevel/pointers-and-memory/void-star-and-type-erasure|void* y type erasure]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — type qualifiers, reglas de constraints, acceso por lvalue y undefined behavior después de modificar objetos const. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — `const` type qualifier** — referencia compacta para ubicación de qualifiers, niveles de puntero y constraints. https://en.cppreference.com/w/c/language/const
- **cppreference — Pointer declaration** — sintaxis de declarators de puntero y ejemplos de const multinivel. https://en.cppreference.com/w/c/language/pointer
- **Jens Gustedt — *Modern C*** — const-correctness práctica a nivel API y disciplina de qualification de punteros. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Robert Seacord — *Effective C*** — guía de C seguro sobre interfaces const-correct, casts y evitar mutación accidental. https://nostarch.com/Effective_C
