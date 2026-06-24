---
title: "Undefined behavior: el contrato que no sabías que firmaste"
description: Undefined behavior no es un resultado raro en runtime; es un contrato roto con la máquina abstracta de C. Una vez que tu programa invoca UB, el compilador puede asumir que ese camino nunca pasa y optimizar alrededor de eso.
tags: [c, undefined-behavior, compiler, optimization, safety]
order: 3
updated: 2026-06-22
---
# Undefined behavior: el contrato que no sabías que firmaste

Undefined behavior (UB, comportamiento indefinido) es lo que pasa cuando un programa C se
sale de las reglas de la máquina abstracta de C. No es "la CPU va a hacer algo raro" y no
es "el resultado es impredecible pero local." Significa que el estándar no le impone
ningún requisito a la implementación, así que el compilador puede asumir que ese camino
nunca pasa y optimizar usando esa suposición. UB es el contrato oculto detrás de la
velocidad de C: tenés control de bajo nivel solo mientras cumplas las promesas en las que
se apoya el optimizador.

> El reset: UB no es un valor malo. UB es el programa saliéndose del lenguaje. Después de
> eso, razonar desde el fuente, el assembly o una corrida local puede engañarte.

## UB es un contrato con la máquina abstracta

C se especifica en términos de una máquina abstracta: objetos, lifetimes, effective types,
sequencing, reglas de enteros, reglas de punteros y precondiciones de biblioteca. Tu fuente
no es una lista de instrucciones de CPU. Es una afirmación de que, para cada ejecución que
el compilador tiene que considerar, esas reglas de la máquina abstracta se respetan.

Esa afirmación compra performance. El compilador puede asumir:

| Regla que prometés | Optimización que puede hacer el compilador |
|---|---|
| el overflow de `int` signed nunca pasa | simplificar comparaciones y salidas de loops |
| el acceso a arrays queda dentro del objeto | eliminar casos de bounds imposibles |
| los punteros non-null no se dereferencian siendo null | borrar null checks redundantes |
| los lifetimes de objetos se respetan | reutilizar slots de stack y registros agresivamente |
| las reglas de effective type / aliasing se respetan | mantener valores cacheados en vez de releer memoria |

Por eso UB es distinto de **implementation-defined behavior** (comportamiento definido por
la implementación) y **unspecified behavior** (comportamiento no especificado).
Implementation-defined behavior tiene una elección documentada, como la signedness de
`char` plano en un target. Unspecified behavior elige entre posibilidades válidas, como el
orden de evaluación de argumentos en muchas llamadas. UB no tiene requisitos: el compilador
puede comportarse como si tu programa no hiciera eso.

## Cómo funciona realmente

Cuando el optimizador ve código, arma un modelo de lo que tiene que ser cierto si el
programa no tiene UB. Esas verdades se vuelven hechos. Si `p` se dereferencia, entonces en
ese camino se asume que `p` no es null. Si `i + 1 > i` para `int` signed, entonces se asume
que no hay overflow signed. Si un índice de loop se usa para acceder `a[i]`, los accesos
fuera del array se consideran imposibles en ejecuciones válidas.

A nivel máquina, la CPU quizá tenga una instrucción concreta para la operación mala. El
overflow signed en x86-64 produce bits con wraparound. Leer uno más allá de un array quizá
cargue el byte que sigue. Hacer type punning a través del puntero equivocado quizá parezca
funcionar. Pero el compilador no prometió preservar ese accidente. Puede reordenar,
borrar, plegar, vectorizar o cachear basado en el contrato más fuerte del fuente.

Familias comunes de UB:

- **UB de lifetime.** Use-after-free, devolver un puntero a un local, leer un objeto antes
  de que empiece su lifetime, o usar storage después de que termina su lifetime.
- **UB de bounds.** Acceder fuera de un objeto array, incluso por un elemento cuando lo
  dereferenciás. Formar one-past está permitido; leer one-past no.
- **UB de enteros.** Overflow de enteros signed, overflow de división (`INT_MIN / -1`) y
  división por cero.
- **UB de punteros.** Dereferenciar null, acceso desalineado, aritmética de punteros
  inválida, o comparar punteros no relacionados de formas que el estándar no define.
- **UB de tipos.** Violar effective type / strict aliasing accediendo a un objeto a través
  de un tipo de puntero incompatible.
- **UB de sequencing.** Modificar un escalar más de una vez sin sequencing, como trampas
  viejas de entrevistas tipo `i = i++ + ++i`.

El [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|sistema de tipos débil]] atrapa
algunos errores de forma antes de llegar acá. UB es lo que pasa cuando el programa todavía
type-checkea pero el contrato de abajo es falso.

## Artefacto ejecutable: evitá el camino malo

Este demo deliberadamente **no ejecuta UB**. Muestra tres bordes clásicos y la versión
definida: suma signed chequeada, bounds checks antes de indexar, y `memcpy` para type
punning de bytes. Compilalo normal primero; los sanitizers vienen después.

```c
// demo.c — UB como contrato: no ejecutes "el caso malo"; chequealo antes.
// gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void checked_add_int(int a, int b) {
    int out = 0;

    if (__builtin_add_overflow(a, b, &out)) {
        printf("checked add: %d + %d would overflow a signed int (UB avoided)\n", a, b);
        return;
    }

    printf("checked add: %d + %d = %d\n", a, b, out);
}

static void read_index(const int *items, size_t count, size_t index) {
    if (index >= count) {
        printf("bounds check: index %zu is outside 0..%zu (UB avoided)\n",
               index, count - 1);
        return;
    }

    printf("bounds check: items[%zu] = %d\n", index, items[index]);
}

static void show_float_bits(float value) {
    uint32_t bits = 0;

    memcpy(&bits, &value, sizeof bits);
    printf("memcpy punning: %.1f has bits 0x%08X\n", value, (unsigned)bits);
}

int main(void) {
    int items[] = {10, 20, 30};

    checked_add_int(40, 2);
    checked_add_int(INT_MAX, 1);

    read_index(items, sizeof items / sizeof items[0], 1);
    read_index(items, sizeof items / sizeof items[0], 3);

    show_float_bits(1.0f);
    return 0;
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
```

Salida real:

```
checked add: 40 + 2 = 42
checked add: 2147483647 + 1 would overflow a signed int (UB avoided)
bounds check: items[1] = 20
bounds check: index 3 is outside 0..2 (UB avoided)
memcpy punning: 1.0 has bits 0x3F800000
```

Lo importante es lo que el programa se niega a hacer. No calcula `INT_MAX + 1` como
expresión signed esperando wraparound. No lee `items[3]`. No reinterpreta un `float *` como
`uint32_t *`; copia la representación del objeto mediante `memcpy`, que C define. En código
real, combiná este estilo con UBSan/ASan en tests:

```bash
gcc -O1 -g -fsanitize=undefined,address demo.c -o demo-asan-ubsan
```

Los sanitizers no cambian el contrato. Hacen ruidosas muchas violaciones del contrato
antes de que el optimizador las convierta en algo más difícil de debuggear.

## Modos de falla y trade-offs

- **"En mi máquina funcionaba."** Una corrida local solo prueba un compilador, un nivel de
  optimización, un input, un layout y un momento. UB puede desaparecer en `-O0` y romperse
  en `-O2`, o recién con link-time optimization.
- **Razonamiento accidental desde el hardware.** x86-64 wrappea overflow signed en
  hardware, pero el overflow signed en C sigue siendo UB. La regla del fuente gana sobre el
  accidente a nivel instrucción.
- **Los warnings son incompletos.** `-Wall -Wextra` atrapa muchos patrones sospechosos, no
  todo UB. Parte del UB depende de valores de runtime, aliasing entre funciones o historia
  de ownership.
- **Los sanitizers son tests, no pruebas.** UBSan y ASan son excelentes, pero cubren caminos
  ejecutados. Los caminos no testeados todavía pueden contener UB.
- **Evitar UB puede costar ceremonia.** Bounds checks, aritmética chequeada, `memcpy` para
  representación y disciplina de ownership agregan código. Esa ceremonia es más barata que
  debuggear un fantasma habilitado por el optimizador.

## En la práctica

- **Tratá UB como bug de diseño, no como evento de runtime.** El fix correcto es hacer que
  el estado ilegal sea irrepresentable o esté chequeado antes de que pase.
- **Usá unsigned solo cuando querés aritmética modular.** No lo uses meramente para silenciar
  overflow signed; también cambia comparaciones y conversiones.
- **Mantené simple la provenance de punteros.** Asigná, pasá y liberá a través de caminos
  de ownership obvios. Evitá aritmética ingeniosa fuera del objeto que realmente poseés.
- **Usá `memcpy` para copias de representación de objeto.** Los compiladores reconocen y
  optimizan `memcpy` chicos; obtenés comportamiento definido sin perder calidad de código
  generado.
- **Prendé las herramientas temprano.** Warnings, UBSan, ASan, fuzzing y code review son el
  contrapeso práctico al contrato fino de C.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/why-c-still-matters|Por qué C todavía importa]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|El sistema de tipos de C es débil]] · [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Complemento a dos y representación de enteros]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre undefined behavior, la máquina abstracta, lifetimes de objetos, effective type, sequencing y reglas de enteros. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Undefined behavior** — catálogo compacto de undefined, unspecified e implementation-defined behavior con ejemplos de optimización. https://en.cppreference.com/w/c/language/behavior
- **cppreference — Object model and effective type** — las reglas detrás de representación de objetos, aliasing, alineación y por qué la inspección con character bytes y `memcpy` son especiales. https://en.cppreference.com/w/c/language/object
- **Jens Gustedt — *Modern C*** — marco práctico de la máquina abstracta de C, undefined behavior, reglas de enteros, lifetimes de objetos e idioms portables. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Clang UndefinedBehaviorSanitizer docs** — cómo UBSan instrumenta muchas clases de UB en builds de test y qué puede atrapar. https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
- **GCC integer overflow built-ins** — primitivas de aritmética chequeada como `__builtin_add_overflow`, útiles cuando el overflow se tiene que manejar explícitamente. https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html
