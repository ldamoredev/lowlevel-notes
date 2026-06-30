---
title: "Integer promotions y conversiones implícitas"
description: C convierte silenciosamente muchas expresiones enteras antes de evaluarlas. Integer promotions, usual arithmetic conversions, mezcla signed/unsigned, narrowing y char implementation-defined son donde empiezan muchos bugs legales.
tags: [c, integers, promotions, conversions, signedness]
order: 10
updated: 2026-06-29
---
# Integer promotions y conversiones implícitas

La aritmética de C no se hace en el tipo chico que escribiste solo porque la variable tenga
ese tipo. Antes de la mayoría de las operaciones enteras, C aplica **integer promotions**
(promociones enteras) y después las **usual arithmetic conversions** (conversiones
aritméticas usuales) para elegir un tipo común. Por eso dos valores `uint8_t` normalmente
se suman como `int`, por eso `-1 < 1u` es false en targets comunes, y por eso asignar un
resultado de vuelta a un tipo chico puede hacer narrowing silencioso. El tipo de la
expresión es una regla, no una sensación.

> El reset: cada expresión entera tiene un tipo después de las conversiones. Si no conocés
> ese tipo, todavía no sabés qué aritmética, comparación o asignación escribiste.

## Las promociones pasan antes de la aritmética

Los tipos enteros chicos como `char`, `signed char`, `unsigned char`, `short`, `uint8_t` y
`uint16_t` normalmente se promueven a `int` antes de la aritmética. Si `int` puede
representar todos los valores del tipo original, el tipo promovido es `int`; si no, es
`unsigned int`.

Eso significa que esto:

```c
uint8_t a = 250;
uint8_t b = 10;
int promoted_sum = a + b;
uint8_t narrowed_sum = (uint8_t)(a + b);
```

no suma en 8 bits. En la plataforma del demo, `a + b` tiene tipo `int` y valor `260`. Solo
la conversión explícita de vuelta a `uint8_t` lo wrappea a `4`. La promoción es útil:
evita muchos overflows accidentales durante aritmética intermedia. La trampa es asumir que
el tipo chico de storage controla la expresión.

## Cómo funciona realmente

Después de las integer promotions, los operadores binarios suelen aplicar las usual
arithmetic conversions. La regla aproximada es: elegir un tipo común que pueda representar
ambos operandos, o convertir a un tipo unsigned cuando el tipo signed no puede representar
todos los valores del tipo unsigned. Esa última parte es donde viven los bugs
signed/unsigned.

```c
int left = -1;
unsigned int right = 1;
left < right;  // left se convierte a unsigned int antes de comparar
```

En un `unsigned int` de 32 bits, convertir `-1` produce `4294967295`, así que la
comparación es false. El source se lee como matemática sobre enteros; el lenguaje lo evalúa
como una comparación después de la conversión.

La asignación es otro punto de conversión. Si el tipo destino no puede representar el valor
source, el resultado depende de la categoría del destino. La conversión a entero unsigned
wrappea módulo uno más que el valor máximo. El narrowing a signed que no puede representar
el valor es implementation-defined o puede levantar una señal implementation-defined.
Floating a entero, entero a floating y conversiones de enum tienen sus propias reglas. Los
warnings son tus amigos porque el lenguaje base permite mucho.

`char` plano es su propia trampa chica. Es un tipo distinto tanto de `signed char` como de
`unsigned char`, y si `char` plano se comporta como signed o unsigned es
implementation-defined. Usá `signed char` o `unsigned char` cuando importe la signedness;
usá `unsigned char` para bytes crudos.

## Artefacto ejecutable: mirá las conversiones

El demo vive en
`examples/c-from-the-metal/integer-promotions-and-implicit-conversions/demo.c`. Mantiene el
warning de comparación signed/unsigned local e intencional mientras compila limpio con
`-Wall -Wextra`.

```c
#include <stdint.h>
#include <stdio.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
static void compare_signed_and_unsigned(int left, unsigned int right) {
    printf("-1 < 1u              = %s\n", left < right ? "true" : "false");
}
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

int main(void) {
    uint8_t a = 250;
    uint8_t b = 10;
    int promoted_sum = a + b;
    uint8_t narrowed_sum = (uint8_t)(a + b);
    int negative = -1;
    unsigned int one = 1;
    unsigned int converted_negative = (unsigned int)negative;
    char plain = (char)0xFF;

    /* Los uint8_t se promueven a int antes de sumar. */
    printf("sizeof(a + b)        = %zu bytes\n", sizeof(a + b));
    printf("promoted sum          = %d\n", promoted_sum);
    printf("narrowed uint8_t sum  = %u\n", (unsigned)narrowed_sum);

    /* En una comparacion mixta, el int negativo se convierte a unsigned. */
    compare_signed_and_unsigned(negative, one);
    printf("(unsigned)-1         = %u\n", converted_negative);

    /* La signedness de char plano es implementation-defined. */
    printf("(char)0xFF as int     = %d\n", (int)plain);

    return 0;
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Salida real:

```
sizeof(a + b)        = 4 bytes
promoted sum          = 260
narrowed uint8_t sum  = 4
-1 < 1u              = false
(unsigned)-1         = 4294967295
(char)0xFF as int     = -1
```

La primera línea prueba que la expresión `uint8_t + uint8_t` se volvió un `int` de 4 bytes
acá. La línea de comparación muestra el valor signed convertido a `unsigned int`. La última
línea es implementation-defined: en esta plataforma `char` plano es signed, así que
`(char)0xFF` imprime `-1`.

## Modos de falla y trade-offs

- **Comparaciones de signedness mixta.** `int i` comparado con `size_t n` puede convertir
  `i` en un valor unsigned enorme cuando `i` es negativo.
- **Narrowing silencioso.** Asignar un resultado ancho a `uint8_t`, `int16_t` o un campo
  tipo enum puede descartar información. Hacé explícito el narrowing intencional y chequeá
  rangos en fronteras.
- **Asumir aritmética de tipo chico.** El storage `uint8_t` no implica aritmética
  intermedia de 8 bits. Las promociones pasan primero.
- **Portabilidad de `char` plano.** Código que trata `char` plano como signed en un target
  puede romperse en otro donde es unsigned.
- **Warnings vs ruido.** `-Wconversion` y `-Wsign-conversion` pueden hablar mucho, pero
  exponen exactamente las conversiones de esta nota. Usalos en desarrollo aunque tus flags
  de release sean más silenciosos.

## En la práctica

- **Elegí signedness por significado.** Usá tipos signed para cantidades que pueden ser
  negativas; usá unsigned cuando necesitás aritmética modular o bit patterns, no porque
  "los tamaños nunca son negativos."
- **Convertí cerca de la frontera.** Parseá, chequeá rango y convertí una vez. No dejes que
  conversiones implícitas se filtren por el core del programa.
- **Hacé ruidoso el narrowing.** Usá casts solo después de un chequeo o al lado de un
  comentario que explique el comportamiento módulo que querés.
- **Mantené índices compatibles con sus bounds.** Si un bound es `size_t`, preferí un índice
  `size_t`, o manejá valores sentinel negativos antes de la comparación.
- **Usá tipos de ancho fijo para representación, no magia.** `uint32_t` dice ancho de
  storage; las reglas de expresión siguen aplicando.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|El sistema de tipos de C es débil]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: el contrato]] · [[lowlevel/c-from-the-metal/more-ub-signed-overflow-aliasing-and-sequencing|Más UB: signed overflow, aliasing y sequencing]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Complemento a dos y representación de enteros]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre integer promotions, usual arithmetic conversions, conversiones de asignación y comportamiento entero implementation-defined. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Implicit conversions in C** — mapa compacto de integer promotions, usual arithmetic conversions, boolean conversion y assignment conversion. https://en.cppreference.com/w/c/language/conversion
- **cppreference — Arithmetic operators** — reglas de conversión específicas de operadores aritméticos, comparación, shifts y operaciones bitwise. https://en.cppreference.com/w/c/language/operator_arithmetic
- **cppreference — Integer types** — tipos enteros de ancho fijo, límites y la distinción entre ancho de storage y comportamiento de expresión. https://en.cppreference.com/w/c/types/integer
- **GCC warning options** — flags prácticos como `-Wconversion`, `-Wsign-conversion` y `-Wsign-compare` que exponen conversiones riesgosas. https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
- **Robert Seacord — *Effective C*** — guía con foco en seguridad sobre conversiones enteras, chequeos de rango, signedness y cómo evitar defectos por conversiones. https://nostarch.com/Effective_C
