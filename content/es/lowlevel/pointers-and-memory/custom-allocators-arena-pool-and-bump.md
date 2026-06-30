---
title: "Allocators propios: arena, pool y bump"
description: `malloc` general maneja lifetimes y tamaños desconocidos. Los allocators propios ganan cuando tu programa conoce la forma del lifetime: asignar muchas cosas juntas, reutilizar slots fijos o resetear una región entera de una vez.
tags: [allocators, arena, pool, bump, memory-management, performance]
order: 15
updated: 2026-06-30
---
# Allocators propios: arena, pool y bump

`malloc` general es un compromiso: tamaños arbitrarios, lifetimes arbitrarios, orden de
`free` arbitrario y muchos callers compartiendo un heap. Un allocator propio achica el
problema. Si tus objetos mueren juntos, usá un arena. Si todos tus objetos tienen un solo
tamaño, usá un pool. Si asignar es solamente "mover un cursor hacia adelante", usá un bump
allocator. El win no es magia; es hacer explícitas las restricciones de lifetime y tamaño.

> El reset: un allocator es una política sobre lifetime, layout y reutilización. La
> performance es consecuencia de elegir la política correcta para los datos.

## Cómo funciona realmente

Un **bump allocator** posee un buffer contiguo y un cursor. La asignación alinea el cursor,
devuelve la dirección actual y después avanza el cursor. No existen frees individuales.
Resetear el allocator mueve el cursor hacia atrás, invalidando todo lo asignado desde él.
Eso hace barata la asignación y casi gratis la liberación cuando todos los objetos
comparten un lifetime de fase.

Un **arena** suele ser un bump allocator empaquetado como región de lifetime. La región
puede usar un buffer fijo, encadenar varios bloques desde `malloc` o reservar memoria
virtual y commitear páginas bajo demanda. El diseño importante no es el backing store. Es
que el usuario dice: "todos estos objetos viven hasta que el arena se resetea o se
destruye."

Un **pool allocator** posee slots de un tamaño fijo. Los slots libres comúnmente se
encadenan dentro de los propios slots:

```c
struct PoolNode {
    struct PoolNode *next;
};
```

Cuando un slot está libre, sus bytes guardan el puntero al siguiente. Cuando está asignado,
el objeto del usuario ocupa esos bytes. Allocation popea un nodo de la free list. Free lo
empuja de vuelta. Esto da operaciones `O(1)` predecibles y evita fragmentación externa
para ese tamaño de objeto.

Todo allocator propio igual debe respetar las reglas de objetos de C. Las direcciones
devueltas necesitan alineación correcta para el tipo pedido. El storage debe alcanzar.
Resetear un arena termina el lifetime práctico de todo lo que contiene, aunque punteros
stale sigan guardando direcciones viejas. Un double-free en un pool puede corromper la
free list tan seguro como un double-free puede corromper metadata de `malloc`.

## Artefacto ejecutable: tiny arena y pool de tamaño fijo

El demo vive en `examples/pointers-and-memory/custom-allocators-arena-pool-and-bump/demo.c`.

```c
// demo.c — implementa un tiny arena/bump allocator y un pool de slots fijos para
// mostrar alineacion, reset masivo, exhaustion y reutilizacion de slots.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct Arena {
    unsigned char *base;
    size_t capacity;
    size_t used;
};

struct PoolNode {
    struct PoolNode *next;
};

struct Pool {
    struct PoolNode *free_list;
    unsigned char *storage;
    size_t slot_size;
    size_t slot_count;
};

union PoolSlot {
    struct PoolNode node;
    int value;
};

static size_t align_up(size_t value, size_t alignment) {
    size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

static void *arena_alloc(struct Arena *arena, size_t size, size_t alignment) {
    size_t start = align_up(arena->used, alignment);
    size_t end = start + size;

    if (end > arena->capacity) {
        return NULL;
    }

    arena->used = end;
    return arena->base + start;
}

static void arena_reset(struct Arena *arena) {
    arena->used = 0;
}

static void pool_init(struct Pool *pool,
                      unsigned char *storage,
                      size_t slot_size,
                      size_t slot_count) {
    pool->storage = storage;
    pool->slot_size = slot_size;
    pool->slot_count = slot_count;
    pool->free_list = NULL;

    for (size_t i = 0; i < slot_count; i++) {
        struct PoolNode *node = (struct PoolNode *)(void *)(storage + i * slot_size);
        node->next = pool->free_list;
        pool->free_list = node;
    }
}

static void *pool_alloc(struct Pool *pool) {
    if (pool->free_list == NULL) {
        return NULL;
    }

    struct PoolNode *node = pool->free_list;
    pool->free_list = node->next;
    return node;
}

static void pool_free(struct Pool *pool, void *slot) {
    struct PoolNode *node = slot;
    node->next = pool->free_list;
    pool->free_list = node;
}

int main(void) {
    unsigned char arena_storage[64];
    struct Arena arena = {arena_storage, sizeof arena_storage, 0};

    int *ids = arena_alloc(&arena, 3 * sizeof *ids, _Alignof(int));
    double *score = arena_alloc(&arena, sizeof *score, _Alignof(double));
    if (ids == NULL || score == NULL) {
        printf("arena allocation failed\n");
        return 1;
    }

    ids[0] = 10;
    ids[1] = 20;
    ids[2] = 30;
    *score = 9.5;

    printf("arena used            = %zu bytes\n", arena.used);
    printf("arena values          = %d %d %d %.1f\n",
           ids[0], ids[1], ids[2], *score);

    arena_reset(&arena);
    printf("arena after reset     = %zu bytes\n", arena.used);

    union PoolSlot pool_storage[3];
    struct Pool pool;
    pool_init(&pool, (unsigned char *)(void *)pool_storage, sizeof pool_storage[0], 3);

    int *a = pool_alloc(&pool);
    int *b = pool_alloc(&pool);
    int *c = pool_alloc(&pool);
    int *d = pool_alloc(&pool);

    if (a == NULL || b == NULL || c == NULL) {
        printf("pool allocation failed\n");
        return 1;
    }

    *a = 1;
    *b = 2;
    *c = 3;

    printf("pool values           = %d %d %d\n", *a, *b, *c);
    printf("pool exhausted        = %s\n", d == NULL ? "yes" : "no");

    pool_free(&pool, b);
    int *again = pool_alloc(&pool);
    if (again == NULL) {
        printf("pool reuse failed\n");
        return 1;
    }

    *again = 22;
    printf("pool reused slot      = %d\n", *again);

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
arena used            = 24 bytes
arena values          = 10 20 30 9.5
arena after reset     = 0 bytes
pool values           = 1 2 3
pool exhausted        = yes
pool reused slot      = 22
```

El arena asigna tres valores `int` y un `double`, pagando padding para mantener alineado
el `double`. Resetearla invalida las asignaciones en una sola operación. El pool tiene
tres slots de tamaño fijo; la cuarta asignación falla, liberar un slot lo vuelve
reutilizable y el allocator nunca le pide otro bloque al heap general.

## Modos de falla y trade-offs

- **Dangling después de resetear un arena.** Reset es un free masivo. Todo puntero al arena
  queda stale después.
- **Agrupar mal lifetimes.** Los arenas filtran por diseño hasta reset. Si un objeto debe
  vivir mucho más que la fase, pertenece a otro lado.
- **Bugs de alineación.** Un cursor que ignora `_Alignof(T)` puede devolver direcciones
  inválidas o lentas para dereferenciar.
- **Double-free en pool.** Empujar el mismo slot dos veces puede crear ciclos o asignación
  duplicada del mismo slot.
- **Desperdicio por tamaño fijo.** Los pools son rápidos para una size class, pero slots
  grandes desperdician memoria cuando la mayoría de los objetos son chicos.
- **Compartir entre threads.** Un arena o pool simple normalmente no es thread-safe.
  Agregar locks puede borrar el win o cambiar los patrones de contención.
- **Cleanup de recursos contenidos.** Resetear memoria cruda no cierra file descriptors,
  libera heap allocations anidadas ni corre destructores propios.

## En la práctica

- **Usá arenas para lifetimes de fase.** Parsear un archivo, construir un request, compilar
  una unidad o simular un frame son formas naturales.
- **Usá pools para churn de tamaño fijo.** Packets, AST nodes de una clase, jobs, particles
  y command objects chicos suelen encajar.
- **Mantené el heap general como baseline.** Reemplazá `malloc` solo donde el lifetime o el
  profiling muestren una razón real.
- **Hacé explícitos los puntos de reset.** El call site debería dejar obvio cuándo todos
  los punteros al arena se vuelven inválidos.
- **Guardá capacidad y bytes usados.** Los allocators propios necesitan bounds checks como
  cualquier otro código de buffers.
- **Instrumentá las primeras versiones.** Agregá counters, high-water marks y asserts
  antes de volver clever al allocator.

**Conecta con:** [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|El heap: `malloc`/`free` y el allocator por debajo]] · [[lowlevel/pointers-and-memory/memory-leaks-and-ownership-discipline|Memory leaks y disciplina de ownership]] · [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alineación y padding]] · [[lowlevel/pointers-and-memory/data-layout-and-cache-friendliness|Data layout y cache-friendliness]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **Ryan Fleury — *Untangling Lifetimes: The Arena Allocator*** — explicación clara de arena allocation como diseño de lifetimes, no solo truco de velocidad. https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator
- **Doug Lea — *A Memory Allocator*** — presiones de diseño detrás de `dlmalloc`: fragmentación, bins, locality y trade-offs. https://gee.cs.oswego.edu/dl/html/malloc.html
- **Dan Luu — *Malloc tutorial*** — walkthrough de un allocator chico que vuelve concretas metadata de heap y free lists. https://danluu.com/malloc-tutorial/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — dynamic memory allocation y allocators con implicit/explicit free-list. https://csapp.cs.cmu.edu/
- **cppreference — `aligned_alloc`** — referencia de API C para storage sensible a alineación. https://en.cppreference.com/w/c/memory/aligned_alloc
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — localidad, comportamiento de cache y por qué el layout del allocator afecta performance. https://www.akkadia.org/drepper/cpumemory.pdf
