---
title: "Endianness"
description: El orden en que los bytes de un valor multi-byte quedan en memoria — little-endian (x86-64, ARM) guarda primero el byte menos significativo, big-endian (network order) el más significativo. Invisible dentro de una máquina, muerde en el instante en que los bytes cruzan un archivo, un socket o una arquitectura distinta.
tags: [machine-model, endianness, byte-order, serialization, networking]
order: 9
updated: 2026-06-22
---
# Endianness

Un entero de 32 bits son cuatro bytes — pero ¿en qué *orden* quedan esos cuatro bytes en
memoria? Esa elección es el **endianness** (orden de bytes), y hay dos respuestas.
**Little-endian** guarda el byte menos significativo en la dirección más baja ("little end
first"); **big-endian** guarda primero el byte más significativo. x86-64 y (en la práctica)
ARM64 son little-endian; los protocolos de red y algunas arquitecturas viejas son
big-endian. Dentro de una sola máquina la elección es completamente invisible — la
aritmética y las comparaciones operan sobre *valores*, no sobre bytes. Se vuelve real, y
muerde fuerte, en el momento en que los bytes crudos cruzan un límite: un archivo, un socket
o una CPU de endianness distinto.

> El reset: el *valor* `0x0A0B0C0D` es inequívoco; su *layout de bytes* no. El número con el
> que computás y los bytes que escribís a disco o al cable no son la misma cosa, y
> confundirlos corrompe datos en silencio.

## Dos órdenes

Guardá el valor de 32 bits `0x0A0B0C0D` (MSB `0A`, LSB `0D`) y leé la memoria desde la
dirección más baja hacia arriba:

| Dirección | Little-endian | Big-endian |
|---|---|---|
| base+0 | `0D` (LSB) | `0A` (MSB) |
| base+1 | `0C` | `0B` |
| base+2 | `0B` | `0C` |
| base+3 | `0A` (MSB) | `0D` (LSB) |

Little-endian pone el *little end* (byte menos significativo) primero. Se siente al revés
cuando leés un hex dump, pero tiene una propiedad linda: el byte bajo de un valor está en la
misma dirección lo leas como tipo de 8, 16, 32 o 64 bits, así que los casts que achican no
necesitan matemática de direcciones. Big-endian coincide con cómo escribimos los números de
izquierda a derecha, que es por qué se eligió como **network byte order** (orden de bytes de
red, RFC 1700) — en el cable, el byte más significativo va primero.

Las dos familias:

- **Little-endian:** x86, x86-64, ARM/ARM64 (default), RISC-V. La aplastante mayoría de lo
  que vas a correr.
- **Big-endian:** protocolos de red (headers TCP/IP), muchos formatos de archivo (PNG, JPEG,
  archivos `.class` de Java), SPARC/PowerPC/m68k viejos, y la mayoría de las specs que dicen
  "este formato es big-endian".

## Por qué normalmente es invisible

Podés escribir C durante años sin pensar en endianness, porque la CPU es
*auto-consistente*: guarda y carga los valores multi-byte en el mismo orden siempre, así que
`x + 1`, `x == y` y `x << 8` producen todos el *valor* correcto sin importar el layout de
bytes. El orden solo se vuelve observable cuando dejás de tratar la memoria como valores
tipados y empezás a tratarla como bytes crudos:

- castear vía `unsigned char*` para inspeccionar bytes individuales,
- `fwrite`/`fread` de un struct o entero a un archivo binario,
- `send`/`recv` de enteros crudos por un socket,
- `memcpy` entre un valor tipado y un buffer de bytes.

Cada una de esas expone el orden de bytes en memoria — y si el lector asume un orden distinto
al del escritor, los bytes se malinterpretan en silencio.

## Cuándo muerde

- **Serialización entre máquinas.** Escribí `uint32_t 0x0A0B0C0D` a un archivo en una máquina
  little-endian (`0D 0C 0B 0A` en disco) y leelo de vuelta en una big-endian, y obtenés
  `0x0D0C0B0A` — un número distinto, sin error reportado.
- **Protocolos de red.** Los campos de TCP/UDP/IP son big-endian. Mandá un puerto o un length
  como un `int` crudo en orden de host desde x86 y el peer lee basura. Convertí siempre.
- **Formatos binarios de archivo.** Todo formato fija un endianness en su spec; parsear uno
  significa leer sus bytes en *su* orden, no en el de tu host.
- **Type punning.** Reinterpretar un buffer de bytes como `int` (o al revés) hornea el
  endianness del host — está bien localmente, mal en el momento en que el dato viaja.

## Miralo

```c
// endian.c — detectá el orden del host, dumpeá los bytes y convertí a network order.
// gcc -O0 -Wall -Wextra endian.c -o endian && ./endian
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>   // htonl, ntohl

static void dump(const char *label, const void *p, size_t n) {
    const unsigned char *b = p;
    printf("%-24s", label);
    for (size_t i = 0; i < n; i++) printf("%02x ", b[i]);
    putchar('\n');
}

int main(void) {
    uint32_t x = 0x0A0B0C0D;

    unsigned char first = *(unsigned char*)&x;          // primer byte en memoria
    printf("value: 0x%08X\n", x);
    printf("this machine is: %s\n\n",
           first == 0x0D ? "little-endian (LSB first)" : "big-endian (MSB first)");

    dump("bytes in memory:", &x, sizeof x);

    uint32_t net = htonl(x);                            // host -> network (big-endian)
    dump("after htonl (network):", &net, sizeof net);
    printf("ntohl(htonl(x)) == x ? %s\n", ntohl(net) == x ? "yes" : "no");

    uint32_t s = ((x & 0xFF000000u) >> 24) | ((x & 0x00FF0000u) >> 8) |   // bswap a mano
                 ((x & 0x0000FF00u) <<  8) | ((x & 0x000000FFu) << 24);
    printf("manual bswap: 0x%08X -> 0x%08X\n", x, s);
    return 0;
}
```

En una máquina little-endian el valor `0x0A0B0C0D` queda en memoria como `0d 0c 0b 0a`;
`htonl` lo reordena al big-endian `0a 0b 0c 0d` para el cable, y `ntohl` lo revierte exacto.
El swap a mano muestra lo que la instrucción de hardware `bswap` (y `__builtin_bswap32`) hace
en una sola op. Detectar el endianness leyendo el primer byte de un valor conocido es el
chequeo clásico en runtime.

## Hacerlo bien

- **Nunca serialices valores multi-byte crudos a través de un límite.** Convertí a un orden
  definido primero. Para red, usá `htonl`/`htons`/`ntohl`/`ntohs` (host↔red, o sea
  big-endian).
- **O serializá byte por byte con shifts explícitos** — `buf[0] = x >> 24; buf[1] = x >> 16;
  …` Esto es *independiente del endianness*: produce los mismos bytes en cualquier host, que
  es el enfoque más portable para formatos de archivo/cable.
- **Usá el endianness declarado del formato, no el de tu host.** Al parsear, leé cada campo en
  el orden que su spec exige; no asumas que el host coincide.
- **Existen helpers.** `__builtin_bswap16/32/64` (GCC/Clang) y el `<endian.h>` de Linux/BSD
  (`htole32`, `htobe32`, …) hacen las conversiones sin masks a mano.

## Modos de falla y trade-offs

- **`htonl` sobre un struct no hace nada útil.** Las conversiones son por campo, por entero;
  no podés byte-swapear un struct entero en una llamada. Serializá campo por campo.
- **Olvidar que es un no-op en big-endian.** `htonl` es identidad en un host big-endian, así
  que un bug que solo swapea en little-endian "funciona" en una máquina y falla en la otra.
  Testeá el camino de conversión, no solo el round-trip local.
- **El signo de `char` ≠ endianness.** Dos ejes de portabilidad separados; no confundas "¿es
  `char` signed?" con el orden de bytes.
- **Endianness ≠ orden de bits.** Dentro de un byte, la numeración de bits es una convención
  aparte; casi todo lo que tocás es a granularidad de byte, así que razoná en bytes.

## En la práctica

- **Dato solo-local: ignorá el endianness.** Un valor que nunca sale del proceso puede quedar
  en orden de host; convertir de gusto solo agrega bugs.
- **Cualquier cosa que cruce un cable, un archivo o un límite de arquitectura: elegí un orden
  y convertí.** Red → big-endian vía `htonl`/`ntohl`; formatos propios → documentá el orden y
  serializá con shifts explícitos.
- **x86-64 y ARM64 son little-endian** — tu día a día — **pero la red es big-endian.** Ese
  solo desajuste es donde viven la mayoría de los bugs de endianness.
- **Cuando los bytes se ven al revés en un hex dump, sospechá endianness primero.** `0D0C0B0A`
  donde esperabas `0A0B0C0D` es la pista.

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Complemento a dos y enteros]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/systems-programming/index|Systems Programming]]

## Fuentes

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 2 — byte ordering, big/little-endian y cómo mostrar los bytes de cualquier objeto; la columna de esta nota. https://csapp.cs.cmu.edu/
- **Beej's Guide to Network Programming — "Byte Order"** — orden de host vs red y la familia `htonl`/`ntohl`, con ejemplos. https://beej.us/guide/bgnet/html/#byte-order
- **RFC 1700 / RFC 791 — network byte order** — la especificación de que los protocolos de red son big-endian. https://www.rfc-editor.org/rfc/rfc791
- **cppreference — enteros de ancho fijo y `<stdint.h>`** — los tipos de ancho exacto con los que serializás. https://en.cppreference.com/w/c/types/integer
- **Linux `endian.h` man page — `htole32`/`htobe32` etc.** — helpers estándar de conversión de orden de bytes más allá de la familia de red. https://man7.org/linux/man-pages/man3/endian.3.html
