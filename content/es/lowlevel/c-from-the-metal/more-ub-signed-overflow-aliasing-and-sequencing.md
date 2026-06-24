---
title: "Más UB: signed overflow, aliasing y sequencing"
description: Tres trampas de undefined behavior en C que parecen inofensivas porque el hardware tiene un resultado obvio: signed overflow, violaciones de strict aliasing y side effects no secuenciados. Qué dice el estándar, qué asumen los optimizadores y cómo escribir la versión definida.
tags: [c, undefined-behavior, overflow, aliasing, sequencing]
order: 4
updated: 2026-06-22
---
# Más UB: signed overflow, aliasing y sequencing

La nota anterior dio la regla grande: undefined behavior (UB, comportamiento indefinido)
es el programa saliéndose de la máquina abstracta de C. Esta nota baja a tres casos que
engañan incluso a ingenieros con experiencia porque el hardware parece tener una respuesta
obvia: el signed overflow da la vuelta en registros, el type punning "solo lee los mismos
bytes", y `i++` tiene un valor que podés imaginar. En C, esas intuiciones no son el
contrato. El compilador puede optimizar como si el signed overflow nunca pasara, los
punteros incompatibles no hicieran alias, y los side effects ocurrieran solo de formas
secuenciadas.

> El reset: si el estándar C dice que una operación es UB, el comportamiento visible de la
> CPU no es tu regla de portabilidad. El optimizador razona primero desde la promesa del
> lenguaje fuente.

## Tres promesas que explotan los optimizadores

C le da al optimizador más libertad que "emitir la instrucción obvia por cada línea de
fuente." Tres de las promesas más importantes son:

| Promesa | Forma mala tentadora | Forma definida |
|---|---|---|
| el signed overflow no pasa | `int y = INT_MAX + 1;` | chequear, ensanchar o usar aritmética modular unsigned |
| tipos de punteros incompatibles no hacen alias | `uint32_t bits = *(uint32_t *)&f;` | copiar representación con `memcpy` |
| los side effects están secuenciados | `int y = i++ + ++i;` | separar actualizaciones en sentencias distintas |

No son preferencias de estilo. Son suposiciones que el compilador puede usar cuando pliega
expresiones, vectoriza loops, cachea valores en registros y borra ramas que prueba
inalcanzables en cualquier ejecución C válida.

## Signed overflow no es wraparound

La aritmética unsigned en C es modular: `UINT_MAX + 1u == 0u`. El signed overflow es
distinto. Si una expresión `int` produce un valor fuera del rango representable, el
comportamiento es undefined. En x86-64, la instrucción `add` o `imul` puede dejar bits con
wraparound en un registro, pero C no prometió que tu expresión signed mapeara a "ejecutá
la instrucción y quedate con los bits bajos."

Esa libertad deja que el optimizador asuma hechos como `x + 1 > x` para `int` signed. Si
el signed overflow pudiera wrappear, eso sería falso en `INT_MAX`. Como el overflow es UB,
el compilador puede tratar el caso con overflow como imposible y simplificar código
alrededor. Por eso "funcionaba en `-O0`" es evidencia débil: poca optimización suele
preservar más forma del fuente, mientras que `-O2` usa el contrato agresivamente.

Regla de decisión:

- Usá tipos **signed** para cantidades conceptualmente negativas u ordenadas, pero protegé
  la aritmética que pueda hacer overflow.
- Usá tipos **unsigned** solo cuando la aritmética modular sea el modelo real del dominio,
  no meramente como forma de esquivar UB.
- Usá helpers chequeados como `__builtin_add_overflow` / `__builtin_mul_overflow`,
  intermediarios más anchos o range checks explícitos cuando el overflow sea posible.

Esto conecta directo con
[[lowlevel/machine-model/twos-complement-and-integer-representation|complemento a dos]]:
el patrón de bits existe, pero la regla de signed-overflow de C sigue controlando el
programa fuente.

## Aliasing: bytes no siempre son tipos

Aliasing significa que dos expresiones se refieren al mismo storage. C permite cierto
aliasing y prohíbe otro para que el compilador pueda evitar recargas innecesarias. La regla
aproximada: accedé a un objeto mediante un lvalue de tipo compatible, una versión
calificada de ese tipo, ciertos casos de agregados o un character type. Acceder a un objeto
`float` mediante un `uint32_t *` solo porque ambos tienen 4 bytes no está definido en
general.

La trampa común:

```c
float f = 1.0f;
uint32_t bits = *(uint32_t *)&f;  // strict-aliasing UB
```

Esto muchas veces "funciona" en un build debug porque los bytes efectivamente están ahí. El
problema no es la memoria; es la promesa que le hiciste al compilador. Si se asume que un
`float *` y un `uint32_t *` no hacen alias, el optimizador puede mantener el float en un
registro, reordenar stores o reutilizar valores viejos. La forma definida de copiar
representación de objeto es `memcpy` hacia un objeto entero del mismo tamaño. Los
compiladores modernos optimizan ese `memcpy` chico hasta hacerlo desaparecer.

Los character types son especiales: inspeccionar la representación de un objeto mediante
`unsigned char *` está definido. Por eso el dump de bytes de la primera nota era legal, y
por eso el código binario suele usar bytes en las fronteras mientras mantiene honesto el
acceso tipado dentro del programa.

## Sequencing: los side effects necesitan orden

Las expresiones C no siempre evalúan de izquierda a derecha. El estándar define lugares
donde las evaluaciones están secuenciadas, y lugares donde el orden no está especificado.
Si modificás el mismo objeto escalar más de una vez entre puntos de sequencing, o lo
modificás y también lo leés para un cómputo de valor no relacionado sin sequencing, podés
tener UB.

Forma mala clásica:

```c
int i = 3;
int y = i++ + ++i;  // modificaciones no secuenciadas de i
```

No intentes memorizar qué "imprime en GCC." No hay valor para memorizar. El programa violó
las reglas de sequencing. El fix es simple y aburrido: separá los side effects en
sentencias para que el orden sea explícito. En C de bajo nivel, la secuencia aburrida es
una virtud. Le da al compilador un contrato real y al reviewer algo auditable.

El orden de argumentos en llamadas a función es otra trampa. En muchas llamadas, el orden
en que se evalúan los argumentos es unspecified. Si dos argumentos mutan o dependen del
mismo objeto, primero sacalos a temporarios con nombre.

## Artefacto ejecutable: las versiones definidas

Este programa no ejecuta las expresiones malas. Muestra los tres patrones definidos:
multiplicación signed chequeada, type punning alias-safe con `memcpy`, y side effects
secuenciados.

```c
// demo.c — tres bordes de UB y sus versiones definidas.
// gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void checked_mul_int(int a, int b) {
    int out = 0;

    if (__builtin_mul_overflow(a, b, &out)) {
        printf("signed overflow avoided: %d * %d does not fit in int\n", a, b);
        return;
    }

    printf("checked multiply: %d * %d = %d\n", a, b, out);
}

static void show_float_bits(float value) {
    uint32_t bits = 0;

    memcpy(&bits, &value, sizeof bits);
    printf("alias-safe punning: %.2f -> 0x%08X\n", value, (unsigned)bits);
}

static void sequenced_update(int start) {
    int i = start;
    int left = i;
    i++;
    int right = i;
    i++;

    printf("sequenced effects: start=%d left=%d right=%d final=%d sum=%d\n",
           start, left, right, i, left + right);
}

int main(void) {
    checked_mul_int(1000, 20);
    checked_mul_int(INT_MAX, 2);

    show_float_bits(1.50f);

    sequenced_update(3);
    return 0;
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
```

Salida real:

```
checked multiply: 1000 * 20 = 20000
signed overflow avoided: 2147483647 * 2 does not fit in int
alias-safe punning: 1.50 -> 0x3FC00000
sequenced effects: start=3 left=3 right=4 final=5 sum=7
```

Fijate el patrón: el programa mantiene la operación peligrosa fuera del camino ejecutado.
Chequea antes de hacer overflow, copia bytes en vez de mentir mediante un puntero
incompatible, y le da a cada side effect su propia sentencia. Eso no es menos "low-level."
Es la forma definida de escribir bajo nivel.

## Modos de falla y trade-offs

- **Asumir que el comportamiento de debug es el contrato.** UB suele parecer estable bajo
  `-O0` porque el compilador preserva la forma del fuente. El mismo fuente puede cambiar
  bajo `-O2`, LTO u otro compilador.
- **Usar unsigned por reflejo.** Unsigned evita UB de signed-overflow, pero introduce
  aritmética modular y trampas de conversión signed/unsigned. Usalo intencionalmente.
- **Casts de puntero como serialización.** Castear un byte buffer a un `struct *` de
  protocolo puede violar alineación, effective type, padding y supuestos de endianness.
  Parseá bytes explícitamente en las fronteras.
- **Expresiones ingeniosas que esconden side effects.** `++`, `--`, asignación dentro de
  expresiones y argumentos de macros pueden hacer difícil revisar sequencing. Separalos
  cuando cambian estado.
- **Huecos de herramientas.** UBSan atrapa muchos problemas de signed-overflow y
  sequencing, pero los bugs de strict aliasing pueden ser sutiles y depender de inputs. La
  forma del código sigue importando.

## En la práctica

- **Hacé explícita la política de overflow.** Elegí aritmética chequeada, saturada,
  ensanchada o modular según el dominio. No dejes que la política de overflow sea un
  accidente de elección de tipo.
- **Preferí `memcpy` para mover representación.** Comunica intención, obedece las reglas de
  effective type y optimiza bien para tamaños chicos fijos.
- **Dejá `-fno-strict-aliasing` como último recurso.** Puede hacer sobrevivir código legacy,
  pero entrega información del optimizador globalmente. Corregir la forma de aliasing es
  mejor cuando el código es tuyo.
- **Mantené aburridos los side effects.** Una mutación por sentencia no es infantil; es C
  revisable.
- **Prendé warnings y sanitizers enfocados.** Sumá `-Wstrict-aliasing`, `-Wsequence-point`,
  `-Wconversion`, UBSan y ASan al loop de tests donde encajen.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: el contrato]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|El sistema de tipos de C es débil]] · [[lowlevel/c-from-the-metal/why-c-still-matters|Por qué C todavía importa]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Complemento a dos y representación de enteros]] · [[lowlevel/machine-model/endianness|Endianness]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre signed overflow, effective type, permisos de aliasing y reglas de sequencing. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Undefined behavior** — ejemplos concisos de signed overflow, strict aliasing y side effects no secuenciados como UB. https://en.cppreference.com/w/c/language/behavior
- **cppreference — Order of evaluation** — la referencia práctica para sequencing, orden unspecified y modificaciones no secuenciadas. https://en.cppreference.com/w/c/language/eval_order
- **cppreference — Object model and effective type** — las reglas detrás de representación de objetos, inspección con character types y aliasing. https://en.cppreference.com/w/c/language/object
- **GCC integer overflow built-ins** — helpers de aritmética chequeada usados por el demo y por código C de producción que necesita manejar overflow. https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html
- **Clang UndefinedBehaviorSanitizer docs** — cobertura de sanitizer para signed overflow, alineación, null y varias otras familias de UB. https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
