---
title: "Los strings son solo `char*` (y las consecuencias)"
description: Un string de C no es un objeto string administrado. Es una secuencia de bytes char que termina en el primer byte NUL, normalmente alcanzada mediante un puntero char que no lleva longitud ni capacidad.
tags: [c, strings, char-pointer, null-terminated, buffers]
order: 9
updated: 2026-06-29
---
# Los strings son solo `char*` (y las consecuencias)

C no tiene un objeto string incorporado con longitud, capacidad, encoding y bounds. Lo que
los programadores C llaman "string" es un **null-terminated byte string** (string de bytes
terminado en null): una secuencia de bytes `char` cuyo final está marcado por el primer
byte `'\0'`. El valor que reciben la mayoría de las APIs es solo `char *` o `const char *`,
un puntero al primer byte. Ese puntero no sabe cuántos bytes son válidos, cuánta capacidad
queda, si el storage es escribible, ni si existe un terminador.

> El reset: `char *` es una dirección, no un objeto string. El string existe solo si los
> bytes alcanzables desde esa dirección contienen eventualmente un terminador NUL.

## La convención: bytes hasta NUL

Este es un objeto array real:

```c
char word[] = "metal";
```

El initializer copia seis bytes dentro de `word`: `m`, `e`, `t`, `a`, `l` y `'\0'`.
`sizeof word` por eso es 6. `strlen(word)` es 5 porque `strlen` cuenta bytes antes del
primer NUL y no incluye el terminador.

Esto es distinto:

```c
const char *literal = "metal";
```

Acá `"metal"` es un string literal con tipo array en storage que el programa debe tratar
como read-only. En la mayoría de las expresiones decae a un puntero a su primer carácter, y
ese puntero se guarda en `literal`. `sizeof literal` es el tamaño del puntero, no el tamaño
del literal. `sizeof "metal"` sigue siendo 6 porque `sizeof` es uno de los contextos donde
un array no decae.

Esto conecta directo con
[[lowlevel/c-from-the-metal/arrays-and-array-to-pointer-decay|array-to-pointer decay]]: los
arrays pueden contener bytes de string, pero el puntero pasado a una función perdió el
bound del array.

## Cómo funciona realmente

Las funciones de string de la biblioteca estándar operan sobre esta convención. `strlen`
camina hacia adelante hasta ver `'\0'`. `printf("%s", p)` imprime bytes hasta ver `'\0'`.
`strcpy` copia hasta incluir el terminador. Ninguna de estas funciones puede inferir la
capacidad del buffer destino desde un puntero desnudo.

Eso vuelve no negociables cuatro hechos:

| Cosa | Qué sabe C |
|---|---|
| `char buf[16]` en su propio scope | 16 bytes de storage |
| `char *p = buf` | dirección del primer byte |
| `const char *s = "hi"` | puntero a bytes de literal read-only por convención |
| `strlen(s)` | conteo escaneando hasta el primer `'\0'` |

`char` también significa byte acá, no carácter Unicode. Un string UTF-8 usa múltiples bytes
`char` para algunos caracteres visibles por el usuario. `strlen` cuenta bytes antes de NUL,
no graphemes, code points ni columnas de display. Bytes NUL embebidos cortan el string de C
temprano, por eso los datos binarios necesitan APIs de puntero-más-longitud como `memcpy`.

Los string literals merecen cuidado especial. En C, su tipo es un array de `char`, pero
intentar modificar uno tiene undefined behavior. Usá `const char *` para punteros a
literals así el sistema de tipos ayuda a mantener read-only el storage read-only.

## Artefacto ejecutable: puntero, array, terminador

El demo vive en `examples/c-from-the-metal/strings-are-just-char-pointers/demo.c`. Imprime
la diferencia entre tamaño de array, tamaño de puntero, longitud de string y bytes crudos.

```c
#include <stdio.h>
#include <string.h>

static void inspect_parameter(const char *text) {
    printf("sizeof parameter      = %zu bytes\n", sizeof text);
    printf("strlen parameter      = %zu chars\n", strlen(text));
}

static void print_bytes(const char *label, const char *bytes, size_t count) {
    printf("%s", label);

    for (size_t i = 0; i < count; i++) {
        printf(" %02X", (unsigned char)bytes[i]);
    }

    printf("\n");
}

int main(void) {
    char word[] = "metal";
    const char *literal = "metal";
    char copied[8] = {0};
    char not_a_string[4] = {'b', 'u', 'g', '!'};
    char small[6] = {0};

    memcpy(copied, "low", 4);

    printf("sizeof word array     = %zu bytes\n", sizeof word);
    printf("strlen word           = %zu chars\n", strlen(word));
    printf("sizeof \"metal\"        = %zu bytes\n", sizeof "metal");
    printf("sizeof literal ptr    = %zu bytes\n", sizeof literal);
    printf("strlen literal        = %zu chars\n", strlen(literal));

    inspect_parameter(word);

    /* El array es modificable; el string literal debe tratarse como read-only. */
    word[0] = 'M';
    printf("modified array        = %s\n", word);

    /* strlen camina hasta encontrar NUL; esto no es un C string. */
    print_bytes("not_a_string bytes   =", not_a_string, sizeof not_a_string);

    int needed = snprintf(small, sizeof small, "%s", "systems");
    printf("snprintf needed       = %d chars\n", needed);
    printf("small buffer          = %s\n", small);

    print_bytes("copied bytes         =", copied, sizeof copied);

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
sizeof word array     = 6 bytes
strlen word           = 5 chars
sizeof "metal"        = 6 bytes
sizeof literal ptr    = 8 bytes
strlen literal        = 5 chars
sizeof parameter      = 8 bytes
strlen parameter      = 5 chars
modified array        = Metal
not_a_string bytes   = 62 75 67 21
snprintf needed       = 7 chars
small buffer          = syste
copied bytes         = 6C 6F 77 00 00 00 00 00
```

El array escribible tiene storage para el terminador, así que `sizeof word` es 6 y
`strlen` es 5. El puntero al literal mide 8 bytes en esta plataforma porque es solo un
puntero. Pasar `word` a `inspect_parameter` pierde el bound del array. `not_a_string` no
tiene terminador NUL a propósito, así que el programa imprime sus bytes y nunca llama
`strlen` sobre eso. `snprintf` escribe un string truncado y terminado en NUL, y devuelve la
longitud que habría necesitado: `"systems"` tiene 7 bytes antes del terminador, pero
`small` solo puede guardar cinco bytes visibles más NUL.

## Modos de falla y trade-offs

- **Terminador faltante.** Llamar `strlen`, `printf("%s")` o `strcpy` sobre bytes sin un NUL
  alcanzable hace que la función lea más allá del objeto. Eso es undefined behavior.
- **Buffer overflow.** Un destino `char *` no revela capacidad. Copias sin límite pueden
  sobrescribir objetos vecinos incluso cuando el source es un string válido.
- **Mutación de literals.** `char *p = "hello"; p[0] = 'H';` compila en algunos modos y
  tiene undefined behavior. Usá `const char *`.
- **Confusión con `sizeof`.** `sizeof buf` funciona solo mientras `buf` es un array en el
  scope actual. Una vez que decae a `char *`, `sizeof` da tamaño de puntero.
- **NULs embebidos.** Las APIs de C string cortan temprano. Frames de red, archivos y datos
  comprimidos son bytes con longitudes, no strings.
- **`strncpy` no es un `strcpy` seguro.** Puede dejar el destino sin terminador cuando el
  source es demasiado largo, y rellena con ceros cuando el source es corto. Preferí manejo
  explícito de capacidad como `snprintf`, `memcpy` con longitudes conocidas o un helper
  local del proyecto con semántica clara.

## En la práctica

- **Llevá capacidad con buffers mutables.** Las APIs que escriben deberían recibir
  `(char *buf, size_t cap)` y definir si siempre terminan en NUL.
- **Llevá longitud con datos binarios.** Las APIs que procesan bytes arbitrarios deberían
  recibir `(const unsigned char *data, size_t len)` o un `struct` de slice, no fingir que
  los datos son un C string.
- **Usá `const char *` para texto prestado.** Documenta que el callee no debe mutar los
  bytes apuntados y protege los string literals.
- **Chequeá resultados de `snprintf`.** Un retorno mayor o igual a la capacidad significa
  que la salida fue truncada.
- **Reservá un byte para NUL.** Un buffer de tamaño `N` puede contener como máximo `N - 1`
  bytes visibles como string de C.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/arrays-and-array-to-pointer-decay|Arrays y array-to-pointer decay]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|El sistema de tipos de C es débil]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: el contrato]] · [[lowlevel/c-from-the-metal/structs-unions-and-bitfields|Structs, unions y bitfields]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre string literals, arrays de character type, funciones de string de biblioteca, bounds de objetos y undefined behavior. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — String literal** — reglas exactas para storage de string literals, terminadores, tipos, concatenación y por qué modificarlos es undefined. https://en.cppreference.com/c/language/string_literal
- **cppreference — Null-terminated byte strings** — referencia para el modelo de strings de C y las funciones de biblioteca estándar que consumen null-terminated byte strings. https://en.cppreference.com/w/c/string/byte
- **cppreference — `strlen`** — contrato preciso: cuenta caracteres antes del primer null character, con comportamiento dependiente de un null-terminated byte string válido. https://en.cppreference.com/w/c/string/byte/strlen
- **cppreference — `snprintf`** — valor de retorno, comportamiento de truncación y contrato de formatting con capacidad. https://en.cppreference.com/w/c/io/fprintf
- **Beej's Guide to C — Strings** — explicación accesible de arrays, punteros, terminadores NUL y por qué la longitud de string es una convención. https://beej.us/guide/bgc/html/split/strings.html
