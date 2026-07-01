---
title: "Símbolos: definición, referencia y resolución"
description: Los linkers trabajan sobre símbolos: las definiciones proveen código o storage, las referencias los piden, y la resolución decide si el programa final es coherente.
tags: [toolchain, linker, symbols, linkage, object-files]
order: 4
updated: 2026-07-01
---
# Símbolos: definición, referencia y resolución

Un linker no entiende tu programa como módulos C, clases o diagramas de arquitectura. Ve
**símbolos**. Un símbolo es una cosa nombrada en un object file: una función, objeto global,
helper local, marcador de section o nombre externo unresolved. Linkear es matchear
referencias con definiciones, elegir direcciones y reportar los casos donde el grafo no
tiene sentido. Una vez que podés leer símbolos, "undefined reference" se vuelve un hecho
preciso en vez de un clima.

> El reset: una declaración alcanza para el compilador; una definición es lo que el linker
> puede bindear. La resolución de símbolos es donde esa distinción se vuelve mecánica.

## Cómo funciona realmente

Cada object relocatable aporta una tabla de símbolos. Algunas entradas son
**definiciones**: "este object file provee código o storage para este nombre." Algunas
entradas son **referencias**: "este object file necesita una definición de este nombre desde
otro lado." El linker construye una vista combinada e intenta resolver cada referencia
externa que debe resolverse en link time.

| Idea de símbolo | Forma en `nm` | Significado |
|---|---|---|
| Función definida | `T name` | definición global de text/código |
| Función local | `t name` | definición de text/código privada de este object |
| Objeto inicializado | `D name` | definición global de datos writables |
| Objeto zero/common | `B` o `C name` | storage zero-filled o common |
| Referencia undefined | `U name` | este object necesita una definición afuera |
| Símbolo weak | `W` / `V` / variantes minúsculas | definición o referencia con reglas de binding más débiles |

Las letras exactas varían por formato de object y herramienta, pero las categorías son
estables: definido acá, local acá, faltante acá, weakly bound o atado a una section. ELF
también registra binding (`LOCAL`, `GLOBAL`, `WEAK`), tipo (`FUNC`, `OBJECT`, `SECTION`,
`FILE`), visibility, size, value y section index.

Para C ordinario, la regla default es simple: funciones y objetos a file scope no `static`
se vuelven definiciones externas; nombres `static` a file scope se vuelven símbolos locales;
declaraciones como `extern int f(void);` dejan que el compilador type-checkee un uso pero
no crean una definición. Esta es exactamente la separación del lado de C de
[[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declaraciones vs definiciones y linkage]].

## Reglas de resolución

El camino común del linker es una acumulación left-to-right de object files y bibliotecas.
Para object files simples, lee definiciones y referencias unresolved, y después matchea
nombres. Los static archives agregan una regla de extracción sensible al orden, que tiene
su propia nota más adelante; el modelo de símbolos por debajo es el mismo.

Las definiciones strong son las definiciones normales que escribís en C: cuerpos de función
y objetos globales inicializados. Una referencia externa requerida debe resolverse a una
definición compatible al final del link. Si no existe definición, obtenés un error de
undefined reference. Si aparecen varias definiciones strong del mismo nombre externo,
obtenés un error de multiple definition.

Los símbolos weak son una excepción controlada. Una definición weak puede ser reemplazada
por una definición strong con el mismo nombre, y una referencia weak unresolved puede quedar
en cero/null según plataforma y relocation. Los weak symbols sirven para hooks de runtime,
features opcionales y mecanismos de bibliotecas de bajo nivel, pero no reemplazan ownership
claro de definiciones.

Los símbolos locales no participan en la resolución global. Un `static int hidden_bias(void)`
en un object file puede tener el mismo nombre local que un helper en otro object file porque
el linker no exporta esos nombres como candidatos globales.

## Artefacto ejecutable: mirá cómo `U` queda resuelto

El ejemplo vive en
`examples/toolchain-and-linking/symbols-definition-reference-and-resolution/`. `main.c`
incluye declaraciones y llama a dos funciones, pero sus definiciones viven en `mathlib.c`:

```c
#include <stdio.h>

#include "api.h"

int main(void) {
    int sum = public_add(20, 1);
    printf("sum=%d twice=%d\n", sum, public_twice(sum));
    return 0;
}
```

`mathlib.c` exporta dos símbolos y mantiene un helper local:

```c
#include "api.h"

static int hidden_bias(void) {
    return 1;
}

int public_add(int left, int right) {
    return left + right + hidden_bias();
}

int public_twice(int value) {
    return value * 2;
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/symbols-definition-reference-and-resolution
./run.sh
```

Salida real de esta máquina:

```text
== compile objects ==
== nm main.o: references waiting for definitions ==
0000000000000000 T _main
                 U _printf
                 U _public_add
                 U _public_twice
== nm mathlib.o: exported and internal definitions ==
0000000000000030 t _hidden_bias
0000000000000000 T _public_add
0000000000000040 T _public_twice
== link resolves the U symbols ==
sum=22 twice=44
```

Antes del link, `main.o` tiene `U _public_add` y `U _public_twice`: el compilador aceptó
las declaraciones, pero el object file todavía necesita definiciones. `mathlib.o` provee
`T _public_add` y `T _public_twice`, así que el linker puede bindear las referencias. El
helper es `t _hidden_bias`, text local en minúscula, porque `static` a file scope le dio
internal linkage. `_printf` se resuelve desde libc mediante el link hosted normal de C.

En Linux/ELF normalmente ves nombres sin guiones bajos iniciales. En C++, los nombres pueden
estar mangled para codificar namespaces, overloads y tipos de parámetros; `c++filt` los
demanglea. El linker sigue resolviendo nombres de símbolos, pero C++ hace que esos nombres
sean menos humanos.

## Modos de falla y trade-offs

- **Undefined reference.** Algún object refiere a un símbolo global, pero los inputs del
  link nunca proveen una definición requerida. Causas comunes: falta un `.o`, falta un flag
  de biblioteca, orden incorrecto de biblioteca para static archives, typo o conditional
  compilation no alineada.
- **Multiple definition.** Se linkeó más de una definición externa strong del mismo símbolo.
  El bug clásico en C es definir un objeto global en un header en vez de declararlo con
  `extern` y definirlo una vez en un `.c`.
- **Oculto por `static`.** Un helper `static` a file scope es local intencionalmente. Eso es
  bueno para encapsulación, pero otro object file no puede linkear contra él.
- **Mismatch de ABI o lenguaje.** Name mangling de C++, diferencias de calling convention o
  falta de `extern "C"` pueden producir un símbolo que parece ausente aunque exista una
  función parecida.
- **Los weak symbols pueden ocultar bugs de ownership.** Sirven para extension points de
  bajo nivel, pero también pueden hacer que el link tenga éxito cuando esperabas un error
  duro de definición faltante.
- **Los dynamic symbols son una etapa posterior.** Shared libraries agregan dynamic symbol
  tables exportadas, interposition, versioning y lookup en load time. No debuggees eso solo
  con el modelo de static link.

## En la práctica

- **Leé la salida de `nm` como checklist.** Por cada `U name`, encontrá exactamente un
  proveedor intencional entre tus objects o bibliotecas.
- **Preferí un owner por símbolo externo.** Los headers declaran la superficie pública; un
  source file es dueño de cada definición externa.
- **Dejá helpers privados como `static` por default.** Si ninguna otra translation unit
  debería llamar a un helper, mantenelo fuera del namespace global de símbolos.
- **Cuando linkees C++ desde C, controlá el nombre.** Usá `extern "C"` en el borde para que
  el lado C busque el símbolo que el lado C++ realmente exporta.
- **Separá resolución de símbolos de relocation.** La resolución elige qué definición
  significa una referencia. La relocation parchea los bytes después del layout. Cooperan,
  pero no son el mismo paso.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/toolchain-and-linking/object-files-and-whats-inside-them|Object files y qué tienen adentro]] · [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|El formato ELF]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declaraciones vs definiciones y linkage]] · [[lowlevel/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link|El pipeline: preprocess → compile → assemble → link]]

## Fuentes

- **GNU binutils — manual de `nm`** — letras de símbolos, símbolos undefined, weak symbols y display de símbolos locales/globales. https://sourceware.org/binutils/docs/binutils/nm.html
- **System V ABI — especificación ELF** — entradas de tabla de símbolos, binding, tipo, visibility, section index y registros de relocation. https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html
- **David Drysdale — *Beginner's Guide to Linkers*** — recorrido concreto de símbolos, referencias unresolved y errores de linker. https://www.lurklurk.org/linkers/linkers.html
- **Ian Lance Taylor — serie *Linkers*** — vista de implementación de linker sobre tablas de símbolos y resolución. https://www.airs.com/blog/archives/38
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 7 — símbolos strong/weak, resolución de símbolos y relocation. https://csapp.cs.cmu.edu/
- **cppreference — Declarations and definitions in C** — distinción del lado del lenguaje que alimenta el modelo de símbolos del linker. https://en.cppreference.com/w/c/language/declarations
