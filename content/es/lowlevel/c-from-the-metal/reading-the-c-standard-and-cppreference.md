---
title: "Leer el estándar C y cppreference"
description: El estándar C es el contrato; cppreference es un mapa. Aprendé a leer definiciones, constraints, semantics, undefined behavior, implementation-defined behavior, marcadores de versión y extensiones de compilador sin tratar ejemplos como ley.
tags: [c, standard, cppreference, wg14, reference]
order: 13
updated: 2026-06-29
---
# Leer el estándar C y cppreference

El trabajo low-level en C eventualmente llega a la pregunta "qué promete realmente C acá?"
La respuesta vive en el estándar C, pero el estándar es denso, normativo y no está escrito
como tutorial. cppreference es el mapa rápido: searchable, cruzado con links, versionado y
práctico. Usá cppreference para orientarte; usá el estándar cuando importe el contrato
exacto.

> El reset: cppreference te dice dónde está la regla y cómo suele hablar de ella la gente.
> El estándar es la regla. El comportamiento del compilador es evidencia sobre una
> implementación, no sobre el lenguaje.

## Qué estás leyendo

El estándar C lo produce WG14 y lo publica ISO. Los estándares finales oficiales no siempre
son descargables gratis, pero los working drafts de WG14 se usan mucho para estudiar. No
son el mismo artefacto legal que el estándar final, pero están lo bastante cerca para
enseñar la forma del lenguaje: términos, constraints, semantics, contratos de biblioteca,
undefined behavior y elecciones implementation-defined.

cppreference es una referencia secundaria. Es excelente para trabajo diario porque linkea
conceptos, marca cambios C99/C11/C17/C23, resume constraints e incluye ejemplos. Igual
puede tener omisiones o wording menos preciso que el estándar. Cuando el bug o la frontera
de API es high stakes, seguí las referencias hacia texto normativo y documentación del
compilador.

## Cómo funciona realmente

Leé material de referencia C con algunas palabras cargadas:

| Palabra | Significado al leer |
|---|---|
| shall | requisito; violar un constraint con "shall" suele requerir diagnóstico o volver el comportamiento undefined |
| constraint | regla que la implementación debe diagnosticar cuando se viola en una translation unit |
| semantics | qué significa una construcción válida |
| undefined behavior | no hay requisitos después de que el programa viola la regla |
| unspecified behavior | la implementación puede elegir entre posibilidades válidas sin documentar cuál |
| implementation-defined behavior | la implementación debe elegir y documentar su elección |
| footnote | contexto útil, normalmente no normativo |
| example | ilustración, no toda la regla |

En cppreference, primero chequeá en qué tipo de página estás: lenguaje, biblioteca, header
o soporte de compilador. Después chequeá marcadores de versión. Una regla que dice "since
C23" no es regla para un build `-std=c17`. Leé las secciones "Notes" y "Defect reports",
pero no te quedes ahí si estás diseñando una frontera de portabilidad.

Las extensiones de compilador son otra capa. GCC y Clang soportan extensiones útiles como
statement expressions, attributes, builtins y warnings extra. No son C portable solo porque
tu compilador las acepte. Decidí deliberadamente si un archivo es ISO C, GNU C, C
específico de Clang o C freestanding específico del target.

## Artefacto ejecutable: preguntale a la implementación

El demo vive en `examples/c-from-the-metal/reading-the-c-standard-and-cppreference/demo.c`.
Imprime hechos que el estándar describe pero la implementación elige o instancia.

```c
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

int main(void) {
#ifdef __STDC_VERSION__
    printf("__STDC_VERSION__      = %ld\n", (long)__STDC_VERSION__);
#else
    printf("__STDC_VERSION__      = pre-C90\n");
#endif

    printf("CHAR_BIT              = %d\n", CHAR_BIT);
    printf("INT_MAX               = %d\n", INT_MAX);
    printf("sizeof(size_t)        = %zu bytes\n", sizeof(size_t));

    /* El estandar deja la signedness de char plano a la implementacion. */
    if ((char)0xFF < 0) {
        printf("plain char signed     = yes\n");
    } else {
        printf("plain char signed     = no\n");
    }

    return 0;
}
```

Compilá y corré como C17:

```bash
gcc -std=c17 -O0 -Wall -Wextra -pedantic demo.c -o demo
./demo
```

Salida real:

```
__STDC_VERSION__      = 201710
CHAR_BIT              = 8
INT_MAX               = 2147483647
sizeof(size_t)        = 8 bytes
plain char signed     = yes
```

El estándar te dice que `CHAR_BIT` existe y es al menos 8, no que cada target debe verse
como este. Te dice que `INT_MAX` viene de `<limits.h>`, no que cada ABI tenga el mismo
`int`. Te dice que la signedness de `char` plano es implementation-defined, así que la
última línea es algo para preguntarle a tu compilador/target, no algo para asumir
globalmente.

## Modos de falla y trade-offs

- **Leer ejemplos como reglas exhaustivas.** Los ejemplos enseñan forma; constraints y
  semantics definen el contrato.
- **Ignorar marcadores de versión.** C90, C99, C11, C17 y C23 difieren. Tu modo de
  compilador decide qué dialecto pediste.
- **Confundir undefined e implementation-defined.** Implementation-defined behavior es una
  elección documentada. Undefined behavior es ausencia de contrato.
- **Asumir que cppreference es normativo.** Es útil y normalmente excelente, pero no es el
  estándar.
- **Olvidar la implementación.** ABI, flags de compilador, arquitectura target, libc y OS
  instancian elecciones que el estándar deja abiertas.

## En la práctica

- **Empezá con cppreference, terminá con el contrato.** Usalo para encontrar el concepto,
  después verificá bordes exactos en texto WG14 o docs de la implementación.
- **Mantené una lista local de vocabulario.** "Object", "lvalue", "effective type",
  "constraint", "storage duration" y "linkage" tienen significados técnicos en C.
- **Chequeá el dialecto seleccionado.** `-std=c17` y `-std=gnu17` no prometen lo mismo.
- **Preferí consultas de implementación para elecciones de implementación.** Usá
  `<limits.h>`, `<stdint.h>`, `_Alignof`, `sizeof`, macros de compilador y programas chicos
  para inspeccionar el target.
- **Marcá claims no verificables.** Si no podés atar una regla al estándar, cppreference o
  documentación del compilador, tratala como hipótesis.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: el contrato]] · [[lowlevel/c-from-the-metal/integer-promotions-and-implicit-conversions|Integer promotions y conversiones implícitas]] · [[lowlevel/c-from-the-metal/build-and-run-a-c-program-from-zero|Build y ejecución de un programa C desde cero]] · [[lowlevel/reference-registry|Registro de Referencias]]

## Fuentes

- **WG14 documents** — índice oficial de documentos del working group para drafts de C, papers, defect reports y material de reuniones. https://www.open-std.org/jtc1/sc22/wg14/www/docs/
- **ISO/IEC 9899:2011 draft N1570** — draft C11 disponible libremente, útil para aprender la estructura de clauses, constraints, semantics y wording de biblioteca. https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- **cppreference — C reference** — mapa searchable de páginas de lenguaje y biblioteca C con marcadores de versión, ejemplos, notes y defect reports. https://en.cppreference.com/w/c
- **cppreference — Undefined behavior** — catálogo práctico de undefined, unspecified e implementation-defined behavior con ejemplos. https://en.cppreference.com/w/c/language/behavior
- **GCC manual — Standards** — cómo GCC selecciona dialectos C, extensiones GNU y modos de conformidad. https://gcc.gnu.org/onlinedocs/gcc/Standards.html
- **Clang language compatibility** — notas de compatibilidad de Clang y contexto de extensiones/conformidad para lenguajes de la familia C. https://clang.llvm.org/compatibility.html
