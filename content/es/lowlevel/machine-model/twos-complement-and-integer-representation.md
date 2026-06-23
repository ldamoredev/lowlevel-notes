---
title: "Complemento a dos y representación de enteros"
description: Cómo la máquina guarda enteros con signo — complemento a dos, la codificación donde el mismo hardware de suma/resta sirve para signed y unsigned, -1 es todos unos, e INT_MIN no tiene gemelo positivo. Por qué el overflow signed es UB pero el unsigned da la vuelta, y la trampa de comparación signed/unsigned.
tags: [machine-model, integers, twos-complement, overflow, signedness]
order: 8
updated: 2026-06-22
---
# Complemento a dos y representación de enteros

La nota anterior dijo que un número es solo bytes. Ahora: ¿*qué* bytes? Para enteros sin
signo es obvio — base 2 pelada. Para enteros **con signo** casi toda máquina usa
**complemento a dos** (two's complement), una codificación elegida por una propiedad
hermosa: la *misma* circuitería de suma y resta sirve para valores con y sin signo, sin
casos especiales para el signo. El costo es una asimetría deliberada — un número negativo
más que positivos — y un conjunto de bordes filosos (overflow, conversión signed/unsigned)
que causan algunos de los bugs más comunes y peligrosos de C. Esta nota hace concreta la
codificación para que esos bordes dejen de ser sorpresas.

> El reset: no hay ningún "signo menos" guardado en ningún lado. La negatividad es solo un
> patrón de bits que el hardware acordó interpretar de cierta forma. `-1` y `4294967295`
> son los *mismos 32 bits* — solo el tipo decide cuál quisiste decir.

## Cómo funciona el complemento a dos

Para un entero con signo de N bits, el bit más alto tiene peso **negativo**. En 32 bits el
valor es:

```
value = -b31·2^31 + b30·2^30 + ... + b1·2^1 + b0·2^0
```

Así que el bit alto en 1 no significa "restar" como flag — contribuye −2³¹ directamente.
La regla para **negar** un valor es "invertí todos los bits, después sumá 1" (`-x == ~x + 1`):

| Valor | Hex 32-bit | Bit alto |
|---|---|---|
| `0` | `0x00000000` | 0 |
| `1` | `0x00000001` | 0 |
| `-1` | `0xFFFFFFFF` | 1 |
| `INT_MAX` (2147483647) | `0x7FFFFFFF` | 0 |
| `INT_MIN` (−2147483648) | `0x80000000` | 1 |

Dos consecuencias salen directo:

- **−1 es todos unos.** Cada bit prendido es el inverso aditivo de 1 — por eso las bitmasks
  y `-1` aparecen de forma intercambiable en código de bajo nivel.
- **El rango es asimétrico.** `INT_MIN = −2³¹` pero `INT_MAX = 2³¹−1`. Hay 2³¹ negativos y
  solo 2³¹−1 positivos, así que **`−INT_MIN` no es representable** — negarlo hace overflow.
  Este solo hecho rompe el `abs()` ingenuo, el parsing ingenuo, y más.

El premio: como la codificación da la vuelta módulo 2ᴺ, `a + b` y `a − b` producen el patrón
de bits correcto leas los operandos como signed o unsigned. La CPU tiene *un* `add`, no dos.
(Por eso una instrucción `add` setea flags para las dos interpretaciones; la
[[lowlevel/machine-model/registers-and-the-isa|ISA]] no trackea el signo — tus tipos sí.)

## El overflow signed es UB; el unsigned da la vuelta

Esta es la parte que muerde más fuerte, y es asimétrica en C:

- **El overflow unsigned está definido.** La aritmética unsigned es modular: `0u - 1 ==
  UINT_MAX`, `UINT_MAX + 1 == 0`. Garantizado por el estándar, perfectamente portable.
- **El overflow signed es undefined behavior.** `INT_MAX + 1` es **UB** (comportamiento
  indefinido) — no "da la vuelta a INT_MIN". El compilador puede asumir que el overflow
  signed *nunca pasa* y optimizar sobre esa base, así que una expresión signed que hace
  overflow puede hacer literalmente cualquier cosa, incluido miscompilar el código de
  alrededor.

Esto enreda a todos los que vienen de lenguajes donde `int` da la vuelta en silencio. En C,
`for (int i = 0; i <= n; i++)` con `n == INT_MAX` es un *loop infinito que el optimizador
puede borrar o destrozar*, porque el test de salida asume que no hay overflow. El fix es
saber qué enteros pueden hacer overflow y usar unsigned (o tipos más anchos, o `-fwrapv`, o
aritmética chequeada) donde importa. Mirá [[lowlevel/c-from-the-metal/index|C desde el
metal]] para el contrato completo de UB.

## La trampa de conversión signed/unsigned

Cuando mezclás signed y unsigned en una expresión, C convierte el **operando signed a
unsigned** (las "usual arithmetic conversions"). Los bits no cambian — pero su significado
se da vuelta, y un número negativo se vuelve uno positivo enorme:

```c
int a = -1;
unsigned b = 0;
if (a > b) { /* ESTO CORRE */ }   // -1 se convierte a UINT_MAX (4294967295) > 0
```

`-1 > 0u` es **true**, porque `-1` reinterpretado como unsigned es `0xFFFFFFFF`. La misma
trampa se esconde en `for (unsigned i = n; i >= 0; i--)` — un loop infinito, porque un
unsigned `i` nunca es < 0 — y en comparar un length con signo contra el `size_t` unsigned de
`strlen`. Los compiladores modernos avisan (`-Wsign-compare`); hacele caso.

## Mirá los bits

```c
// integers.c — patrones de bits, negación, wraparound y la trampa de comparación.
// gcc -O0 -Wall -Wextra integers.c -o integers && ./integers
#include <stdio.h>
#include <limits.h>

static void bits32(const char *name, unsigned int v) {
    printf("%-8s = %11d  0x%08X  ", name, (int)v, v);
    for (int i = 31; i >= 0; i--) {
        putchar((v >> i) & 1 ? '1' : '0');
        if (i % 8 == 0) putchar(' ');
    }
    putchar('\n');
}

int main(void) {
    printf("INT_MAX=%d  INT_MIN=%d  UINT_MAX=%u\n\n", INT_MAX, INT_MIN, UINT_MAX);

    bits32("0", 0);
    bits32("1", 1);
    bits32("-1", (unsigned)-1);                 // todos unos
    bits32("INT_MAX", (unsigned)INT_MAX);       // 0x7FFFFFFF
    bits32("INT_MIN", (unsigned)INT_MIN);       // 0x80000000

    int x = 5;
    printf("\nnegate 5 via ~x + 1 = %d\n", ~x + 1);

    unsigned u = 0;
    printf("unsigned 0u - 1     = %u   (wraps to UINT_MAX)\n", u - 1);

    int a = -1; unsigned b = 0;                 // la trampa de conversión
    printf("in (a > b): (unsigned)(-1) = %u, so a > b is %s\n",
           (unsigned)a, ((unsigned)a > b) ? "true (!)" : "false");
    return 0;
}
```

La salida lo confirma: `-1` es `0xFFFFFFFF`, `INT_MIN` es `0x80000000`, `~5 + 1 == -5`,
`0u - 1 == 4294967295`, y el `-1` signed se vuelve `4294967295` en el instante en que se
compara contra un unsigned. (Ojo: los casts `(unsigned)a` son a propósito — escribir la
comparación como `a > b` directo es justo lo que marca `-Wsign-compare`.)

## Modos de falla y trade-offs

- **`abs(INT_MIN)` es UB.** `-INT_MIN` hace overflow porque no hay gemelo positivo. La misma
  asimetría rompe `-x`, `x / -1` y el parsing ingenuo del valor más negativo.
- **Asumir que el signed da la vuelta.** `INT_MAX + 1` es indefinido, no `INT_MIN`. Los
  optimizadores lo explotan; "funcionaba en debug" no es seguridad. Usá unsigned o math
  chequeada para valores que puedan hacer overflow.
- **Mezclar signo en comparaciones/loops.** `size_t` (unsigned) vs `int` (signed) es el
  clásico; `i >= 0` en un contador unsigned nunca termina. Mantené los contadores de loop y
  los tamaños del mismo signo.
- **Right-shift de valores signed.** `>>` sobre un int signed negativo es
  implementation-defined (normalmente shift aritmético). Para trabajo de bits, shifteá tipos
  **unsigned**.

## En la práctica

- **Los tamaños e índices son `size_t` (unsigned); usá `int` signed para valores que van a
  negativo.** No los mezcles en una comparación — convertí explícito para que el lector lo
  vea.
- **Tratá `-Wsign-compare` y `-Wconversion` como errores.** Atrapan la trampa de conversión
  antes de que llegue a producción; en el trabajo de OS de este atlas, `-Werror` está
  prendido.
- **Para semántica exacta de wraparound, usá unsigned;** para trapear en overflow, usá
  helpers chequeados (`__builtin_add_overflow`) o `-fsanitize=signed-integer-overflow` en los
  tests.
- **Acordate de `-1 == 0xFFFF...`** Ver todos unos en un debugger y reconocer "eso es −1 (o
  un máximo unsigned, o una bitmask)" es fluidez cotidiana de bajo nivel.

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]] · [[lowlevel/machine-model/registers-and-the-isa|Registros y la ISA]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 2 — representación de enteros, complemento a dos, overflow y conversiones signed/unsigned; la columna de esta nota. https://csapp.cs.cmu.edu/
- **Jens Gustedt — *Modern C*** — el modelo de enteros de C, las usual arithmetic conversions y por qué el unsigned da la vuelta pero el signed no. https://gustedt.gitlabpages.inria.fr/modern-c/
- **cppreference — Operadores aritméticos y conversiones implícitas** — las reglas exactas de las usual arithmetic conversions y el overflow de enteros. https://en.cppreference.com/w/c/language/operator_arithmetic
- **Dietz et al. — *Understanding Integer Overflow in C/C++*** — estudio empírico de bugs de overflow y por qué el UB del overflow signed importa en la práctica. https://www.cs.utah.edu/~regehr/papers/overflow12.pdf
- **ISO/IEC 9899 (estándar C), §6.2.5 y §6.3.1.8** — los tipos enteros, las reglas de rango de conversión y la semántica de overflow. https://www.open-std.org/jtc1/sc22/wg14/
