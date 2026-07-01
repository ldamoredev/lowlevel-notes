---
title: "valgrind y profilers (perf, cachegrind)"
description: Valgrind, cachegrind y perf responden preguntas distintas a los sanitizers: corrección de memoria, costos simulados de cache/calls y performance counters de hardware.
tags: [toolchain, valgrind, perf, cachegrind, profiling, debugging]
order: 13
updated: 2026-06-22
---
# valgrind y profilers (perf, cachegrind)

Los sanitizers instrumentan tu build. Valgrind y los profilers observan la ejecución desde
ángulos distintos. Memcheck corre tu programa sobre la CPU sintética de Valgrind y rastrea
validez de memoria. Cachegrind simula comportamiento de cache y branches para explicar
costos de localidad. Linux `perf` samplea eventos reales de hardware/software desde el lado
del kernel. La regla práctica es hacer la pregunta correcta: "¿este acceso de memoria es
válido?", "¿dónde gasta ciclos el programa?" o "¿qué comportamiento de cache/branches
domina?"

> El reset: herramientas de corrección y herramientas de profiling no son intercambiables.
> Memcheck encuentra muchos bugs de memoria lentamente; `perf` mide ejecución real con poco
> overhead; cachegrind da una simulación determinística útil, pero no es el hardware.

## Cómo funciona realmente

Valgrind traduce machine code a una representación intermedia y lo corre bajo una
herramienta. Memcheck rastrea definedness, addressability, allocations, frees y leaks. Por
eso puede detectar lecturas no inicializadas e invalid frees en binarios que no compilaste
con ASan. El costo es alto: los programas pueden correr decenas de veces más lento.

Cachegrind es otra herramienta de Valgrind. Registra instruction counts y simula
comportamiento de cache y branches. Es excelente para comparar localidad algorítmica y hot
spots de llamadas de forma repetible, pero es un modelo. Máquinas reales tienen
prefetchers, caches compartidas, TLBs, cambios de frecuencia y ruido del OS.

`perf` es específico de Linux y usa performance events del kernel. Puede samplear CPU
cycles, instructions, cache misses, branches, page faults, context switches y call stacks.
Mide el binario real en la máquina real con mucho menos overhead que Valgrind, pero requiere
permisos Linux e interpretación cuidadosa.

| Herramienta | Mejor pregunta | Costo |
|---|---|---|
| Valgrind Memcheck | "¿Leí memoria inválida/no inicializada o filtré memoria?" | muy alto |
| Cachegrind | "¿Dónde están los costos de instructions/cache/branches en un modelo repetible?" | alto |
| Linux `perf stat` | "¿Qué contadores de eventos de hardware/software produjo esta corrida?" | bajo |
| Linux `perf record/report` | "¿Dónde cayó el tiempo sampleado?" | bajo a medio |
| Sanitizers | "¿La instrumentación del compilador puede detectar este bug en tests?" | medio |

## Artefacto ejecutable: workload más disponibilidad local

El ejemplo vive en `examples/toolchain-and-linking/valgrind-and-profilers-perf-cachegrind/`.
Este entorno macOS no tiene Valgrind, Cachegrind ni Linux `perf`, así que el script construye
y corre un workload determinístico, y después reporta disponibilidad local de herramientas
en vez de inventar output.

```c
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t mix(uint64_t value) {
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return value;
}

int main(void) {
    enum { count = 4096 };
    uint64_t *values = malloc(count * sizeof(*values));
    if (values == NULL) {
        return 1;
    }

    for (int i = 0; i < count; i++) {
        values[i] = mix((uint64_t)i + 1U);
    }

    uint64_t checksum = 0;
    for (int pass = 0; pass < 8; pass++) {
        for (int i = 0; i < count; i++) {
            checksum ^= mix(values[i] + (uint64_t)pass);
        }
    }

    printf("profile checksum: 0x%016llx\n", (unsigned long long)checksum);
    free(values);
    return 0;
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/valgrind-and-profilers-perf-cachegrind
./run.sh
```

Salida real de esta máquina:

```text
== build workload ==
profile checksum: 0xa57dea46fbef3b53
== tool availability on this machine ==
valgrind: not installed on this machine
perf: not installed on this machine
cachegrind: not available without valgrind
```

En Linux con las herramientas instaladas, los comandos para correr contra el mismo workload
son:

```bash
valgrind --tool=memcheck --leak-check=full ./demo
valgrind --tool=cachegrind ./demo
cg_annotate cachegrind.out.*
perf stat ./demo
perf record -g ./demo
perf report
```

En macOS, equivalentes aproximados son Instruments, `sample`, `leaks`, `vmmap`,
`dtrace`/DTrace donde esté permitido y la UI de profiling de Xcode. No son reemplazos
drop-in para Linux `perf`, y el soporte de Valgrind en macOS moderno es limitado.

## Valgrind vs sanitizers

ASan suele correr mucho más rápido que Memcheck y detecta muchos bugs de memoria en loops de
tests. Memcheck puede inspeccionar binarios no instrumentados y detecta usos de valores no
inicializados que ASan no apunta igual. ASan cambia la compilación; Memcheck cambia la
ejecución. Ninguno reemplaza disciplina de ownership ni tests.

TSan es el sanitizer para data races. Valgrind tiene Helgrind/DRD para análisis de threads,
pero TSan suele ser la primera opción cuando el soporte de compiler/runtime está disponible.
Para performance, usá profilers, no memory checkers.

## Modos de falla y trade-offs

- **Profilear debug builds miente.** `-O0` cambia la forma del programa. Profileá builds
  optimizados con símbolos, como `-O2 -g`.
- **Los microbenchmarks son frágiles.** Frecuencia de CPU, warmup, estado de cache, tamaño de
  input y ruido del OS pueden dominar corridas chicas.
- **Sampling necesita suficiente tiempo.** Un programa que corre por 2 ms puede no producir
  samples útiles con `perf record`.
- **Valgrind cambia la ejecución.** Es excelente para diagnostics de memoria, pero demasiado
  lento y sintético para conclusiones finales de performance.
- **Cachegrind es un modelo.** Tratálo como herramienta comparativa de localidad, no como
  modelo perfecto de tu CPU.
- **Importan los permisos.** Linux `perf` puede requerir cambios de `perf_event_paranoid` o
  root para algunos eventos.

## En la práctica

- **Usá Memcheck cuando ASan no está disponible o necesitás detalle de lecturas no
  inicializadas/leaks.**
- **Usá ASan/UBSan primero en loops rápidos de CI.** Encuentran muchos bugs de corrección
  antes.
- **Usá `perf stat` antes de `perf record`.** Los contadores de eventos te dicen si el
  programa es instruction-heavy, branchy, cache-missy o pesado en syscalls/context switches.
- **Usá Cachegrind para experimentos de localidad.** Sirve cuando comparás dos data layouts
  o estructuras de loops.
- **Siempre mantené símbolos.** Builds optimizados con debug info vuelven legible la salida
  de profilers sin destruir la forma optimizada.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/toolchain-and-linking/sanitizers-asan-ubsan-and-tsan|Sanitizers: ASan, UBSan y TSan]] · [[lowlevel/toolchain-and-linking/gdb-lldb-essentials-for-low-level-debugging|gdb / lldb esenciales]] · [[lowlevel/pointers-and-memory/data-layout-and-cache-friendliness|Data layout y cache-friendliness]] · [[lowlevel/machine-model/cache-lines-and-locality|Cache lines y locality]]

## Fuentes

- **Valgrind User Manual** — arquitectura de Valgrind, comportamiento de Memcheck y uso de herramientas. https://valgrind.org/docs/manual/manual.html
- **Valgrind Memcheck Manual** — invalid accesses, valores no inicializados, leaks y diagnostics. https://valgrind.org/docs/manual/mc-manual.html
- **Valgrind Cachegrind Manual** — simulación de cache/branches, `cg_annotate` e interpretación. https://valgrind.org/docs/manual/cg-manual.html
- **Linux `perf` wiki** — `perf stat`, `perf record`, modelo de eventos y overview de workflow. https://perfwiki.github.io/main/
- **Brendan Gregg — Linux perf examples** — patrones prácticos de comandos `perf` e interpretación. https://www.brendangregg.com/perf.html
