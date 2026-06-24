---
title: "El sistema de tipos de C es débil (y lo que te cuesta)"
description: El sistema de tipos de C atrapa errores de forma, pero no codifica ownership, lifetimes, bounds, unidades, rangos ni la mayoría de la intención semántica. Por qué los typedefs son aliases, las conversiones cortan, los casts silencian al compilador, y la disciplina tiene que vivir por encima del type checker.
tags: [c, types, conversions, typedef, safety]
order: 2
updated: 2026-06-22
---
# El sistema de tipos de C es débil (y lo que te cuesta)

C tiene tipos, pero no son una red de seguridad como espera alguien que viene de lenguajes
administrados. Sobre todo describen **representación y operaciones**: cuántos bytes, qué
aritmética está permitida, si la aritmética de punteros escala, si una llamada a función
tiene la forma correcta. No codifican de forma confiable significado de dominio,
ownership, lifetime, bounds, unidades, estado de inicialización, o "este entero es un file
descriptor, no un user id." El compilador puede rechazar muchos programas mal formados,
pero también va a aceptar muchos programas equivocados porque son C perfectamente legal.

> El reset: un tipo de C suele significar "estos bits pueden usarse de esta forma", no
> "este valor es semánticamente válido." El type checker atrapa errores de categoría; el
> resto es tu contrato.

## Qué significa "débil"

"Débil" no significa "no hay sistema de tipos." C tiene tipos escalares, tipos agregados,
tipos función, qualifiers, tipos incompletos y reglas de tipos compatibles. Rechaza llamar
a una función con la cantidad equivocada de argumentos, asignar un `struct User *` a un
`struct File *` sin un cast, o dereferenciar algo que no es un puntero. Esos chequeos
importan.

Débil significa que el checker deja huecos semánticos enormes:

| Querés expresar | C puede expresar | C normalmente no puede probar |
|---|---|---|
| "esto es un user id, no un file descriptor" | `typedef uint32_t UserId` | distinción nominal frente a otro alias de `uint32_t` |
| "este puntero posee memoria de heap" | `T *` | quién debe llamar a `free`, y cuándo |
| "este string tiene longitud n" | `char *` más `size_t` | que el puntero y la longitud todavía coincidan |
| "este entero está en rango 0..255" | `uint8_t` | que no se truncó un valor más grande antes de guardarlo |
| "este booleano es true o false" | `_Bool` o convención con `int` | que cada flag `int` sea solo 0 o 1 |
| "este objeto está inicializado" | un nombre de tipo | que cada campo se haya escrito antes de usarlo |

Ese es el costo central: los tipos de C están cerca de la máquina, así que también están
cerca de la indiferencia de la máquina. Un registro no sabe si `42` es un user id, un
conteo de bytes, un tag de enum o un file descriptor. Salvo que construyas convenciones más
fuertes arriba de C, tu programa tampoco.

## Cómo funciona realmente

Las reglas de tipos de C mezclan chequeos estrictos, conversiones implícitas y escapes
explícitos. Las partes estrictas son reales: los tags de `struct` son distintos, los
prototipos de función se chequean, la aritmética de punteros depende del tipo apuntado, y
qualifiers como `const` restringen qué podés modificar a través de una expresión
particular. Las partes débiles son donde viven los bugs de sistemas.

- **`typedef` crea un alias, no un tipo nominal nuevo.** `typedef uint32_t UserId;` y
  `typedef uint32_t FileDescriptor;` son dos nombres para tipos enteros unsigned
  compatibles. Pasar uno donde se espera el otro es legal. El compilador ve
  representación, no dominio.
- **Las conversiones de enteros son rutinarias.** Las usual arithmetic conversions
  (conversiones aritméticas usuales), integer promotions y conversiones de asignación
  pueden cambiar signedness o ancho. Algunas son inofensivas; algunas convierten `-1` en
  un `size_t` enorme; algunas truncan bits altos. Los warnings ayudan, pero el lenguaje
  base permite mucho.
- **Los casts son una promesa tuya, no una prueba del compilador.** Un cast puede pedir
  una conversión, borrar un warning, reinterpretar un puntero o descartar qualifiers.
  Algunos casts son correctos y necesarios en fronteras. Otros son "por favor, dejá de
  chequear esto."
- **Los punteros llevan tipo, no tamaño ni lifetime.** `int *p` dice cómo interpretar
  `*p` y cuánto avanza `p + 1`. No dice si hay un `int` o mil, si el objeto apuntado sigue
  vivo, o si `p` puede liberarlo.
- **`void *` es type erasure deliberado.** APIs genéricas como `malloc`, `qsort` y sistemas
  de callbacks en C dependen de borrar el tipo de un object pointer y restaurarlo después.
  Eso es útil, pero el paso de restauración es tu responsabilidad.

A nivel máquina, la mayoría de estos valores son solo registros y memoria. El compilador
usa los tipos para elegir anchos de instrucción, escala de addressing, detalles de calling
convention y optimizaciones. Después la información de tipos casi desaparece del código
emitido. Si mentiste en el fuente, la máquina ejecuta la mentira.

## Artefacto ejecutable: legal pero peligroso

Este programa compila limpio con `-Wall -Wextra`. No contiene undefined behavior
(comportamiento indefinido). Ese es el punto: los puntos débiles acá no son crashes
exóticos. Son programas C legales cuyos tipos no alcanzaron para llevar suficiente
intención.

```c
// demo.c — el sistema de tipos de C chequea la forma, no la intencion.
// gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdint.h>
#include <stdio.h>

typedef uint32_t UserId;
typedef uint32_t FileDescriptor;

static void load_user(UserId id) {
    printf("load_user(%u)\n", (unsigned)id);
}

static const char *flag_state(int enabled) {
    return enabled ? "enabled" : "disabled";
}

int main(void) {
    UserId user = 42;
    FileDescriptor fd = 42;

    load_user(user);
    load_user(fd);  // C acepta esto: UserId y FileDescriptor son el mismo tipo real.

    printf("flag_state(1)    = %s\n", flag_state(1));
    printf("flag_state(1234) = %s\n", flag_state(1234));
    printf("flag_state(-7)   = %s\n", flag_state(-7));

    unsigned int big = 300;
    uint8_t small = (uint8_t)big;  // Cast explicito: "confia en mi", aunque trunque.
    printf("(uint8_t)300     = %u\n", (unsigned)small);

    return 0;
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
```

Salida real:

```
load_user(42)
load_user(42)
flag_state(1)    = enabled
flag_state(1234) = enabled
flag_state(-7)   = enabled
(uint8_t)300     = 44
```

La segunda llamada a `load_user` se acepta porque ambos aliases son `uint32_t`. La función
de flag trata cualquier `int` no cero como true, así que `1234` y `-7` son inputs válidos
salvo que tu API diga lo contrario. El cast a `uint8_t` hace explícita la truncación, así
que el compilador confía en vos y guarda `300 mod 256 == 44`. Nada de esto es un bug del
compilador. Es C haciendo exactamente lo que promete su sistema de tipos, y nada más.

## Modos de falla y trade-offs

- **Mixups de dominio.** `UserId`, `FileDescriptor`, `Milliseconds` y `Bytes` pueden ser
  aliases de `int` o `uint32_t`. El compilador no te frena si los intercambiás salvo que
  uses wrapper types más fuertes.
- **Narrowing silencioso y trampas de signedness.** Convertir un entero ancho a uno chico
  puede perder bits; mezclar signed y unsigned puede convertir valores negativos en
  positivos enormes. Esto conecta directo con la
  [[lowlevel/machine-model/twos-complement-and-integer-representation|representación de enteros]].
- **Ceguera inducida por casts.** Un cast puede ser pegamento necesario en una frontera ABI
  o de serialización, pero también puede ocultar justo el warning que te salvaba. Tratá los
  casts como presión de diseño, no decoración.
- **La validez del puntero queda afuera del tipo.** `T *` no codifica "non-null", "apunta a
  12 elementos", "owned", "borrowed" ni "sigue vivo." La mayoría de los bugs de memoria en C
  son valores cuyo tipo todavía parece correcto.
- **La abstracción cuesta ceremonia manual.** APIs C más fuertes necesitan wrappers con
  `struct`, handles opacos, funciones de creación/destrucción, assertions y tests. Es más
  código que un `int` crudo, pero recupera significado.

## En la práctica

- **Usá `typedef` para legibilidad, no seguridad.** Documenta intención, pero no fuerza un
  tipo nuevo. Para separación real, envolvé el valor en un `struct` o usá un handle opaco.
- **Preferí APIs nombradas y estrechas.** `load_user(UserId)` es mejor que `load(int)`,
  pero `struct UserId { uint32_t value; }` es más fuerte cuando los mixups importan.
- **Hacé ruidosas las conversiones.** Compilá con `-Wall -Wextra -Wconversion
  -Wsign-conversion` en desarrollo, y exigí casts explícitos donde narrowing o cambios de
  signedness sean intencionales.
- **Usá `bool` para booleanos, pero igual validá inputs en fronteras.** `_Bool` normaliza a
  0 o 1 dentro de C, pero protocolos externos, archivos y syscalls todavía necesitan
  chequeos.
- **Mantené los contratos de puntero cerca del puntero.** Pasá longitudes con buffers,
  documentá ownership, usá `const` para préstamos read-only, y mantené lifetimes lo
  bastante chicos como para revisarlos.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/why-c-still-matters|Por qué C todavía importa]] · [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]] · [[lowlevel/machine-model/twos-complement-and-integer-representation|Complemento a dos y representación de enteros]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]

## Fuentes

- **Jens Gustedt — *Modern C*** — tratamiento moderno del sistema de tipos de C, conversiones, object types, `typedef` y cómo diseñar interfaces con menos trampas. https://gustedt.gitlabpages.inria.fr/modern-c/
- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre tipos compatibles, `typedef`, conversiones, qualifiers, effective type y la máquina abstracta. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Type classification in C** — mapa compacto de tipos escalares, agregados, función, incompletos, compatibles y calificados. https://en.cppreference.com/w/c/language/type
- **cppreference — Implicit conversions in C** — las reglas exactas detrás de integer promotions, usual arithmetic conversions, boolean conversion y assignment conversion. https://en.cppreference.com/w/c/language/conversion
- **GCC warning options** — referencia práctica para `-Wall`, `-Wextra`, `-Wconversion` y `-Wsign-conversion`, el loop de feedback del compilador que compensa el tipado débil. https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
- **Robert Seacord — *Effective C*** — guía con foco en seguridad sobre interfaces C, conversiones, lifetimes de objetos y cómo evitar defectos comunes relacionados con tipos. https://nostarch.com/Effective_C
