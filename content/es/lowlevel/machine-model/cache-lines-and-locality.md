---
title: "Cache lines y localidad"
description: La cache mueve memoria en líneas de 64 bytes, no en bytes — así que un cache miss cuesta ~100× un hit, y el mismo loop puede correr 25× más lento solo cambiando el orden en que tocás la memoria. Qué es una cache line, por qué un miss es tan caro y un benchmark en C que lo demuestra.
tags: [machine-model, cache, cache-line, locality, performance]
order: 5
updated: 2026-06-22
---
# Cache lines y localidad

La [[lowlevel/machine-model/the-memory-hierarchy|jerarquía de memoria]] te contó *que*
las caches viven entre la CPU y la RAM. Esta nota es sobre el único hecho mecánico que
las hace funcionar — y que las hace morder. La cache nunca mueve un byte. Mueve un
bloque fijo llamado **cache line** (64 bytes en x86-64), siempre. Leés un `int` y te
llevás sus 63 vecinos gratis; tocás la memoria en un orden que ignora esas líneas y
pagás latencia completa de RAM en casi cada acceso. Como un miss cuesta más o menos
**100× un hit**, el *orden* en que recorrés la memoria puede cambiar la velocidad de un
programa por 10–25× sin cambiar una sola operación que ejecuta.

> El reset: no le podés pedir a la cache que guarde "esta variable". Te da líneas. El
> código rápido es código cuyo patrón de acceso usa cada byte de cada línea que paga,
> antes de que esa línea sea desalojada. Ese es todo el juego.

## La cache line es la unidad de transferencia

Cuando la CPU necesita una dirección que no está en cache, **no** trae ese byte. Trae
la línea entera de 64 bytes alineada que lo contiene desde el nivel de abajo, y la
instala en cache. Consecuencias:

- **Los vecinos son gratis.** Después de leer `a[0]`, los bytes de `a[1]..a[15]` (una
  línea de `int[16]`) ya están cacheados. El acceso secuencial consigue 15 hits al
  precio de un miss.
- **La línea es la granularidad de todo.** El desalojo, la coherencia entre cores y el
  "¿esto está cacheado?" ocurren todos por línea, nunca por byte.
- **El acceso desalineado/disperso desperdicia la línea.** Si usás solo 4 de los 64
  bytes antes de que la línea sea desalojada, pagaste un miss completo para transportar
  60 bytes que tiraste.

Una cache es **set-associative**: cada línea mapea a un set de unos pocos slots; el
hardware mantiene un **tag** por slot para saber qué dirección vive ahí ahora, y desaloja
(normalmente) la línea menos usada recientemente cuando un set se llena. Vos no manejás
nada de esto — pero saber que la línea es de 64 bytes alcanza para razonar casi
cualquier efecto de cache. (El tamaño de línea es un detalle de la ISA/microarquitectura:
64 bytes en x86-64 y en la mayoría de ARM, pero **128 bytes en Apple Silicon** —
verificá con `sysctl hw.cachelinesize` en macOS o `getconf LEVEL1_DCACHE_LINESIZE` en
Linux.)

## Por qué un miss cuesta ~100× un hit

El número sale directo de las latencias de la jerarquía:

| Resultado | Dónde está el dato | Latencia aprox. |
|---|---|---|
| Hit en L1 | ya en L1 | ~4 ciclos (~1 ns) |
| Hit en L2 | un nivel abajo | ~12 ciclos (~4 ns) |
| Hit en LLC (L3) | cache compartida | ~40 ciclos (~15 ns) |
| **Miss → RAM** | memoria principal | **~200–300 ciclos (~100 ns)** |

Un hit de L1 alimenta el pipeline en un ciclo o dos; un miss completo frena ~100 ns —
en un core de 3 GHz eso es **el tiempo de ~300 instrucciones** que la CPU podría haber
estado ejecutando. La ejecución out-of-order y el prefetching ocultan *parte* de esto
cuando el patrón de acceso es predecible (streaming secuencial), que es justo por qué el
prefetcher hizo tan grande la brecha aleatorio-vs-secuencial en la nota anterior. Pero un
miss dependiente que la CPU no puede predecir — un pointer chase, un índice aleatorio —
expone el ~100× completo.

## El mismo trabajo, 25× más lento: filas vs columnas

Nada demuestra mejor las cache lines que recorrer una matriz de dos formas. Los dos
loops leen cada elemento exactamente una vez y los suman — idéntica cantidad de
operaciones. Solo difiere el *orden*. Compilá y corré:

```c
// cache_lines.c — trabajo idéntico, órdenes de recorrido opuestos.
// gcc -O2 -Wall -Wextra cache_lines.c -o cache_lines && ./cache_lines
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(void) {
    const int N = 8192;                       // 8192 x 8192 ints = 256 MB
    int *m = malloc((size_t)N * N * sizeof(int));
    if (!m) { perror("malloc"); return 1; }
    for (long i = 0; i < (long)N * N; i++) m[i] = 1;

    volatile long sink = 0;
    double t0 = now_ms();
    long s = 0;
    for (int i = 0; i < N; i++)               // row-major: stride 1, recorre cada línea
        for (int j = 0; j < N; j++) s += m[(long)i * N + j];
    double t1 = now_ms(); sink = s;

    s = 0;
    for (int j = 0; j < N; j++)               // column-major: stride N, línea nueva por paso
        for (int i = 0; i < N; i++) s += m[(long)i * N + j];
    double t2 = now_ms(); sink = s;

    printf("row-major  (stride 1) : %8.1f ms\n", t1 - t0);
    printf("col-major  (stride N) : %8.1f ms\n", t2 - t1);
    printf("slowdown              : %8.1fx\n", (t2 - t1) / (t1 - t0));
    return sink == -1;
}
```

Una corrida warm en una laptop Intel Core i7: **row-major ~35 ms, column-major ~870 ms —
unas 25× más lento**, las mismas sumas. Row-major marcha a lo largo de la memoria, así
que cada línea de 64 bytes se carga una vez y se usan los 16 `int` que tiene antes de
seguir. Column-major salta `N` ints (32 KB) en cada paso, así que cada acceso cae en una
línea *fresca*, usa 4 de sus 64 bytes y es desalojada mucho antes de que vuelvas a ella —
esencialmente un miss por elemento. Dá vuelta los loops (o los datos) y recuperás el
orden de magnitud.

## La localidad es una propiedad que se diseña

No podés agregar localidad después; está horneada en tu data layout y tu recorrido:

- **Array of structs (AoS) vs struct of arrays (SoA).** Recorrer `particle[i].x` para un
  millón de partículas desperdicia la mayor parte de cada línea en los `y`, `z`, `mass`…
  que no leíste. Separar los campos calientes en sus propios arrays (SoA) los empaqueta
  densos por línea. Este es el corazón del *data-oriented design*.
- **Hot/cold splitting.** Mantené juntos los campos que se tocan seguido y empujá los
  campos rara vez usados a un struct "frío" separado, así el camino caliente llena
  líneas con datos útiles.
- **Empaquetá y alineá.** El layout, el padding y la alineación de un struct deciden
  cuántos entran por línea — lo vemos cuando lleguemos a
  [[lowlevel/pointers-and-memory/index|punteros y memoria]].

## Modos de falla y trade-offs

- **El Big-O puede mentir.** Una linked list y un array las dos "iteran en O(n)", pero el
  array suele ser 10×+ más rápido porque streamea líneas contiguas mientras la lista
  persigue punteros a nodos fríos y dispersos — un miss por salto.
- **Los strides grandes derrotan a la línea.** Cualquier stride ≥ al tamaño de línea (16
  `int`) significa un elemento útil por línea. El acceso column-major, los hash buckets y
  la transpuesta de matriz ingenua pegan todos contra esto.
- **False sharing (una trampa multicore).** Dos cores escribiendo variables *distintas*
  que comparten una cache line hacen ping-pong de la línea entre sus caches, serializando
  lo que parecía paralelo. El fix es padear el dato caliente por-thread a su propia línea
  — lo revisitamos en concurrencia.
- **Es invisible en el fuente.** Dos loops con C idéntico pueden diferir 25× en runtime.
  Solo los contadores de cache-miss de un profiler (o un benchmark como el de arriba) lo
  revelan.

## En la práctica

- **Recorré la memoria como está dispuesta.** Para los arrays row-major de C, hacé que el
  *último* índice sea el loop interno. Esta sola regla arregla una fracción enorme de los
  slowdowns accidentales.
- **Preferí datos contiguos y densos por línea.** Arrays de structs chicos recorridos en
  orden le ganan a grafos enlazados por punteros siempre que puedas arreglarlo.
- **Medí misses, no solo tiempo.** `perf stat -e cache-misses,cache-references ./prog` en
  Linux, o Instruments/`Cachegrind`, te dice si estás line-bound.
- **Pensá en chunks de 64 bytes.** Cuando leés un campo, pagaste su línea entera —
  diseñá para que el resto de esa línea sea dato que también querés.

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/the-memory-hierarchy|La jerarquía de memoria]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|La CPU: fetch–decode–execute]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 6 — organización de la cache, líneas, set-associativity y localidad; la columna de esta nota. https://csapp.cs.cmu.edu/
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — cache lines, asociatividad y patrones de acceso medidos en profundidad. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- **Igor Ostrovsky — *Gallery of Processor Cache Effects*** — experimentos cortos y ejecutables que exponen tamaño de línea, asociatividad y false sharing. https://igoro.com/archive/gallery-of-processor-cache-effects/
- **Mike Acton — *Data-Oriented Design and C++* (CppCon 2014)** — por qué el data layout, no la abstracción, maneja la performance real. https://www.youtube.com/watch?v=rX0ItVEVjHc
- **Intel 64 and IA-32 Architectures Optimization Reference Manual** — tamaño de cache-line autoritativo, comportamiento de prefetch y guía de false-sharing en x86-64. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
