---
title: "La jerarquía de memoria"
description: La memoria no es una sola cosa plana — es una pirámide de almacenes que cambian tamaño por velocidad, de los registros a la cache L1/L2/L3, a la RAM y al disco. Por qué existe la brecha, los números de latencia que importan y un benchmark en C para ver el precipicio.
tags: [machine-model, memory, cache, latency, performance]
order: 4
updated: 2026-06-22
---
# La jerarquía de memoria

Cuando escribís `x + y`, un lenguaje administrado te deja imaginar una "memoria"
uniforme a la que la CPU llega a un costo constante. Esa imagen es una mentira que el
hardware trabaja muy duro para sostener. El almacenamiento real es una **pirámide**:
unos pocos registros que son prácticamente gratis, y después almacenes cada vez más
grandes y más lentos — cache L1, L2, L3, RAM principal y, por último, disco. Cada nivel
hacia abajo es más o menos un orden de magnitud más grande y un orden de magnitud más
lento. Toda la máquina está diseñada alrededor de una apuesta: que el dato que vas a
tocar próximamente está probablemente cerca del que acabás de tocar, así que un almacén
chico y rápido puede hacerse pasar por uno enorme y lento la mayor parte del tiempo.

> El reset: "leer una variable" no es una sola operación con un solo costo. La misma
> línea de C puede tardar 4 ciclos o 300 según *dónde vive el valor ahora mismo*, y por
> lo general no lo ves en el fuente. El trabajo de performance es en gran parte el arte
> de mantener los datos arriba en esta pirámide.

## Por qué existe una jerarquía

La memoria rápida es cara y físicamente chica; la memoria grande es barata y está lejos.
No podés tener un almacén que sea a la vez enorme, rápido y accesible — la velocidad de
la luz y la economía de los transistores lo prohíben. Así que el hardware lo finge
apilando varios almacenes y **moviendo automáticamente el dato caliente hacia arriba**.
Los registros los decide el compilador; las caches las maneja la CPU de forma
transparente; el movimiento RAM↔disco lo maneja el OS (paginación). En general no
controlás las caches directamente — las influís por *cómo accedés a la memoria*.

| Nivel | Latencia típica | Tamaño típico | Manejado por |
|---|---|---|---|
| Registro | ~0 ciclos (*es* el operando) | ~16 GPRs × 8 B | Compilador |
| Cache L1 | ~4 ciclos (~1 ns) | 32–64 KB por core | Hardware |
| Cache L2 | ~12 ciclos (~4 ns) | 256 KB–2 MB por core | Hardware |
| Cache L3 | ~40 ciclos (~15 ns) | 8–64 MB compartida | Hardware |
| RAM (DRAM) | ~100–300 ciclos (~60–100 ns) | 8–128 GB | OS (memoria virtual) |
| SSD | ~100 µs | 0.25–4 TB | OS (filesystem) |
| Disco rígido | ~10 ms | TBs | OS (filesystem) |

Los números varían según el chip, pero la lección son las **proporciones**. Un hit de
L1 contra un acceso a RAM es una diferencia de más o menos **1:100**; de RAM a SSD hay
otros ~1000×; de SSD a un disco rígido otros ~100×. Si un acceso a registro fuera un
segundo, un hit de L1 serían unos pocos segundos, la RAM un par de minutos, una lectura
de SSD un día y un seek de disco rígido meses. Esa escala mental es por qué "es solo
memoria" nunca es solo memoria.

## Localidad: la apuesta que lo hace funcionar

Las caches rinden solo porque los programas reales exhiben **localidad de referencia**:

- **Localidad temporal** — si tocaste una dirección, es probable que la vuelvas a tocar
  pronto (un contador de loop, un campo caliente de un struct). Mantenela cacheada y la
  reutilización gana.
- **Localidad espacial** — si tocaste una dirección, es probable que toques pronto a sus
  vecinas (el próximo elemento del array). Por eso las caches no mueven bytes sueltos;
  mueven una **cache line** entera (64 bytes en x86-64; 128 en Apple Silicon) de una.

Por esto *cómo* recorrés la memoria domina la performance. Recorrer un array en orden
usa cada byte de cada línea traída antes de seguir — localidad espacial casi perfecta.
Saltar al azar tira la mayor parte de cada línea y paga un miss fresco de alta latencia
casi en cada acceso. El mismo dato, la misma cantidad total de bytes, velocidad
radicalmente distinta. (La mecánica de las cache lines y el costo de un miss tienen su
propia nota a continuación.)

## Mirá el precipicio: un benchmark ejecutable

Este programa cronometra una cantidad fija de accesos contra un working set que crece de
unos pocos KB a decenas de MB. Mientras el set entra en L1, los accesos son rápidos; a
medida que se derrama a L2, L3 y después RAM, el tiempo por acceso sube escalón a
escalón en cada frontera. Compilá y corré:

```c
// hierarchy.c — observá crecer la latencia por acceso a medida que el working set
// deja cada cache.
// gcc -O2 -Wall -Wextra hierarchy.c -o hierarchy && ./hierarchy
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    const long ACCESSES = 256L * 1024 * 1024;   // trabajo fijo por tamaño
    volatile int sink = 0;
    printf("%10s  %12s\n", "size(KB)", "ns/access");

    for (long kb = 4; kb <= 64L * 1024; kb *= 2) {
        long n = kb * 1024 / (long)sizeof(int);
        int *a = malloc((size_t)n * sizeof(int));
        if (!a) { perror("malloc"); return 1; }

        // Armá UN solo ciclo aleatorio grande (algoritmo de Sattolo). Un orden
        // *aleatorio* es lo que derrota al prefetcher: así cada carga espera de verdad
        // a la anterior.
        for (long i = 0; i < n; i++) a[i] = (int)i;
        for (long i = n - 1; i > 0; i--) {
            long j = rand() % i;
            int t = a[i]; a[i] = a[j]; a[j] = t;
        }

        int idx = 0;
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (long i = 0; i < ACCESSES; i++) idx = a[idx];   // chase dependiente
        clock_gettime(CLOCK_MONOTONIC, &t1);

        double ns = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
        printf("%10ld  %12.2f\n", kb, ns / (double)ACCESSES);
        sink = idx;          // fuerza a observar el chase (evita dead-code elimination)
        free(a);
    }
    return sink == -1;
}
```

Mientras el array entra en L1/L2 vas a ver un ~2 ns más o menos plano; a medida que se
derrama a L3 el tiempo trepa de forma sostenida, y una vez que el working set ya no entra
en cache y cada chase espera a la RAM sube hacia ~100+ ns — una corrida real en una
laptop Intel Core i7 dio `2.3 → 4.6 → 12.6 → 33.7 → 99.5 → 164.5 ns` de 32 KB a 64 MB.
Acabás de *medir* la jerarquía — sin profiler. Dos detalles lo hacen funcionar, y los dos
son fáciles de errar: el ciclo **aleatorio** de Sattolo derrota al prefetcher (un anillo
secuencial `(i+1)%n` se queda en ~2 ns aun a 64 MB porque la CPU lo predice), y la cadena
dependiente `idx = a[idx]` (con el sink `volatile`) impide que `-O2` solape las cargas o
borre el loop. Sacá cualquiera de los dos y la curva se aplana y oculta el efecto.

## Modos de falla y trade-offs

- **El supuesto de memoria plana.** Algoritmos comparados solo por cantidad de
  operaciones pueden perder feo en la práctica: una linked list y un `std::vector`/array
  pueden tener el mismo Big-O, pero el array gana enormemente porque streamea cache lines
  contiguas mientras la lista persigue punteros por toda la RAM.
- **Working set, no tamaño total.** Lo que importa es cuánta memoria está *caliente* en
  una ventana ajustada, no cuánto reservaste. Un array de 1 GB tocado secuencialmente
  puede ser cache-friendly; una estructura de 1 MB tocada al azar puede no serlo.
- **No podés cachear tu salida de malos patrones de acceso.** Las caches son automáticas
  pero no mágicas. El acceso al azar, el pointer chasing y los strides grandes las
  derrotan; el fix es el data layout, no una cache más grande.
- **El disco es otro universo.** Una vez que el OS tiene que paginar a SSD o disco,
  dejaste el mundo de los nanosegundos por micro/milisegundos. El thrashing (paginación
  constante) hace que una máquina se sienta congelada aun con la CPU mayormente ociosa.

## En la práctica

- **Pensá en términos de "dónde vive esto ahora mismo".** Leer el mismo valor en un loop
  es barato; tocar una cache line fresca en cada iteración no lo es.
- **Contiguo y secuencial le gana a astuto y disperso.** Arrays de structs recorridos en
  orden son el camino rápido por defecto; esta es la semilla del *data-oriented design*.
- **Medí, no adivines.** El benchmark de arriba, `perf stat` (contadores de cache-miss) y
  Cachegrind te dicen dónde estás en la pirámide. La intuición de latencia suele errar
  por 10–100×.
- **Esto es el sustrato de todo el atlas.** El stack es rápido en parte porque siempre
  está caliente en cache; `malloc` puede ser lento en parte porque la memoria fresca del
  heap está fría. Tené esta pirámide en mente cuando lleguemos a los allocators y a la
  performance.

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/registers-and-the-isa|Registros y la ISA]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|La CPU: fetch–decode–execute]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 6 — la jerarquía de memoria, la localidad y la organización de la cache; la columna de esta nota. https://csapp.cs.cmu.edu/
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — el deep dive definitivo sobre DRAM, caches y patrones de acceso, con mediciones. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- **Latency Numbers Every Programmer Should Know** (Jeff Dean / Peter Norvig, versión "by year") — la tabla canónica de latencias por orden de magnitud. https://colin-scott.github.io/personal_website/research/interactive_latency.html
- **Agner Fog — *The microarchitecture of Intel, AMD and VIA CPUs*** — tamaños y latencias de cache reales, y cómo se comporta la jerarquía por microarquitectura. https://www.agner.org/optimize/
- **Intel 64 and IA-32 Architectures Optimization Reference Manual** — comportamiento autoritativo de la cache, tamaños de línea y prefetch en x86-64. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
