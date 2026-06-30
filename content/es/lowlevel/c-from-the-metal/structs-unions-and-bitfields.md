---
title: "Structs, unions y bitfields"
description: Los structs le ponen nombres a bytes con layout, las unions reutilizan el mismo storage para una interpretación activa, y los bitfields empaquetan campos enteros chicos con layout implementation-defined. Las partes útiles, las trampas de padding y qué no serializar.
tags: [c, structs, unions, bitfields, layout, padding]
order: 8
updated: 2026-06-29
---
# Structs, unions y bitfields

Los agregados de C son donde el sistema de tipos se encuentra con el layout de memoria. Un
`struct` dispone miembros nombrados en orden, con padding insertado para que cada miembro
quede correctamente alineado. Una `union` superpone varios tipos de miembro sobre el mismo
storage, así que se supone que una sola interpretación está activa a la vez. Los bitfields
te dejan nombrar campos enteros chicos dentro de una unidad de storage, pero su layout
exacto es deliberadamente implementation-defined (definido por la implementación). Estas
herramientas son poderosas porque están cerca de los bytes; son peligrosas por la misma
razón.

> El reset: `struct` no es un objeto JSON, `union` no es un sum type seguro, y los bitfields
> no son un formato portable de wire. Son la forma de C de describir objetos que ocupan
> bytes.

## Los structs son layout con padding

Un `struct` guarda sus miembros en orden de declaración. El compilador puede insertar bytes
sin nombre entre miembros y después del último miembro para que cada objeto obedezca las
reglas de alineación del target. Ese padding es parte de la representación del objeto, pero
no es un campo que puedas leer como valor.

```c
struct PacketHeader {
    uint8_t tag;
    uint32_t length;
    uint16_t flags;
};
```

En un target típico de 64 bits, `tag` empieza en offset 0, después aparecen tres bytes de
padding para que `length` pueda empezar en offset 4. `flags` empieza en offset 8, y el tail
padding redondea el objeto completo a la alineación del struct. Reordenar miembros por
alineación decreciente puede reducir padding:

```c
struct CompactHeader {
    uint32_t length;
    uint16_t flags;
    uint8_t tag;
};
```

Eso no es un retoque estético; cambia `sizeof`, el stride de arrays, la huella de cache y
expectativas de ABI. Usá `sizeof`, `_Alignof` y `offsetof` para preguntarle al compilador
qué eligió realmente. No cuentes bytes a ojo cuando puede existir padding.

## Cómo funciona realmente

Las definiciones de `struct` crean tipos agregados. La asignación copia el objeto completo,
incluidos bytes de padding como representación; pasar un struct por valor lo copia según la
ABI; los arrays de structs usan `sizeof(struct T)` como stride de un elemento al siguiente.
Los designated initializers hacen explícito el mapeo:

```c
struct PacketHeader header = {
    .tag = 7,
    .length = 1024,
    .flags = 3,
};
```

Una `union` asigna storage suficiente para su miembro más grande, con alineación suficiente
para el miembro más estricto. Todos los miembros empiezan en offset 0. Eso hace útil a una
union para datos etiquetados: mantené un tag separado que diga qué miembro tiene sentido
ahora.

```c
union Payload {
    uint32_t as_u32;
    double as_double;
};

struct Value {
    enum ValueKind kind;
    union Payload payload;
};
```

Sin el tag, una union es solo storage compartido. Leer un miembro distinto del escrito más
recientemente no es una forma portable de deserializar bytes ni de saltear el sistema de
tipos; depende de detalles de representación y puede chocar con trap values, effective type
y reglas de strict aliasing. Si necesitás bytes, copiá bytes con `memcpy`. Si necesitás un
sum type, llevá el tag.

Los bitfields adjuntan anchos a miembros enteros:

```c
struct StatusBits {
    unsigned ready : 1;
    unsigned error : 1;
    unsigned mode : 3;
    unsigned reserved : 3;
};
```

Sirven para flags compactos en memoria y código con forma de registro cuando también
controlás el compilador y el target. Pero el estándar deja elecciones importantes a la
implementación: orden de asignación dentro de la unidad de storage, si los campos cruzan
unidades de asignación, la signedness de bitfields `int` planos y el padding. No podés
tomar la dirección de un bitfield, y `offsetof` no aplica a uno.

## Artefacto ejecutable: preguntale al compilador

El demo vive en `examples/c-from-the-metal/structs-unions-and-bitfields/demo.c`. Le pregunta
al compilador offsets y tamaños en vez de asumirlos.

```c
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct PacketHeader {
    uint8_t tag;
    uint32_t length;
    uint16_t flags;
};

struct CompactHeader {
    uint32_t length;
    uint16_t flags;
    uint8_t tag;
};

union Payload {
    uint32_t as_u32;
    double as_double;
};

enum ValueKind {
    VALUE_U32,
    VALUE_DOUBLE,
};

struct Value {
    enum ValueKind kind;
    union Payload payload;
};

struct StatusBits {
    unsigned ready : 1;
    unsigned error : 1;
    unsigned mode : 3;
    unsigned reserved : 3;
};

static void print_packet_layout(void) {
    printf("PacketHeader size     = %zu bytes\n", sizeof(struct PacketHeader));
    printf("PacketHeader align    = %zu bytes\n", _Alignof(struct PacketHeader));
    printf("  tag offset          = %zu\n", offsetof(struct PacketHeader, tag));
    printf("  length offset       = %zu\n", offsetof(struct PacketHeader, length));
    printf("  flags offset        = %zu\n", offsetof(struct PacketHeader, flags));
    printf("CompactHeader size    = %zu bytes\n", sizeof(struct CompactHeader));
}

static void print_value(struct Value value) {
    if (value.kind == VALUE_U32) {
        printf("tagged union value    = u32:%u\n", value.payload.as_u32);
        return;
    }

    printf("tagged union value    = double:%.2f\n", value.payload.as_double);
}

int main(void) {
    struct PacketHeader header = {
        .tag = 7,
        .length = 1024,
        .flags = 3,
    };
    struct Value id = {
        .kind = VALUE_U32,
        .payload.as_u32 = 42,
    };
    struct Value ratio = {
        .kind = VALUE_DOUBLE,
        .payload.as_double = 0.75,
    };
    struct StatusBits status = {
        .ready = 1,
        .error = 0,
        .mode = 5,
        .reserved = 0,
    };

    /* El orden de los campos cambia el padding que agrega el compilador. */
    print_packet_layout();

    printf("header values         = tag:%u length:%u flags:%u\n",
           (unsigned)header.tag, header.length, (unsigned)header.flags);
    printf("Payload union size    = %zu bytes\n", sizeof(union Payload));
    printf("Value struct size     = %zu bytes\n", sizeof(struct Value));

    /* El tag externo dice que miembro de la union esta activo. */
    print_value(id);
    print_value(ratio);

    /* Los bitfields sirven para flags en memoria, no para formatos portables. */
    printf("StatusBits size       = %zu bytes\n", sizeof(struct StatusBits));
    printf("status bits           = ready:%u error:%u mode:%u\n",
           status.ready, status.error, status.mode);

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
PacketHeader size     = 12 bytes
PacketHeader align    = 4 bytes
  tag offset          = 0
  length offset       = 4
  flags offset        = 8
CompactHeader size    = 8 bytes
header values         = tag:7 length:1024 flags:3
Payload union size    = 8 bytes
Value struct size     = 16 bytes
tagged union value    = u32:42
tagged union value    = double:0.75
StatusBits size       = 4 bytes
status bits           = ready:1 error:0 mode:5
```

El primer layout desperdicia espacio porque `length` necesita alineación de 4 bytes después
de un campo de 1 byte. El orden compacto mete los mismos valores en 8 bytes en este target.
La union mide 8 bytes porque `double` es el miembro más grande y más estricto. `struct
Value` es más grande que la union porque el tag y el padding de alineación también ocupan
espacio. Que `StatusBits` ocupe 4 bytes es una elección del compilador, no una promesa que
deberías poner en disco.

## Modos de falla y trade-offs

- **Asumir que no hay padding.** `sizeof(struct T)` puede ser más grande que la suma de sus
  miembros. Arrays, I/O binario, networking, hashing y `memcmp` se preocupan por eso.
- **Serializar structs crudos.** Padding bytes, endianness, alineación y ABI del compilador
  hacen que `fwrite(&header, sizeof header, 1, file)` sea una trampa de portabilidad.
  Serializá campos deliberadamente.
- **Usar unions como variantes sin tag.** Una union no recuerda qué miembro está activo.
  Guardá un tag al lado, o tu lector está adivinando.
- **Type punning mediante unions.** Algunos compiladores soportan idioms comunes de union
  punning, pero C portable debería preferir `memcpy` para representaciones de objeto y
  conversión explícita para valores.
- **Dependencia del layout de bitfields.** El orden de bits y la asignación no son
  portables. Para archivos, paquetes y ABIs entre compiladores, usá masks y shifts sobre
  enteros de ancho fijo.
- **Los pragmas de packing cortan para ambos lados.** Extensiones de compilador pueden
  quitar padding, pero pueden crear loads desalineados, código más lento o mismatch de ABI.
  Usalas en fronteras, no como estilo default.

## En la práctica

- **Usá `offsetof` y `sizeof` como instrumentación.** Cuando el layout importa, escribí un
  check chico o `static_assert` en vez de confiar en intuición.
- **Ordená structs hot con intención.** Agrupá campos por alineación y patrón de acceso,
  pero no reordenes structs de ABI pública casualmente.
- **Preferí tagged unions para variantes.** `enum kind` más `union payload` es el patrón en
  C; cada función debería switchear por el tag antes de leer el payload.
- **Usá enteros de ancho fijo para formatos binarios.** Después codificá endianness y layout
  de bits explícitamente con masks, shifts y escrituras de bytes.
- **Mantené los bitfields locales.** Están bien para flags privados en un compilador/target.
  Son sospechosos en headers públicos, storage persistente y definiciones de protocolo.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|El sistema de tipos de C es débil]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: el contrato]] · [[lowlevel/c-from-the-metal/arrays-and-array-to-pointer-decay|Arrays y array-to-pointer decay]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words y direcciones]] · [[lowlevel/machine-model/endianness|Endianness]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad sobre structures, unions, bitfields, alineación, padding, acceso a miembros y representaciones de objeto. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Struct and union initialization** — referencia práctica para inicialización aggregate y designated de structs y unions. https://en.cppreference.com/c/language/struct_initialization
- **cppreference — Struct declaration** — reglas de layout, sintaxis de bitfields, miembros anónimos y restricciones como no tomar la dirección de un bitfield. https://en.cppreference.com/c/language/struct
- **cppreference — Object model and alignment** — representación de objetos, padding, alineación, trap representations y contexto de effective type. https://en.cppreference.com/c/language/object
- **System V AMD64 ABI** — cómo el layout de agregados y las calling conventions se vuelven ABI en sistemas Unix-like x86-64. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Jens Gustedt — *Modern C*** — guía moderna sobre aggregate types, initializers, representación de objetos y disciplina de layout portable. https://gustedt.gitlabpages.inria.fr/modern-c/
