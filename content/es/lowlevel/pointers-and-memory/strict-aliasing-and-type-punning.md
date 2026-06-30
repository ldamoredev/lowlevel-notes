---
title: "Strict aliasing y type punning"
description: Strict aliasing es el permiso que tiene el optimizador para asumir que punteros de tipos no relacionados no nombran el mismo objeto. Type punning se rompe cuando reinterpretás storage con el lvalue equivocado en vez de copiar bytes deliberadamente.
tags: [strict-aliasing, type-punning, effective-type, undefined-behavior, memcpy]
order: 11
updated: 2026-06-30
---
# Strict aliasing y type punning

El compilador de C no solo traduce tus loads y stores. Razona sobre qué loads y stores
pueden referirse al mismo objeto. Strict aliasing es la regla que le permite asumir que
punteros a tipos no relacionados normalmente no aliasan, y type punning es mirar los bytes
de un objeto como si fueran otro tipo. El modelo seguro: los valores tienen tipos, los
objetos tienen lifetimes y bytes de representación, y `memcpy` es el puente aburrido y
portable entre valores tipados y bytes crudos.

> El reset: un `void*` o un cast no borra las reglas del objeto que hay debajo. Solo cambia
> el tipo de la expresión que estás por usar.

## Cómo funciona realmente

Todo objeto C tiene una **object representation**: los bytes que guardan su valor. Los
tipos de carácter pueden inspeccionar esos bytes. Por eso `unsigned char *` es la
herramienta correcta para dumps de bytes, primitivas de serialización, checksums y copias
de payloads opacos.

La regla más estricta trata de acceder un objeto a través de un lvalue. El estándar de C
dice que el valor guardado de un objeto solo puede accederse mediante ciertos tipos
compatibles, versiones calificadas de esos tipos, tipos aggregate/union adecuados o un
tipo de carácter. Si escribís un objeto `float` y después leés el mismo storage mediante
un `int *`, el compilador puede tratar eso como imposible en C estrictamente conforme. En
niveles de optimización donde strict aliasing está activo, esa suposición puede mover,
combinar o eliminar operaciones de memoria de maneras que parecen "miscompilación." El
bug viene antes: el programa le dio al optimizador un contrato falso.

Esto importa incluso cuando las direcciones son numéricamente iguales:

```c
float f = 1.0f;
int *p = (int *)&f;
printf("%x\n", *p);        /* no es C portable */
```

El cast produce un `int *`, pero no crea un objeto `int` en esa dirección. Tampoco promete
que el patrón de bits de un `float` sea un objeto `int` válido leído mediante un lvalue
`int`. En una máquina común con IEEE-754 quizá veas los bits esperados en debug. Esa no
es la regla que el optimizador tiene que preservar.

`memcpy` es distinto. Copia bytes desde un objeto real hacia otro objeto real:

```c
float f = 1.0f;
uint32_t bits;
memcpy(&bits, &f, sizeof bits);
```

Ahora `bits` es un objeto `uint32_t` real, y leer `bits` usa el tipo de lvalue correcto.
Los bytes vinieron de la representación del objeto `float`, pero el acceso ya no es un
acceso tipado incompatible al objeto `float`. Los compiladores entienden este patrón y
normalmente optimizan la copia.

El union punning es la zona gris a la que mucha gente recurre en C. Es común en sistemas,
y C tiene texto específico para unions, pero igual expone detalles de representación y
puede producir resultados trap o dependientes de implementación para algunos tipos. Usá
una union cuando la union sea el modelo de datos. Usá `memcpy` cuando la operación sea
"copiar estos bytes a un objeto de otro tipo."

## Artefacto ejecutable: copiar bytes, no aliasar por cast

El demo vive en `examples/pointers-and-memory/strict-aliasing-and-type-punning/demo.c`.

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Salida real:

```
float 1.0 bits        = 0x3f800000
restored float        = 1.0
object bytes          = 22 11 44 33
pair copied as u32    = 0x33441122
```

Los bits del `float` se copian con `memcpy`, y después se copian de vuelta a un objeto
`float` real. El dump del struct usa `unsigned char`, que puede inspeccionar la object
representation. El `uint32_t` final también se crea por copia de bytes. En esta máquina
little-endian, los bytes se imprimen primero con el byte menos significativo; ese byte
order es una observación, no un formato portable de serialización.

## Modos de falla y trade-offs

- **Castear a través de otro tipo de objeto.** Los casts a `T *` pueden callar diagnósticos
  mientras mantienen el mismo undefined behavior.
- **Asumir que `void*` es magia.** `void*` transporta una dirección sin tipo. El acceso
  después del cast todavía debe respetar el tipo y lifetime reales del objeto.
- **Empaquetar protocolos con casts a structs.** Network packets, bloques de disco y MMIO
  suelen necesitar parsing de bytes, conversión de endian y manejo de alineación, no un
  cast a puntero de struct C.
- **Depender del comportamiento en debug.** `-O0` suele esconder bugs de strict aliasing
  porque el optimizador todavía no explota sus suposiciones.
- **Desactivar strict aliasing globalmente.** `-fno-strict-aliasing` puede ser una llave
  práctica para migrar código viejo, pero también le quita margen de optimización a código
  correcto.
- **Olvidar alineación.** Incluso si aliasing estuviera permitido, dereferenciar un puntero
  mal alineado puede ser undefined o lento en máquinas reales.

## En la práctica

- **Usá `memcpy` para reinterpretar bits.** Dice exactamente lo que querés y optimiza bien
  en compiladores modernos.
- **Usá `unsigned char *` para inspección de bytes.** No generalices esa excepción a tipos
  de puntero arbitrarios.
- **Mantené juntos ownership del storage y vistas tipadas.** Si una API entrega bytes
  crudos, documentá qué función crea el objeto tipado y cuándo empieza su lifetime.
- **Serializá explícitamente.** Para archivos y redes, escribí funciones encode/decode con
  endian explícito en vez de dumpear structs.
- **Tratate los warnings de strict aliasing como humo, no cobertura.** El compilador no
  puede probar todos los alias malos. La revisión de código todavía tiene que conocer la
  regla.
- **Usá `-fno-strict-aliasing` solo como decisión de frontera.** Puede ser razonable para
  una biblioteca legacy; no debería reemplazar entender acceso a objetos.

**Conecta con:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero]] · [[lowlevel/pointers-and-memory/void-star-and-type-erasure|`void*` y type erasure]] · [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alineación y padding]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Aritmética de punteros y stride]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **ISO WG14 N1570 — draft de C11** — object representation, effective type, aliasing y reglas de acceso por bytes en texto de estándar. https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- **cppreference — Object model** — referencia compacta para effective type, strict aliasing, alineación y object representation. https://en.cppreference.com/w/c/language/object
- **cppreference — `memcpy`** — documenta la copia de bytes como primitiva portable para mover object representations. https://en.cppreference.com/w/c/string/byte/memcpy
- **Manual de GCC — Optimize Options** — explica `-fstrict-aliasing` y la suposición del optimizador expuesta por el flag. https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
- **Jens Gustedt — *Modern C*** — tratamiento práctico de C sobre object representation, aliasing e idioms portables de bajo nivel. https://gustedt.gitlabpages.inria.fr/modern-c/
