---
title: "El address space del proceso y la memoria virtual"
description: Por qué cada proceso ve su propia memoria limpia y privada desde la dirección 0 hacia arriba — la ilusión que el OS y la MMU construyen sobre la RAM física. Páginas, traducción virtual-a-física, aislamiento, y un demo con fork donde la misma dirección guarda dos valores distintos.
tags: [machine-model, virtual-memory, address-space, paging, mmu]
order: 12
updated: 2026-06-22
---
# El address space del proceso y la memoria virtual

Cada nota hasta acá imprimió direcciones como si tu programa fuera dueño de toda la memoria.
No lo es — y sin embargo la ilusión es hermética. Cada proceso recibe su **propio address
space virtual privado**: un rango plano desde 0 hacia arriba que *parece* RAM dedicada pero
es una ficción que el OS y la **MMU** (memory management unit) de la CPU mantienen sobre la
memoria física compartida. La dirección `0x1040` en tu proceso y `0x1040` en otro son bytes
*distintos* de la RAM física — o uno puede estar en RAM y el otro swapeado a disco. Esta
indirección compra tres cosas a la vez: **aislamiento** (un proceso no puede ver ni corromper
la memoria de otro), **la ilusión de espacio contiguo**, y **más address space del que tenés
RAM**. Esta nota es la primera mirada; el mecanismo completo es un tema de kernel que el
proyecto del OS va a construir.

> El reset: un puntero es una dirección *virtual*, no una ubicación física. Dos procesos
> pueden tener el mismo valor de puntero y referirse a memoria completamente distinta. El
> mapa plano del address space de las notas anteriores es por-proceso y enteramente virtual.

## La ilusión: virtual vs físico

Los programas solo usan **direcciones virtuales**. En cada acceso a memoria, la MMU traduce
la dirección virtual a una **dirección física** en RAM, usando tablas por-proceso que el
kernel mantiene. Como el mapeo es por-proceso:

- **El aislamiento es automático.** El proceso A simplemente no tiene mapeo para las páginas
  del proceso B, así que *no puede* nombrarlas. Un puntero salvaje crashea tu propio proceso,
  no el sistema.
- **El layout puede ser simple.** Cada proceso ve el mapa limpio text/data/heap/…/stack de la
  nota de [[lowlevel/machine-model/stack-vs-heap|stack-vs-heap]], arrancando cerca de 0, sin
  importar dónde cayeron físicamente sus páginas ni cuán fragmentada esté la RAM.
- **Lo virtual puede exceder lo físico.** Las páginas poco usadas viven en disco (swap) y se
  traen bajo demanda, así que el total de memoria virtual entre procesos puede exceder la RAM
  instalada.

## Páginas: la unidad de mapeo

La memoria virtual no se mapea byte por byte — esa tabla sería enorme. La memoria se divide en
**páginas** de tamaño fijo (4 KB en x86-64 y Apple Silicon; existen "huge pages" más grandes).
Las **page tables** del kernel mapean cada página virtual a un **frame** físico, y la MMU las
recorre en cada acceso, cacheando las traducciones recientes en el **TLB** (translation
lookaside buffer) para que el caso común sea rápido.

- Una dirección virtual se parte en un **número de página** (cuál página) y un **offset**
  (dónde dentro de la página). Solo se traduce el número de página; el offset pasa tal cual.
- Una referencia a una página que no está en RAM dispara un **page fault** — la CPU trapea al
  kernel, que trae la página (de disco, o la rellena con ceros si es fresca) y reanuda tu
  instrucción como si nada.
- Cada página lleva **permisos** (read/write/execute). Escribir una página read-only (como el
  código) o una página sin mapear es lo que produce un **segmentation fault**.

## Mirá el aislamiento: la misma dirección, dos valores

`fork()` duplica un proceso. El padre y el hijo después tienen la *misma* dirección virtual
para un global — pero escribirla en el hijo no afecta al padre, porque cada uno tiene su
propia página física (copiada al escribir). Compilá y corré:

```c
// vmem.c — una dirección virtual, dos valores independientes entre procesos.
// gcc -O0 -Wall -Wextra vmem.c -o vmem && ./vmem
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int shared_global = 100;       // misma dirección virtual en padre e hijo

int main(void) {
    printf("page size: %ld bytes\n", sysconf(_SC_PAGESIZE));
    printf("&shared_global = %p (value %d)\n\n",
           (void*)&shared_global, shared_global);
    fflush(stdout);            // flush antes del fork, o el hijo hereda nuestro buffer

    pid_t pid = fork();
    if (pid == 0) {            // hijo: misma dirección, su propia página física (COW)
        shared_global = 999;
        printf("[child  pid=%d] &g = %p  value = %d\n",
               getpid(), (void*)&shared_global, shared_global);
        return 0;
    }
    wait(NULL);                // padre: la escritura del hijo no lo tocó
    printf("[parent pid=%d] &g = %p  value = %d\n",
           getpid(), (void*)&shared_global, shared_global);
    return 0;
}
```

```
page size: 4096 bytes
&shared_global = 0x105960000 (value 100)

[child  pid=68695] &g = 0x105960000  value = 999
[parent pid=68694] &g = 0x105960000  value = 100
```

El mismo puntero `0x105960000` en los dos procesos, y sin embargo el hijo lee `999` y el
padre sigue leyendo `100`. Eso es memoria virtual: la dirección es un *índice en el mapa
privado de cada proceso*, no una ubicación física. El **copy-on-write** que abarata al `fork`
— las páginas se comparten read-only hasta que un lado escribe, y ahí se duplican de forma
transparente — es la misma maquinaria de paginación en acción.

## Dónde conecta (y qué queda diferido)

Esto es el cimiento debajo de buena parte del atlas, pero el mecanismo profundo es trabajo de
kernel:

- **`mmap`** le pide al kernel que agregue mapeos a tu address space directamente — memoria
  anónima (lo que `malloc` usa por debajo para pedidos grandes) o un archivo mapeado en
  memoria. Eso es un tema de [[lowlevel/systems-programming/index|systems programming]].
- **Las page tables, el TLB, el manejo de page faults y la política de paginación** se
  construyen desde cero en el [[lowlevel/os-from-scratch/index|proyecto del OS]] — cuando
  escribas el código de paginación, esta ilusión se vuelve algo que *vos* implementás.
- **Las caches están físicamente entre la MMU y la RAM**, así que la memoria virtual y la
  [[lowlevel/machine-model/the-memory-hierarchy|jerarquía de memoria]] interactúan: un TLB
  miss y un cache miss son frenos distintos en el mismo camino de acceso.

## Modos de falla y trade-offs

- **Segfault = acceso virtual inválido.** Desreferenciar NULL, un puntero colgado, o pasarte
  de un array toca una página sin mapear o con permiso equivocado; la MMU trapea y el kernel
  mata al proceso. Es el hardware protegiendo a otros procesos de tu bug.
- **La traducción tiene un costo.** Cada acceso se traduce; un TLB miss fuerza un recorrido de
  la page table. La mala localidad thrashea el TLB igual que thrashea las caches — otra razón
  por la que los patrones de acceso importan.
- **El swapping (thrashing) es un precipicio.** Cuando el working set excede la RAM, el kernel
  pagina a disco constantemente y el throughput colapsa — el mundo de los nanosegundos cayendo
  a milisegundos, como en la nota de la jerarquía.
- **Las direcciones no son estables ni comparables entre procesos.** Una dirección virtual
  tiene sentido solo dentro de su proceso; nunca compartas punteros crudos entre procesos (usá
  shared memory con su propio mapeo). El ASLR además las randomiza en cada corrida.

## En la práctica

- **Pensá "mis punteros son direcciones virtuales privadas".** Ese solo reencuadre explica el
  aislamiento, por qué los segfaults son locales, y por qué los punteros de dos procesos no se
  relacionan.
- **Razoná la memoria en páginas.** La asignación, la protección (`mprotect`), el mapeo de
  archivos y el comportamiento ante faults son todos a granularidad de página; la página de 4
  KB es el quantum real de memoria del OS.
- **Un segfault es un *diagnóstico*, no un misterio.** Significa "nombraste una dirección
  virtual que no tenías permitido tocar" — casi siempre un puntero malo; agarrá ASan/el
  debugger.
- **Esto cierra el modelo de máquina.** Ahora tenés CPU, registros, la jerarquía de memoria,
  representación de datos, cómo el fuente se vuelve ejecución, y el address space donde viven
  esos bytes — el sustrato sobre el que la capa de C construye a continuación.

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/the-memory-hierarchy|La jerarquía de memoria]] · [[lowlevel/machine-model/how-source-becomes-execution|Cómo el fuente se vuelve ejecución]] · [[lowlevel/systems-programming/index|Systems Programming]] · [[lowlevel/os-from-scratch/index|OS desde Cero]]

## Fuentes

- **Arpaci-Dusseau — *Operating Systems: Three Easy Pieces*, "Virtualizing Memory"** — el tratamiento gratuito más claro de address spaces, paginación, TLBs y page faults; la columna de esta nota. https://pages.cs.wisc.edu/~remzi/OSTEP/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 9 — la memoria virtual desde la vista del programador, con la mecánica de x86-64. https://csapp.cs.cmu.edu/
- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — cómo la memoria virtual, el TLB y las caches interactúan en hardware real. https://people.freebsd.org/~lstewart/articles/cpumemory.pdf
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 3A — la especificación autoritativa de paginación y MMU para x86-64. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **man pages — `mmap(2)`, `fork(2)`, `mprotect(2)`** — los syscalls que manipulan el address space de un proceso. https://man7.org/linux/man-pages/man2/mmap.2.html
