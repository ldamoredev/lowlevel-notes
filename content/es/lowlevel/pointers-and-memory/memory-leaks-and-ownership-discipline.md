---
title: "Bug clásico: memory leaks y disciplina de ownership"
description: Un leak es storage asignado que ningún owner vivo puede liberar. La disciplina de ownership convierte `malloc` y `free` de llamadas dispersas en un lifecycle con un cleanup claro.
tags: [memory-leaks, ownership, malloc, free, realloc, cleanup]
order: 14
updated: 2026-06-30
---
# Bug clásico: memory leaks y disciplina de ownership

Un memory leak no es "se asignó memoria." Es "se asignó memoria y el programa perdió la
capacidad o la disciplina para liberarla." C te da primitivas de asignación, no un sistema
de ownership. Entonces lo construís en la API: quién posee esta asignación, quién puede
tomarla prestada, quién puede transferirla y qué camino de cleanup corre exactamente una
vez.

> El reset: `malloc` es fácil. Lo difícil es preservar un camino desde cada asignación
> exitosa hasta exactamente un cleanup correspondiente.

## Cómo funciona realmente

`malloc`, `calloc` y `realloc` devuelven storage con allocated lifetime. Ese storage dura
hasta que se libera. Si el programa pisa el último puntero owner, sale de un scope sin
cleanup o toma un early return que saltea cleanup, la asignación queda viva para el
allocator pero inalcanzable para el modelo de ownership del programa. Los programas de
larga vida lo sienten como uso creciente de memoria. Herramientas cortas pueden ocultarlo
hasta que los mismos hábitos caen en un daemon, game loop, componente de kernel o servicio
embebido.

Ownership es una convención de diseño encima de C:

| Rol | Significado |
|---|---|
| owner | responsable de liberar eventualmente el recurso |
| borrower | puede usar el recurso solo mientras el owner lo mantiene vivo |
| transfer | mueve la responsabilidad de liberar a otro owner |
| cleanup | el único camino de código que libera todos los recursos actualmente poseídos |

`realloc` merece cuidado especial. Si falla, devuelve `NULL` y deja intacta la asignación
original. Esto está mal:

```c
p = realloc(p, new_size);      /* pierde la asignación vieja si realloc falla */
```

Esta forma es más segura:

```c
void *next = realloc(p, new_size);
if (next == NULL) {
    /* p sigue siendo válido y todavía debe liberarse */
} else {
    p = next;
}
```

La misma regla aplica a todo recurso, no solo heap memory: file descriptors, mapped
memory, locks, archivos temporales, GPU buffers y handles del OS. C no impone un
destructor. Un bloque disciplinado de cleanup es cómo hacés visibles los lifetimes.

## Artefacto ejecutable: resize sin perder ownership

El demo vive en `examples/pointers-and-memory/memory-leaks-and-ownership-discipline/demo.c`.

```c
// demo.c — muestra disciplina de ownership: create/destroy, `realloc` mediante
// temporal y cleanup que devuelve el owner a estado vacio.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>
#include <stdlib.h>

struct OwnedInts {
    int *data;
    size_t count;
};

static int owned_ints_create(struct OwnedInts *owner, size_t count) {
    owner->data = calloc(count, sizeof *owner->data);
    if (owner->data == NULL) {
        owner->count = 0;
        return -1;
    }

    owner->count = count;
    for (size_t i = 0; i < count; i++) {
        owner->data[i] = (int)((i + 1) * 5);
    }

    return 0;
}

static void owned_ints_destroy(struct OwnedInts *owner) {
    free(owner->data);
    owner->data = NULL;
    owner->count = 0;
}

static int owned_ints_resize(struct OwnedInts *owner, size_t new_count) {
    int *new_data = realloc(owner->data, new_count * sizeof *owner->data);
    if (new_data == NULL) {
        return -1;
    }

    for (size_t i = owner->count; i < new_count; i++) {
        new_data[i] = (int)((i + 1) * 5);
    }

    owner->data = new_data;
    owner->count = new_count;
    return 0;
}

static int owned_ints_sum(const struct OwnedInts *owner) {
    int sum = 0;
    for (size_t i = 0; i < owner->count; i++) {
        sum += owner->data[i];
    }
    return sum;
}

int main(void) {
    struct OwnedInts owner = {0};

    if (owned_ints_create(&owner, 3) != 0) {
        perror("create");
        return 1;
    }

    printf("sum initial           = %d\n", owned_ints_sum(&owner));

    if (owned_ints_resize(&owner, 5) != 0) {
        owned_ints_destroy(&owner);
        perror("resize");
        return 1;
    }

    printf("sum after resize      = %d\n", owned_ints_sum(&owner));
    printf("owner before cleanup  = count %zu\n", owner.count);

    owned_ints_destroy(&owner);
    printf("owner after cleanup   = %s count=%zu\n",
           owner.data == NULL ? "NULL" : "live", owner.count);

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
sum initial           = 30
sum after resize      = 75
owner before cleanup  = count 5
owner after cleanup   = NULL count=0
```

El wrapper struct posee una asignación. La creación lo inicializa, el resize usa un
temporal para `realloc` y la destrucción devuelve el owner a estado vacío. No es un
container genérico completo. Es la unidad mínima de disciplina de ownership.

## Modos de falla y trade-offs

- **Pisar el único owner.** Asignar una nueva reserva al mismo puntero antes de liberar la
  vieja pierde la asignación vieja.
- **Early returns.** Las ramas de error que retornan antes del cleanup son el camino
  clásico al leak.
- **Construcción parcial.** Si el paso 4 de 6 falla, cleanup debe liberar pasos 1 a 3 e
  ignorar pasos 4 a 6.
- **Transferencia ambigua de ownership.** Si una función a veces roba un puntero y a veces
  solo lo toma prestado, los callers van a filtrar o hacer double-free.
- **Leaks escondidos por process exit.** El OS recupera memoria cuando un proceso termina,
  pero eso no ayuda a programas de larga vida ni enseña ownership correcto.
- **Over-cleanup.** Arreglar leaks liberando demasiado agresivamente puede crear
  use-after-free. Ownership y lifetime tienen que moverse juntos.

## En la práctica

- **Inicializá owners a estados vacíos.** `{0}` más un destructor que tolere estado vacío
  simplifica los caminos de cleanup.
- **Usá un único bloque de salida para funciones multi-step.** Es explícito, aburrido y
  confiable en C.
- **Asigná `realloc` a un temporal.** Nunca pierdas el owner original ante una falla de
  asignación.
- **Documentá transferencias en nombres o comentarios.** `take_buffer`, `borrow_buffer` y
  `clone_buffer` deberían significar cosas distintas.
- **Usá herramientas de leaks regularmente.** LeakSanitizer y Valgrind Memcheck encuentran
  caminos de cleanup perdidos antes que el tráfico de producción.
- **Preferí lifetimes locales cuando puedas.** Stack storage, arenas y buffers fijos
  reducen bordes de ownership cuando sus límites encajan con el trabajo.

**Conecta con:** [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|El heap: `malloc`/`free` y el allocator por debajo]] · [[lowlevel/pointers-and-memory/use-after-free-and-double-free|Use-after-free y double-free]] · [[lowlevel/pointers-and-memory/custom-allocators-arena-pool-and-bump|Allocators propios: arena, pool y bump]] · [[lowlevel/pointers-and-memory/pointers-to-pointers-double-indirection|Punteros a punteros]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **ISO WG14 N1570 — draft de C11** — allocated storage duration y contratos de biblioteca para funciones de asignación. https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- **cppreference — `malloc`, `calloc`, `realloc`, `free`** — referencia concisa de APIs de heap en C y casos de falla. https://en.cppreference.com/w/c/memory
- **Documentación de Clang LeakSanitizer** — modelo de detección de leaks e integración con AddressSanitizer. https://clang.llvm.org/docs/LeakSanitizer.html
- **Manual de Valgrind Memcheck** — clases de leaks, tracking de heap y diagnósticos prácticos. https://valgrind.org/docs/manual/mc-manual.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — contexto de allocator y memory-management desde sistemas. https://csapp.cs.cmu.edu/
- **Jens Gustedt — *Modern C*** — patrones pragmáticos de C para inicialización, cleanup y resource ownership. https://gustedt.gitlabpages.inria.fr/modern-c/
