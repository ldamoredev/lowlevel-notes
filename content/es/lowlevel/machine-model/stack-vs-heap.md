---
title: "Stack vs heap"
description: Las dos regiones de memoria donde vive todo programa en C — el stack (automático, LIFO, rápido, atado a las llamadas a función) y el heap (manual, cualquier lifetime, flexible, más lento). Qué es cada uno, cómo está dispuesto el address space y un programa en C que imprime ambos para que los veas.
tags: [machine-model, stack, heap, memory, address-space]
order: 6
updated: 2026-06-22
---
# Stack vs heap

Un lenguaje administrado te daba una "memoria" indiferenciada y un garbage collector que
decidía cuándo morían las cosas. Por debajo, todo programa parte su memoria de trabajo en
dos regiones con reglas opuestas. El **stack** (pila de llamadas) es almacenamiento
automático last-in-first-out atado a las llamadas a función: cada llamada recibe un frame,
que desaparece en el instante en que la llamada retorna — nunca lo liberás. El **heap**
(memoria dinámica) es un pozo grande que manejás a mano con `malloc` y `free`: un bloque
vive exactamente lo que vos lo mantengas vivo, a través de cualquier cantidad de llamadas.
El stack es rápido y finito; el heap es flexible y más lento. Saber cuál es cuál — y quién
libera qué — es la diferencia entre C correcto y crashes.

> El reset: el GC no está. La memoria del stack se libera sola cuando una función retorna,
> te guste o no (así que un puntero adentro de ella muere ahí). La memoria del heap no se
> libera *nunca* salvo que llames a `free` (así que olvidarte la fuga). El lifetime ahora
> es tu trabajo.

## Un address space, varias regiones

Un proceso en ejecución ve un único address space virtual plano, tallado en regiones. De
direcciones bajas a altas:

| Región | Contiene | Crece | Lifetime |
|---|---|---|---|
| Text (código) | instrucciones de máquina | fijo | todo el programa |
| Data (`.data`) | globals/statics inicializados | fijo | todo el programa |
| BSS (`.bss`) | globals/statics en cero | fijo | todo el programa |
| **Heap** | bloques de `malloc` | **hacia arriba** ↑ | hasta que hagas `free` |
| *(gap)* | espacio virtual sin usar | — | — |
| **Stack** | frames de llamada, locales | **hacia abajo** ↓ | por llamada a función |

El heap y el stack crecen *uno hacia el otro* desde extremos opuestos del gap — un layout
elegido para que cada uno pueda expandirse sin una frontera fija entre ellos. La nota de la
[[lowlevel/machine-model/the-memory-hierarchy|jerarquía]] era sobre la *velocidad* de la
memoria; esta es sobre la *organización* del address space de un proceso. (La memoria
virtual — por qué cada proceso ve su propia versión limpia de este mapa — tiene su propia
nota más adelante.)

## El stack: almacenamiento automático, LIFO

El stack lo maneja un registro, `rsp` (el stack pointer), y una disciplina simple. Llamar
a una función **pushea** un frame (resta a `rsp`); retornar lo **popea** (suma de vuelta).
Un frame contiene los locales de la función, los registros guardados y la dirección de
retorno. Como es LIFO puro, asignar es casi gratis — solo mover `rsp` — y desasignar es
automático: cuando la función retorna, su frame entero desaparece.

- **Rápido.** Asignar un local es un ajuste de registro, y el tope del stack siempre está
  caliente en cache, así que los locales son casi lo más barato que da la memoria.
- **Lifetime automático.** Nunca liberás un local; muere cuando su función retorna. Esa es
  la trampa también: un puntero a un local queda **colgado** (dangling) en el momento en
  que retornás.
- **Chico y fijo.** El stack tiene un límite duro (comúnmente ~8 MB en Linux, ~1–8 MB en
  threads de macOS). Reventalo — recursión profunda/infinita, o un array local enorme — y
  tenés un **stack overflow**: crash instantáneo, sin diagnóstico.

La mecánica de los frames (prologue/epilogue, `push`/`pop`, la instrucción de call)
pertenece a [[lowlevel/assembly-and-compiler-output/index|assembly y salida del
compilador]]; acá el punto es la *política*: LIFO, automático, acotado.

## El heap: almacenamiento manual, cualquier lifetime

Cuando necesitás memoria cuyo lifetime no está atado a una sola llamada — tiene que
sobrevivir a la función que la creó, o su tamaño no se conoce hasta runtime — le pedís al
**heap**. `malloc(n)` encuentra un bloque libre de `n` bytes y devuelve un puntero;
`free(p)` lo devuelve al allocator. Entre esas dos llamadas el bloque es tuyo, a través de
cualquier cantidad de retornos de función.

- **Lifetime flexible.** Un bloque vive hasta que *vos* lo liberás — perfecto para
  estructuras de datos que sobreviven a su creador (una lista que armás y devolvés).
- **Manual y más lento.** `malloc`/`free` corren bookkeeping real (free lists, size
  classes, coalescing), así que cada llamada cuesta mucho más que un bump del stack, y la
  memoria fresca del heap está fría en cache.
- **Sos dueño de cada consecuencia.** Olvidarte de hacer `free` → un memory leak (fuga de
  memoria). Hacer `free` dos veces o usar después de `free` → corrupción. El allocator y
  estos modos de falla son todo el tema de
  [[lowlevel/pointers-and-memory/index|punteros y memoria]].

## Mirá ambas regiones

Este programa imprime una dirección de cada región. Compilá en `-O0` para que los locales
no se optimicen a registros, y corré:

```c
// regions.c — imprime una dirección de cada parte del address space.
// gcc -O0 -Wall -Wextra regions.c -o regions && ./regions
#include <stdio.h>
#include <stdlib.h>

int global_initialized = 42;   // .data
int global_zero;               // .bss

void show_stack_growth(int depth) {
    int local;                 // un slot de stack fresco en cada llamada
    printf("  depth %d: &local = %p\n", depth, (void*)&local);
    if (depth < 3) show_stack_growth(depth + 1);
}

int main(void) {
    int stack_var = 7;
    int *heap_var = malloc(sizeof(int));

    printf("code  (&main)        = %p\n", (void*)main);
    printf("data  (&global_init) = %p\n", (void*)&global_initialized);
    printf("bss   (&global_zero) = %p\n", (void*)&global_zero);
    printf("heap  (malloc)       = %p\n", (void*)heap_var);
    printf("stack (&stack_var)   = %p\n", (void*)&stack_var);
    printf("\nstack grows DOWN as we recurse:\n");
    show_stack_growth(0);

    free(heap_var);
    return 0;
}
```

Una corrida real (macOS, x86-64) — tus direcciones exactas cambian en cada corrida por
ASLR, pero el *orden* y la dirección se mantienen:

```
code  (&main)        = 0x10cf69e10
data  (&global_init) = 0x10cf6b000
bss   (&global_zero) = 0x10cf6b004
heap  (malloc)       = 0x7fb520804080
stack (&stack_var)   = 0x7ff7b2f95988

stack grows DOWN as we recurse:
  depth 0: &local = 0x7ff7b2f95968
  depth 1: &local = 0x7ff7b2f95948   <- cada frame está en una dirección MÁS BAJA
  depth 2: &local = 0x7ff7b2f95928
  depth 3: &local = 0x7ff7b2f95908
```

El código y los globals están abajo y juntos; el heap es una región grande por encima; el
stack está bien arriba — y cada recursión pone el nuevo `&local` en una dirección *más
baja*, probando que el stack crece hacia abajo. Acabás de ver el mapa del address space que
describía la tabla.

## Cuál usar: una regla de decisión

| Usá el **stack** cuando… | Usá el **heap** cuando… |
|---|---|
| el tamaño se conoce en compile time | el tamaño se decide en runtime |
| el dato muere con la función | el dato tiene que sobrevivir a la llamada |
| es chico (bytes–KB) | es grande (KB–GB) |
| querés costo de alloc cero | aceptás costo a cambio de flexibilidad |

Por defecto, el stack — es más rápido y se libera solo. Andá al heap solo cuando el
lifetime o el tamaño te obliguen, y ahí hacé explícito el ownership (quién llama a `free`).

## Modos de falla y trade-offs

- **Puntero colgado (devolver un local).** `int *f(void){ int x=0; return &x; }` devuelve
  un puntero a memoria que ya está liberada cuando `f` retorna — usarlo es undefined
  behavior (comportamiento indefinido). La memoria del stack se auto-libera; los punteros
  adentro de ella no pueden escapar.
- **Stack overflow.** La recursión sin cota o un array local de varios MB
  (`int buf[4000000]`) excede el límite del stack y crashea feo. Los buffers grandes o de
  tamaño en runtime van al heap.
- **Memory leak.** Los bloques de heap que nunca liberás se acumulan hasta que el proceso
  muere; en un server de larga vida eso es una muerte lenta. Cada `malloc` necesita un
  dueño responsable del `free`.
- **Use-after-free / double-free.** La contracara del manejo manual del heap — corrupción
  y bugs explotables. Lo vemos en profundidad en punteros y memoria.

## En la práctica

- **Los locales son stack, `malloc` es heap — esa es toda la pista.** No hay keyword que
  diga "stack"; las variables automáticas simplemente *son* stack. Solo
  `malloc`/`calloc`/`realloc` (y compañía) tocan el heap.
- **Un puntero no te dice a qué región apunta.** Un `int *p` puede apuntar a un local, a un
  global o a un bloque del heap. *Vos* tenés que trackearlo — sobre todo "¿puedo hacer
  `free` de esto?" (solo bloques del heap, exactamente una vez).
- **Grande o de larga vida → heap; chico o acotado → stack.** Este solo instinto previene
  tanto los stack overflows como la mayoría de los bugs de lifetime.
- **Esto es la base de todo lo que viene.** Todo el modelo de memoria de C, los punteros y
  los allocators se construyen directo sobre estas dos regiones.

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/the-memory-hierarchy|La jerarquía de memoria]] · [[lowlevel/machine-model/registers-and-the-isa|Registros y la ISA]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]

## Fuentes

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3 y 9 — el stack a nivel máquina, el address space del proceso y la memoria virtual; la columna de esta nota. https://csapp.cs.cmu.edu/
- **Jens Gustedt — *Modern C*** — storage duration automático vs allocated, lifetimes y `malloc`/`free` bien hechos. https://gustedt.gitlabpages.inria.fr/modern-c/
- **cppreference — `malloc` / `free` / storage duration** — la semántica precisa de C para storage automático vs allocated. https://en.cppreference.com/w/c/memory/malloc
- **Arpaci-Dusseau — *Operating Systems: Three Easy Pieces*, "Address Spaces"** — por qué un proceso ve un layout virtual limpio con stack y heap en extremos opuestos. https://pages.cs.wisc.edu/~remzi/OSTEP/
- **System V AMD64 ABI** — cómo se organiza el stack en los límites de llamada a función (frames, alineación, la disciplina de `rsp`). https://gitlab.com/x86-psABIs/x86-64-ABI
