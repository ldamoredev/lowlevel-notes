---
title: "Data layout y cache-friendliness"
description: El código de memoria rápido empieza por el layout. Las cache lines premian acceso contiguo y predecible, y castigan pointer chasing, strides anchos, campos cold mezclados con campos hot y layouts que traen bytes que no usás.
tags: [data-layout, cache, locality, soa, aos, performance]
order: 10
updated: 2026-06-30
---
# Data layout y cache-friendliness

La CPU no trae campos individuales desde RAM uno por uno. Mueve cache lines: chunks chicos
contiguos, comúnmente de 64 bytes, que se vuelven rápidos si los reutilizás y caros si los
desperdiciás. Data layout es la decisión de qué bytes quedan al lado de qué otros bytes.
Un layout que coincide con el patrón de acceso del loop puede ser más rápido que código
ingenioso sobre un layout hostil. Acá punteros y memoria se convierten en performance.

> El reset: la localidad es una propiedad del patrón de acceso más el layout. Preguntá
> "cuando toco este byte, qué bytes vecinos también pagué para traer a cache?"

## Cache-friendly significa predecible y denso

Los wins habituales:

| Patrón | Por qué ayuda |
|---|---|
| arrays contiguos | hardware prefetchers y cache lines trabajan con el loop |
| stride chico por elemento | más elementos útiles por cache line |
| split hot/cold | los hot loops dejan de arrastrar campos raramente usados |
| structure of arrays (SoA) | loops sobre un campo leen un array denso |
| menos saltos de puntero | menos loads impredecibles y cache misses |

Las pérdidas habituales son lo opuesto: estructuras enlazadas dispersas por el heap,
arrays de structs grandes cuando el loop lee un solo campo, campos hot y cold mezclados y
patrones indirectos que derrotan el prefetching.

## Cómo funciona realmente

Supongamos que cada partícula tiene posiciones, velocidades y un flag `alive`:

```c
struct Particle {
    float x, y, z;
    float vx, vy, vz;
    int alive;
};
```

Un array de estos structs es un **array of structs** (AoS). Es cómodo cuando el código usa
una partícula completa por vez. Pero un loop que solo suma posiciones `x` vivas avanza por
`sizeof(struct Particle)` para leer un solo `float` y un flag. La mayoría de los bytes
traídos en cada cache line no le sirven a ese loop.

Una **structure of arrays** (SoA) guarda cada campo en un array separado:

```c
struct Particles {
    float *x, *y, *z;
    float *vx, *vy, *vz;
    int *alive;
};
```

Ahora un loop sobre `x[i]` y `alive[i]` recorre dos arrays densos. El stride de `x` es
`sizeof(float)`, no `sizeof(struct Particle)`. Eso mejora la localidad espacial y muchas
veces facilita vectorización. El trade-off es complejidad de API: crear, redimensionar y
pasar los datos ahora involucra varios arrays que deben mantenerse sincronizados.

No hay ganador universal. AoS suele ser mejor para código que consume registros completos:
parsear un packet, actualizar una entidad con todos sus campos o entregar un objeto a una
API. SoA suele ser mejor para loops numéricos que tocan el mismo campo en muchos
elementos. Layouts híbridos, como arrays de chunks chicos o splits hot/cold, son comunes
en sistemas reales.

Las estructuras con muchos punteros pagan otro costo. Un nodo de linked list puede tener
pocos bytes útiles, pero cada puntero `next` puede saltar a otra cache line o página. La
CPU no puede saber la próxima dirección hasta cargar el nodo actual, así que sufren el
memory-level parallelism y el prefetching. Los arrays hacen obvia la próxima dirección.

## Artefacto ejecutable: stride AoS vs stride SoA

El demo vive en `examples/pointers-and-memory/data-layout-and-cache-friendliness/demo.c`.

```c
// demo.c — compara AoS y SoA para el mismo calculo, mostrando el stride real que
// ve un loop cuando solo consume un campo hot.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>

struct ParticleAoS {
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float vz;
    int alive;
};

struct ParticlesSoA {
    float *x;
    float *y;
    float *z;
    float *vx;
    float *vy;
    float *vz;
    int *alive;
};

static float sum_alive_x_aos(const struct ParticleAoS *particles, size_t count) {
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (particles[i].alive) {
            sum += particles[i].x;
        }
    }
    return sum;
}

static float sum_alive_x_soa(struct ParticlesSoA particles, size_t count) {
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (particles.alive[i]) {
            sum += particles.x[i];
        }
    }
    return sum;
}

int main(void) {
    struct ParticleAoS aos[] = {
        {.x = 1.0f, .y = 0.0f, .z = 0.0f, .vx = 0.1f, .vy = 0.0f, .vz = 0.0f, .alive = 1},
        {.x = 2.0f, .y = 0.0f, .z = 0.0f, .vx = 0.1f, .vy = 0.0f, .vz = 0.0f, .alive = 0},
        {.x = 3.0f, .y = 0.0f, .z = 0.0f, .vx = 0.1f, .vy = 0.0f, .vz = 0.0f, .alive = 1},
        {.x = 4.0f, .y = 0.0f, .z = 0.0f, .vx = 0.1f, .vy = 0.0f, .vz = 0.0f, .alive = 1},
    };

    float x[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float y[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float z[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float vx[] = {0.1f, 0.1f, 0.1f, 0.1f};
    float vy[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float vz[] = {0.0f, 0.0f, 0.0f, 0.0f};
    int alive[] = {1, 0, 1, 1};

    struct ParticlesSoA soa = {
        .x = x, .y = y, .z = z,
        .vx = vx, .vy = vy, .vz = vz,
        .alive = alive,
    };

    size_t count = sizeof aos / sizeof aos[0];

    printf("AoS particle size     = %zu bytes\n", sizeof aos[0]);
    printf("AoS x stride          = %td bytes\n",
           (const char *)(const void *)&aos[1].x -
           (const char *)(const void *)&aos[0].x);
    printf("SoA x stride          = %td bytes\n",
           (const char *)(const void *)&x[1] -
           (const char *)(const void *)&x[0]);
    printf("sum alive x AoS       = %.1f\n", sum_alive_x_aos(aos, count));
    printf("sum alive x SoA       = %.1f\n", sum_alive_x_soa(soa, count));

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
AoS particle size     = 28 bytes
AoS x stride          = 28 bytes
SoA x stride          = 4 bytes
sum alive x AoS       = 8.0
sum alive x SoA       = 8.0
```

Ambos layouts computan el mismo resultado. El patrón de acceso no es el mismo. En AoS,
campos `x` adyacentes están a 28 bytes porque cada partícula lleva todos sus campos juntos.
En SoA, valores `x` adyacentes están a 4 bytes, así que una cache line contiene muchos
valores `x`.

## Modos de falla y trade-offs

- **Optimizar layout antes de conocer el loop.** Un layout es "friendly" solo relativo a
  un patrón de acceso. Empezá por el hot loop, no por un slogan.
- **Pointer chasing.** Listas, árboles y grafos pueden ser semánticamente correctos y aun
  así hostiles a cache. Usalos cuando inserción/deleción/topología importan más que scan
  speed.
- **Campos hot y cold juntos.** Strings raramente usados, debug data o metadata de
  ownership dentro de un struct hot pueden contaminar cada cache line.
- **Complejidad de SoA.** Varios arrays deben asignarse, redimensionarse y mantenerse en la
  misma longitud. Los bugs se mueven de "mal stride" a "arrays desincronizados."
- **False sharing después.** En código concurrente, campos hot adyacentes usados por
  distintos threads pueden pelear por la misma cache line.
- **Trampas de benchmarking.** Demos chicos entran en cache y mienten. Decisiones reales
  necesitan tamaños, patrones de acceso, flags de compilador y mediciones realistas.

## En la práctica

- **Diseñá datos alrededor del loop dominante.** Escribí qué campos lee y escribe el loop,
  después poné cerca esos bytes.
- **Preferí arrays para scans.** Si iterás todo frecuentemente, storage contiguo es el
  default.
- **Separá hot y cold data.** Mantené los campos chicos necesarios cada frame/request/packet
  lejos de metadata rara vez usada.
- **Elegí AoS para código de registro completo, SoA para loops numéricos por campo.** Usá
  híbridos cuando ambos lados tengan carga real.
- **Medí comportamiento de cache, no solo wall time.** Usá profilers y hardware counters
  cuando estén disponibles: cache misses, branch misses, ciclos y bandwidth cuentan la
  historia.
- **Mantené correctness primero.** Un AoS claro que es suficientemente rápido gana contra
  un SoA frágil con ownership confuso.

**Conecta con:** [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alineación y padding]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Aritmética de punteros y stride]] · [[lowlevel/machine-model/cache-lines-and-locality|Cache lines y localidad]] · [[lowlevel/machine-model/the-memory-hierarchy|La jerarquía de memoria]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — cache hierarchy, cache lines, prefetching, localidad y costos de acceso a memoria. https://www.akkadia.org/drepper/cpumemory.pdf
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — localidad, comportamiento de cache y jerarquía de memoria vista desde programas C. https://csapp.cs.cmu.edu/
- **Agner Fog — optimization manuals** — detalles prácticos de CPU sobre caches, acceso a memoria y vectorización. https://www.agner.org/optimize/
- **Mike Acton — *Data-Oriented Design and C++*** — charla canónica sobre diseñar layouts alrededor de patrones de acceso. https://www.youtube.com/watch?v=rX0ItVEVjHc
- **cppreference — Structure declaration** — reglas de layout de C que determinan tamaño de elementos AoS y offsets de campos. https://en.cppreference.com/w/c/language/struct
