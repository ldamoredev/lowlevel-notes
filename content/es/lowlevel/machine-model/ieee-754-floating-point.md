---
title: "IEEE 754 floating point"
description: Cómo la máquina guarda los números reales — sign, exponent y mantissa en notación científica binaria. Por qué los bits finitos hacen inexactos a la mayoría de los decimales (así que 0.1 + 0.2 != 0.3), qué son inf y NaN, y por qué nunca tenés que comparar floats con ==.
tags: [machine-model, floating-point, ieee-754, precision, nan]
order: 10
updated: 2026-06-22
---
# IEEE 754 floating point

Los enteros son exactos; los números reales no. Un `float` o `double` guarda un número en
notación científica binaria — **sign × 1.mantissa × 2^exponent** — con una cantidad fija de
bits para cada parte. Como los bits son finitos, la enorme mayoría de las fracciones
decimales no se pueden representar exactamente, igual que 1/3 no tiene expansión decimal
finita. **IEEE 754** es el estándar que clava este formato, las reglas de redondeo y los
valores especiales (infinito, NaN) que toda CPU y lenguaje moderno implementa. La
consecuencia estrella — `0.1 + 0.2 != 0.3` — no es un bug; es precisión finita en base 2, y
una vez que ves la codificación es obvio.

> El reset: un `double` es una *aproximación* de un número real, no el número en sí. La
> igualdad (`==`) sobre floats casi siempre está mal; la aritmética acumula error de
> redondeo; y algunos "números" (NaN) ni siquiera son iguales a sí mismos.

## El formato: sign, exponent, mantissa

Un valor es `(-1)^sign × 1.mantissa × 2^(exponent − bias)`. El `1.` inicial es implícito
(para números normalizados), lo que regala un bit de precisión. Los dos tamaños comunes:

| | `float` (32-bit) | `double` (64-bit) |
|---|---|---|
| Sign | 1 bit | 1 bit |
| Exponent | 8 bits (bias 127) | 11 bits (bias 1023) |
| Mantissa | 23 bits | 52 bits |
| Dígitos decimales | ~7 | ~15–16 |

El exponente se guarda **con bias** (sumá 127 / 1023) para poder representar negativos sin un
signo propio. Ejemplos resueltos de un `float` de 32 bits:

- `1.0` → `0x3F800000`: sign 0, exponent 127 (→ 2⁰), mantissa 0 → `1.0 × 2⁰`.
- `0.5` → `0x3F000000`: exponent 126 (→ 2⁻¹) → `1.0 × 2⁻¹`.
- `-2.0` → `0xC0000000`: sign 1, exponent 128 (→ 2¹) → `−1.0 × 2¹`.
- `0.1` → `0x3DCCCCCD`: exponent 123 (→ 2⁻⁴), mantissa `0x4CCCCD` — fijate el `CCC…D` que se
  repite, la huella de un valor que *no* se puede guardar exacto.

## Por qué 0.1 + 0.2 != 0.3

En binario, `0.1` es `0.0001100110011…` que se repite para siempre — no hay suma finita de
potencias de dos que dé exactamente un décimo. Así que el `0.1` guardado es el *double
representable más cercano*, levemente desviado; lo mismo `0.2`. Sumá los dos valores
redondeados y redondeá de nuevo, y el resultado cae un ULP (unit in the last place) por
encima del double más cercano a `0.3`:

```
0.1 + 0.2 = 0.30000000000000004
0.3       = 0.29999999999999999
equal? NO
```

Las dos líneas tienen 17 dígitos significativos — lo máximo que un `double` puede distinguir.
Nada está roto: cada paso hizo round-to-nearest correcto, pero el redondeo de las entradas y
el de la suma no se cancelan. El mismo efecto hace que `for (double x = 0; x != 1.0; x +=
0.1)` sea un loop infinito. Esto no es específico de C — Python, JavaScript y Java imprimen
el idéntico `0.30000000000000004`.

## Valores especiales

Los valores extremos del campo de exponente están reservados para codificar números no
finitos:

- **±0** — sí, cero con signo; `+0.0 == -0.0` es true pero `1/+0` y `1/-0` difieren.
- **±infinito** — exponente todo unos, mantissa cero. Producido por overflow o `x / 0.0`. La
  aritmética con él está definida: `inf + 1 == inf`.
- **NaN (Not a Number)** — exponente todo unos, mantissa distinta de cero. Producido por
  `0.0/0.0`, `sqrt(-1)`, `inf - inf`. Su trampa definitoria: **`NaN == NaN` es false** — un
  NaN compara desigual contra todo, incluido sí mismo. Así es como lo testeás (`x != x`, o
  `isnan(x)`).
- **Subnormales** — exponente cero: números muy chicos cerca del 0 que dejan caer el 1
  implícito para degradar la precisión de forma gradual en vez de saltar directo a cero.

## Miralo

```c
// floats.c — la desigualdad, el layout de bits y los especiales.
// gcc -O0 -Wall -Wextra floats.c -o floats -lm && ./floats
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>

static void show_float(const char *name, float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof bits);             // type-pun vía memcpy (seguro)
    uint32_t sign = bits >> 31;
    uint32_t exp  = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF;
    printf("%-6s = %-10g 0x%08X  sign=%u exp=%u(2^%d) mant=0x%06X\n",
           name, (double)f, bits, sign, exp, (int)exp - 127, mant);
}

int main(void) {
    printf("0.1 + 0.2 = %.17f\n", 0.1 + 0.2);
    printf("0.3       = %.17f\n", 0.3);
    printf("equal? %s\n\n", (0.1 + 0.2 == 0.3) ? "yes" : "NO");

    show_float("1.0", 1.0f);
    show_float("0.5", 0.5f);
    show_float("-2.0", -2.0f);
    show_float("0.1", 0.1f);                     // la mantissa se repite: inexacto

    float inf = 1.0f / 0.0f, nan = 0.0f / 0.0f;
    printf("\n1.0/0.0 = %g (isnan %d)\n", (double)inf, isnan(inf));
    printf("0.0/0.0 = %g (isnan %d)\n", (double)nan, isnan(nan));
    printf("nan == nan ? %s\n", (nan == nan) ? "true" : "false");

    printf("\ntolerant compare: fabs((0.1+0.2)-0.3) < 1e-9 ? %s\n",
           fabs((0.1 + 0.2) - 0.3) < 1e-9 ? "true" : "false");
    return 0;
}
```

El `memcpy` a un `uint32_t` es la forma portable de inspeccionar los bits de un float (un
cast `float*`→`int*` violaría strict aliasing). La salida hace concreta la codificación: los
exponentes mapean a potencias de dos, la mantissa de `0.1` visiblemente se repite, NaN falla
su propio test de igualdad, y la comparación tolerante tiene éxito donde `==` falló.

## Hacerlo bien

- **Nunca compares floats con `==`.** Usá una tolerancia: `fabs(a - b) < eps` para valores
  cerca de 1, o un epsilon *relativo* (`fabs(a-b) <= eps * fmax(fabs(a), fabs(b))`) a través
  de escalas. Elegir `eps` es específico del problema — `FLT_EPSILON`/`DBL_EPSILON` son el
  salto en 1.0, un punto de partida, no una respuesta universal.
- **Por defecto preferí `double`;** usá `float` solo cuando el ancho de banda de memoria o de
  SIMD lo exija. La precisión extra previene muchas sorpresas por el costo de 4 bytes.
- **Nunca uses floating point para plata.** Usá centavos enteros (o una librería decimal);
  `0.10` no es representable, y los centavos tienen que ser exactos.
- **Cuidá la acumulación y el orden.** Sumar muchos valores pierde precisión; sumar el más
  grande al final o usar suma compensada (Kahan) ayuda. `(a + b) + c != a + (b + c)` en
  general.

## Modos de falla y trade-offs

- **Igualdad y contadores de loop.** Los guardas `x == 0.3` y `x != 1.0` fallan de forma
  impredecible. Contá con enteros; computá el float a partir del entero.
- **Envenenamiento por NaN.** Un NaN se propaga por toda operación posterior, y toda
  comparación con él es false — un sort o un `max` pueden romperse en silencio. Chequeá con
  `isnan`.
- **Cancelación catastrófica.** Restar dos floats casi iguales aniquila los dígitos
  significativos, dejando mayormente ruido de redondeo. Reformulá la matemática para
  evitarlo.
- **Sorpresas `float`↔`double`.** Las expresiones de precisión mixta promueven a `double`, y
  un literal como `0.1` es un `double` salvo que escribas `0.1f`; las conversiones silenciosas
  corren los resultados.

## En la práctica

- **Tratá todo float como "correcto a ~15 dígitos, después borroso".** Diseñá las
  comparaciones, los formatos y los tests alrededor de la aproximación, no de la exactitud.
- **`0.30000000000000004` es la pista canónica.** Cuando un total está desviado en el dígito
  16, es redondeo de floating-point, no un bug de lógica.
- **Los dominios exactos usan enteros.** Plata, contadores, índices de array, hashes —
  mantenelos en tipos enteros; reservá los floats para cantidades medidas/continuas.
- **Al debuggear, imprimí `%.17g` y los bits.** El print por defecto esconde el error que
  realmente está ahí; la precisión completa y el layout hex lo revelan.

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Complemento a dos y enteros]] · [[lowlevel/machine-model/endianness|Endianness]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **David Goldberg — *What Every Computer Scientist Should Know About Floating-Point Arithmetic*** — el tratamiento profundo canónico de redondeo, error e IEEE 754; arrancá acá. https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 2.4 — el layout de bits de float/double, redondeo y trampas, exactamente al nivel de este atlas. https://csapp.cs.cmu.edu/
- **IEEE 754-2019 — Standard for Floating-Point Arithmetic** — la spec autoritativa de formatos, valores especiales y modos de redondeo. https://ieeexplore.ieee.org/document/8766229
- **Bruce Dawson — *Comparing Floating Point Numbers, 2012 Edition*** — la guía práctica de epsilon, ULPs y por qué fallan los chequeos de tolerancia ingenuos. https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
- **cppreference — tipos floating-point y `<math.h>`** — semántica de `float`/`double`, `isnan`, `INFINITY`, `FLT_EPSILON` y compañía. https://en.cppreference.com/w/c/language/arithmetic_types
