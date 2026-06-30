---
title: "Function pointers"
description: Los function pointers permiten que C guarde y llame direcciones de código con una firma chequeada. Sostienen callbacks, dispatch tables, comparators de qsort y polimorfismo runtime chico sin objetos.
tags: [function-pointers, callbacks, dispatch, c, abi]
order: 8
updated: 2026-06-30
---
# Function pointers

Un function pointer es un valor que nombra código llamable con una firma específica. Le
permite a C pasar comportamiento: comparators para `qsort`, callbacks con contexto,
dispatch tables, state machines, signal handlers e interfaces chicas tipo plugin. La firma
es el contrato. Llamar una función a través de un tipo de function pointer incompatible es
undefined behavior porque caller y callee pueden estar en desacuerdo sobre argumentos,
valor de retorno o calling convention.

> El reset: object pointers apuntan a datos; function pointers apuntan a código. En
> máquinas actuales suelen parecer direcciones, pero C les da reglas separadas.

## La forma

La sintaxis cruda de function pointer es ruidosa:

```c
int (*op)(int, int);
```

Leela desde el identificador hacia afuera: `op` es un puntero a una función que toma dos
argumentos `int` y devuelve `int`. Un typedef vuelve legibles las APIs:

```c
typedef int (*binary_op)(int left, int right);
```

Después podés guardar funciones, pasarlas y llamar a través de ellas:

```c
binary_op op = add;
int result = op(2, 3);
```

El function designator `add` se convierte automáticamente a un puntero en la mayoría de
los contextos de expresión, parecido a cómo los arrays decaen a punteros. `op(2, 3)` y
`(*op)(2, 3)` son válidos; la sintaxis corta es idiomática.

## Cómo funciona realmente

El valor de function pointer identifica un punto de entrada de función según el ABI de la
plataforma. El compilador usa el tipo de función apuntado para generar la secuencia de
call: registros o slots de stack para argumentos, registro de retorno y convenciones de
cleanup. En el target System V AMD64 ABI usado por este atlas, los argumentos enteros
suelen pasar por registros como `rdi`, `rsi`, `rdx`, y el retorno por `rax`. Un tipo de
function pointer incorrecto puede hacer que el caller ponga bits donde el callee no los
espera.

Los function pointers son una forma de dispatch dinámico de bajo nivel. Una dispatch table
es solo un array o struct de function pointers:

```c
struct Command {
    const char *name;
    binary_op run;
};
```

Esto no es herencia orientada a objetos; no hay receiver oculto salvo que lo pases. Por
eso muchas APIs de callbacks emparejan un function pointer con un puntero `void *ctx` para
que el callback tenga estado:

```c
typedef void (*visit_fn)(void *ctx, int value);
```

El function pointer provee comportamiento; el context pointer provee datos. Ese par es una
closure manual.

No trates function pointers como `void*`. El estándar C garantiza conversiones entre
`void*` y object pointers, no entre `void*` y function pointers. APIs POSIX como `dlsym`
viven en una frontera de plataforma con sus propias reglas; C portable común debería
mantener separados punteros a código y punteros a objeto.

## Artefacto ejecutable: una dispatch table mínima

El demo vive en `examples/pointers-and-memory/function-pointers/demo.c`.

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Salida real:

```
add(6, 7)              = 13
mul(6, 7)              = 42
apply add             = 5
apply multiply        = 6
same function pointer = yes
```

La tabla guarda dos nombres y dos function pointers con la misma firma. `apply` no sabe
qué operación va a llamar hasta runtime, pero la firma todavía le dice al compilador cómo
hacer la llamada.

## Modos de falla y trade-offs

- **Firma equivocada.** Castear una función al tipo de puntero equivocado y llamarla es
  undefined behavior. La secuencia de call a nivel máquina puede no coincidir.
- **Null function pointer.** Un function pointer puede ser null. Llamarlo es undefined
  behavior, igual que dereferenciar un object pointer null.
- **Confusión con object pointers.** Un function pointer no puede guardarse portablemente
  en `void*`. Mantené callback functions y callback contexts como dos valores separados.
- **Lifetime del código apuntado.** Las funciones normales del programa viven todo el
  programa. Código cargado dinámicamente puede desaparecer si se descarga su biblioteca
  mientras quedan punteros.
- **Trade-off de inlining y optimización.** Las llamadas indirectas son más difíciles de
  inlinear y pueden ser más difíciles para branch prediction que llamadas directas. Usalas
  cuando importa la configurabilidad.
- **Ownership ambiguo del callback.** Un function pointer más `void *ctx` debe definir
  quién posee el contexto y cuánto tiempo sigue vivo.

## En la práctica

- **Usá typedefs para firmas de callbacks.** Vuelven legibles dispatch tables y contratos
  de API.
- **Emparejá callbacks con contexto.** Un callback sin estado es raro; pasá `void *ctx`
  explícitamente cuando la API necesita una closure manual.
- **Mantené firmas exactas.** Evitá casts alrededor de function pointers; arreglá el tipo
  declarado.
- **Preferí tablas a escaleras de `switch` para conjuntos estables de comandos.** Las
  function pointer tables vuelven el dispatch data-driven y fácil de testear.
- **Usá llamadas directas en código hot salvo que necesites dispatch.** El dispatch
  dinámico tiene costo; ganalo con flexibilidad.

**Conecta con:** [[lowlevel/pointers-and-memory/void-star-and-type-erasure|void* y type erasure]] · [[lowlevel/pointers-and-memory/pointers-to-pointers-double-indirection|Punteros a punteros]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — function designators, tipos de function pointer, llamadas a través de function pointers y undefined behavior para llamadas incompatibles. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Pointer declaration** — sintaxis y ejemplos de function pointer. https://en.cppreference.com/w/c/language/pointer
- **cppreference — `qsort`** — API estándar de comparator callback que usa function pointers y `void*`. https://en.cppreference.com/w/c/algorithm/qsort
- **System V AMD64 ABI** — calling convention a nivel máquina que explica por qué las firmas de función deben coincidir. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Jens Gustedt — *Modern C*** — callbacks, tipos de function pointer y diseño de APIs C. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Beej's Guide to C — Function pointers** — sintaxis accesible y ejemplos de callbacks. https://beej.us/guide/bgc/html/split/pointers.html
