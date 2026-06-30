---
title: "El stack: frames, locales y automatic storage"
description: Un stack frame es una activación viva de función. Los locales automáticos viven dentro de esa activación, las llamadas recursivas reciben objetos frescos y los punteros hacia un frame solo son borrows mientras el frame está vivo.
tags: [stack, stack-frame, automatic-storage, lifetime, pointers]
order: 3
updated: 2026-06-30
---
# El stack: frames, locales y automatic storage

El stack no es solo "donde van los locales." Es el registro en runtime de las llamadas a
función activas. Cada llamada recibe un stack frame: una porción de storage para la
maquinaria de retorno, spills y objetos automáticos que existen solo mientras esa
activación está viva. La recursión crea objetos frescos porque crea frames frescos. Un
puntero al frame de un caller puede ser un borrow válido durante la cadena de llamadas; el
mismo puntero se vuelve veneno en el instante en que el frame retorna.

> El reset: automatic storage es una regla de lifetime, no una API de allocation. No hacés
> `free` de un local; el retorno de la función libera el frame entero. Eso también
> significa que no podés retornar, guardar ni usar asincrónicamente un puntero a ese frame.

## Los frames son activaciones de función

Un frame es una invocación viva de una función. El layout exacto en bytes depende del ABI
y del compilador, pero la política es estable:

| Cosa | Qué significa |
|---|---|
| stack pointer (`rsp` en x86-64) | registro que marca el tope actual del stack |
| dirección de retorno | dónde sigue la ejecución cuando la llamada retorna |
| registros guardados / spills | valores que el compilador preserva entre llamadas o mueve fuera de registros |
| locales automáticos | objetos de block scope sin storage duration `static` |
| frame caller | el frame que sigue vivo mientras corre el callee |

En el target System V AMD64 ABI usado en todo el atlas, el stack crece hacia abajo:
reservar storage de frame normalmente resta a `rsp`, y retornar devuelve ese storage
restaurando `rsp`. Muchas funciones optimizadas omiten un frame pointer tradicional
(`rbp`) y mantienen locales en registros cuando nunca se necesita su dirección. El modelo
sigue siendo "una activación, un lifetime"; el layout físico es detalle de
implementación.

## Cómo funciona realmente

En C, la mayoría de los objetos locales de block scope tienen **automatic storage
duration**. Su lifetime empieza cuando la ejecución entra al bloque y termina cuando sale.
Eso incluye locales comunes:

```c
void f(void) {
    int x = 42;   // automatic storage duration
}
```

También incluye llamadas recursivas. Cada invocación de `f` tiene su propio `x`; la
recursión no reutiliza el mismo objeto local. Si un callee recibe `&x`, ese puntero es
válido solo mientras la invocación de `f` que posee `x` siga activa.

Tomar la dirección de un local suele forzar al compilador a ubicarlo en memoria, pero un
local no tiene garantizada una dirección visible estable en todo build. A niveles más
altos de optimización, el compilador puede mantenerlo entero en un registro, partirlo,
reutilizar su slot para otro objeto con lifetime no superpuesto o eliminarlo. Los demos
con `-O0` muestran la forma del frame porque facilitan observar, no porque los frames de
producción sean tan literales.

Automatic no significa "siempre stack" en el estándar C. El estándar especifica lifetime,
no ubicación de hardware. En sistemas hosted x86-64 reales, los objetos automáticos cuya
dirección importa suelen vivir en el stack frame. Los locales `static` son distintos:
tienen static storage duration, se inicializan una vez, persisten durante todo el programa
y no se liberan cuando la función retorna.

Los Variable Length Arrays (VLAs), donde están soportados, también tienen automatic
storage duration. Hacen que el tamaño del frame dependa de input de runtime. Eso puede ser
útil en casos chicos y controlados, pero también es cómo un local que parece inocente se
vuelve un stack overflow.

## Artefacto ejecutable: mirá aparecer frames

El demo vive en
`examples/pointers-and-memory/the-stack-frames-locals-and-automatic-storage/demo.c`.

```c
#include <stdio.h>

static void bump_static_local(void) {
    static int visits = 0;
    visits += 1;
    printf("static local: value=%d address=%p\n", visits, (void *)&visits);
}

static void show_frame(int depth, int *borrowed_from_caller) {
    int local = depth * 10 + 1;
    int scratch[2] = {local, local + 1};

    printf("depth %d: &local=%p  borrowed=%p  borrowed_value=%d\n",
           depth, (void *)&local, (void *)borrowed_from_caller,
           *borrowed_from_caller);

    if (depth < 3) {
        show_frame(depth + 1, &local);
    }

    printf("depth %d returning with local=%d scratch_sum=%d\n",
           depth, local, scratch[0] + scratch[1]);
}

int main(void) {
    int main_local = 100;

    printf("main frame: &main_local=%p\n", (void *)&main_local);
    show_frame(0, &main_local);

    bump_static_local();
    bump_static_local();

    printf("main local after calls=%d\n", main_local);
    return 0;
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Una salida real:

```
main frame: &main_local=0x7ff7b1dff298
depth 0: &local=0x7ff7b1dff25c  borrowed=0x7ff7b1dff298  borrowed_value=100
depth 1: &local=0x7ff7b1dff21c  borrowed=0x7ff7b1dff25c  borrowed_value=1
depth 2: &local=0x7ff7b1dff1dc  borrowed=0x7ff7b1dff21c  borrowed_value=11
depth 3: &local=0x7ff7b1dff19c  borrowed=0x7ff7b1dff1dc  borrowed_value=21
depth 3 returning with local=31 scratch_sum=63
depth 2 returning with local=21 scratch_sum=43
depth 1 returning with local=11 scratch_sum=23
depth 0 returning with local=1 scratch_sum=3
static local: value=1 address=0x10e101000
static local: value=2 address=0x10e101000
main local after calls=100
```

Los frames recursivos caen en direcciones más bajas en esta corrida, coincidiendo con el
stack x86-64 que crece hacia abajo. Cada depth tiene un `local` distinto, y pasar
`&local` a la próxima llamada es seguro porque el caller todavía no retornó. El local
`static` mantiene la misma dirección y el mismo valor entre llamadas porque no es
automatic storage.

## Modos de falla y trade-offs

- **Retornar la dirección de un local.** `return &local;` le entrega al caller un puntero a
  un frame muerto. Usar ese puntero es undefined behavior, aunque los bytes viejos todavía
  no hayan sido pisados.
- **Guardar un puntero prestado al stack.** Guardar `&local` en un global, un objeto del
  heap, un argumento de thread o un callback async sobrevive al frame salvo que la API
  pruebe lo contrario.
- **Objetos automáticos no inicializados.** Los locales automáticos comunes no se ponen en
  cero por default. Leer un `int local;` no inicializado es undefined behavior.
- **Locales enormes y VLAs.** El stack es rápido pero limitado. Arrays grandes, VLAs del
  tamaño del input y recursión profunda pueden reventar el stack mucho antes de agotar el
  heap.
- **Asumir layout de frame.** La dirección y separación que imprimís en `-O0` son
  artefactos pedagógicos. Optimización, decisiones de ABI, sanitizers y omisión de frame
  pointer cambian el layout.
- **Lifetime vs performance.** La allocation en stack es barata porque desasignar es un
  pop de frame, pero esa velocidad se compra con un lifetime inflexible: estrictamente
  acotado y estrictamente last-in-first-out.

## En la práctica

- **Tratá punteros a locales como borrows.** Pueden bajarse por el call stack; no
  deben conservarse después de que retorna el frame dueño.
- **Devolvé datos por valor, buffer provisto por el caller o heap allocation.** Elegí
  según tamaño y ownership. No retornes punteros a arrays locales.
- **Mantené chicos y acotados los objetos del stack.** Buffers runtime-sized o grandes van
  en una estrategia deliberada de allocation, no casualmente en un frame.
- **Inicializá locales automáticos antes de leerlos.** La memoria muscular de lenguajes
  administrados miente acá; C no pone en cero los locales comunes del stack.
- **Usá herramientas cuando los lifetimes se ponen sutiles.** Los warnings del compilador
  agarran muchos casos de `return local address`, y AddressSanitizer puede detectar
  patrones de stack-use-after-scope/use-after-return en tests.
- **Usá frame pointers deliberadamente para debugging/profiling.** `-fno-omit-frame-pointer`
  puede hacer más fácil samplear call stacks, pero es una decisión de build, no una
  semántica de C.

**Conecta con:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Aritmética de punteros y stride]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/process-address-space-and-virtual-memory|Address space de proceso y memoria virtual]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — storage duration, lifetime de objetos, block scope y undefined behavior cuando termina el lifetime de un objeto. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Storage-class specifiers and storage duration** — referencia concisa para automatic, static, thread y allocated storage duration en C. https://en.cppreference.com/w/c/language/storage_duration
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, caps. 3 y 9 — stack frames, calls, código máquina y organización del address space de proceso. https://csapp.cs.cmu.edu/
- **System V AMD64 ABI** — alineación del stack x86-64, calling sequence, reglas de register save y convenciones de frame para targets Unix-like. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Jens Gustedt — *Modern C*** — tratamiento práctico de objetos automáticos, inicialización, lifetimes y disciplina de punteros en C moderno. https://gustedt.gitlabpages.inria.fr/modern-c/
- **AddressSanitizer documentation** — checks en runtime para stack-use-after-scope y bugs de lifetime relacionados durante tests. https://clang.llvm.org/docs/AddressSanitizer.html
