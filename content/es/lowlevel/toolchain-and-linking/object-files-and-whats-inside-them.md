---
title: "Object files y qué tienen adentro"
description: Un object file relocatable es la entrada del linker: machine code, datos, tablas de símbolos, registros de relocation y metadata que son reales pero todavía no forman un programa ejecutable.
tags: [toolchain, object-files, symbols, relocations, linker]
order: 2
updated: 2026-07-01
---
# Object files y qué tienen adentro

Un archivo `.o` no es "medio ejecutable." Es un **object file relocatable** (archivo objeto
reubicable): un contenedor con machine code, datos, tablas de símbolos, registros de
relocation y metadata que el linker puede combinar con otros objects. La nota anterior
mostró dónde aparece `.o` en el pipeline; esta abre la caja. El reset clave es que un object
file puede contener instrucciones reales y aun así contener huecos, porque algunas
direcciones y definiciones se dejan intencionalmente para el linker.

> El reset: compilar con `-c` produce un artefacto linkeable, no un artefacto ejecutable. La
> CPU no puede ejecutar un `.o` cualquiera directamente; el linker todavía tiene que resolver
> símbolos, aplicar relocations y elegir un layout final.

## Cómo funciona realmente

Un object file está organizado en regiones nombradas más tablas que describen esas regiones.
El formato exacto depende de la plataforma: Linux usa ELF, macOS usa Mach-O, Windows usa
COFF/PE. El modelo es suficientemente compartido como para que `nm`, `objdump`, `readelf` y
`otool` respondan las mismas preguntas:

| Parte | Qué contiene | Por qué le importa al linker |
|---|---|---|
| Sección de text/código | instrucciones de machine code codificadas | bytes a ubicar como código ejecutable |
| Sección de data | globals writables inicializados | bytes a ubicar en memoria writable |
| Sección tipo BSS | globals zero-initialized | tamaño a reservar sin guardar ceros en el archivo |
| Constantes read-only | string literals, objetos `const`, jump tables | bytes que no deberían ser writables en runtime |
| Tabla de símbolos | nombres definidos acá y nombres referenciados afuera | la tabla global de matching del linker |
| Registros de relocation | lugares cuyo valor final todavía no se conoce | parches que el linker debe aplicar después del layout |
| Metadata | debug info, unwind info, comentarios, build attributes | herramientas, debuggers, stack unwinding, diagnósticos |

El assembler emite direcciones relativas a secciones porque la dirección final todavía no se
conoce. Si `main.o` contiene una llamada a `scale`, los bytes de la instrucción no pueden
contener la dirección final de `scale`; esa dirección depende de dónde caiga `support.o` en
la salida final. Entonces el object file registra una relocation: "en este offset de esta
sección, escribí la dirección o desplazamiento final de este símbolo usando este tipo de
relocation."

Esa es la diferencia central entre **bytes** y **significado**. La sección `.text` contiene
bytes que la CPU eventualmente puede ejecutar. La tabla de símbolos dice qué nombres
definen esos bytes. La tabla de relocations dice qué bytes son placeholders. El linker
necesita las tres cosas: contenido crudo, nombres e instrucciones de parcheo.

## Sections, símbolos, relocations

Pensá un object relocatable como tres vistas conectadas:

| Vista | Pregunta que responde | Herramientas |
|---|---|---|
| Sections | "¿Qué buckets de bytes hay en este archivo?" | `objdump -h`, `readelf -S`, `otool -l` |
| Símbolos | "¿Qué nombres están definidos acá o todavía faltan?" | `nm`, `objdump -t`, `readelf -s` |
| Relocations | "¿Qué offsets debe parchear el linker?" | `objdump -r`, `readelf -r`, `otool -r` |

Las sections no son lo mismo que los conceptos del lenguaje fuente. Un solo archivo C puede
producir varias sections: código en `__text`/`.text`, globals inicializados en
`__data`/`.data`, storage zero-initialized en áreas common/tipo BSS, string literals en
`__cstring` o `.rodata`, unwind metadata en `__eh_frame`, y debug sections cuando compilás
con `-g`. Los nombres varían por formato de object; el trabajo es el mismo.

Los símbolos son nombres atados a secciones o nombres unresolved que apuntan fuera del
archivo. Con `nm`, las letras mayúsculas normalmente significan símbolos externos/globales,
las minúsculas significan símbolos locales, y `U` significa undefined. En el ejemplo de
abajo, `_compute` y `_global_counter` los provee `main.o`; `_scale` y `_printf` son huecos
que el paso de link debe llenar.

Las relocations son la razón por la que los object files se pueden mover. Una función puede
compilarse antes de que el linker sepa dónde van a vivir la función, un global o un string
literal. La tabla de relocation guarda suficiente información para que el linker parchee
calls, loads y direcciones después de elegir el layout final.

## Artefacto ejecutable: inspeccioná un object relocatable

El ejemplo vive en
`examples/toolchain-and-linking/object-files-and-whats-inside-them/`. Compila dos archivos
C en dos object files, abre `main.o`, y después linkea y corre el programa final.

`main.c` contiene deliberadamente código, datos inicializados, datos zero-initialized, una
constante de string, un helper privado y una llamada externa:

```c
#include <stdio.h>

extern int scale(int value);

int global_counter = 7;
int zero_counter;
const char banner[] = "object";

static int local_bias(void) {
    return 3;
}

int compute(void) {
    zero_counter = scale(global_counter + local_bias());
    return zero_counter;
}

int main(void) {
    printf("%s result: %d\n", banner, compute());
    return 0;
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/object-files-and-whats-inside-them
./run.sh
```

Salida real de esta máquina:

```text
== compile to relocatable object files ==
== file main.o ==
main.o: Mach-O 64-bit object x86_64
== sections in main.o ==

main.o:	file format mach-o 64-bit x86-64

Sections:
Idx Name             Size     VMA              Type
  0 __text           00000083 0000000000000000 TEXT
  1 __data           00000004 0000000000000084 DATA
  2 __const          00000007 0000000000000088 DATA
  3 __cstring        0000000f 000000000000008f DATA
  4 __compact_unwind 00000060 00000000000000a0 DATA
  5 __eh_frame       00000090 0000000000000100 DATA
== symbols in main.o ==
0000000000000088 S _banner
0000000000000000 T _compute
0000000000000084 D _global_counter
0000000000000040 t _local_bias
0000000000000050 T _main
                 U _printf
                 U _scale
0000000000000004 C _zero_counter
== relocations in main.o ==

main.o:	file format mach-o 64-bit x86-64

RELOCATION RECORDS FOR [__text]:
OFFSET           TYPE                     VALUE
0000000000000077 X86_64_RELOC_BRANCH      _printf
0000000000000070 X86_64_RELOC_SIGNED      _banner
0000000000000069 X86_64_RELOC_SIGNED      __cstring
0000000000000060 X86_64_RELOC_BRANCH      _compute
000000000000002e X86_64_RELOC_GOT_LOAD    _zero_counter@GOTPCREL
0000000000000025 X86_64_RELOC_GOT_LOAD    _zero_counter@GOTPCREL
000000000000001c X86_64_RELOC_BRANCH      _scale
0000000000000012 X86_64_RELOC_BRANCH      _local_bias
000000000000000a X86_64_RELOC_SIGNED      _global_counter

RELOCATION RECORDS FOR [__compact_unwind]:
OFFSET           TYPE                     VALUE
0000000000000040 X86_64_RELOC_UNSIGNED    __text
0000000000000020 X86_64_RELOC_UNSIGNED    __text
0000000000000000 X86_64_RELOC_UNSIGNED    __text
== link and run ==
object result: 20
```

Esta corrida es en macOS, así que el formato de object es Mach-O y los símbolos C tienen
guiones bajos iniciales. En Linux verías nombres de section ELF como `.text`, `.data`,
`.bss`, `.rodata`, `.symtab`, `.strtab`, y sections de relocation como `.rela.text`. Las
preguntas de inspección son idénticas: qué bytes existen, qué nombres existen y qué bytes
todavía necesitan parches del linker.

## Modos de falla y trade-offs

- **Un `.o` no es ejecutable.** No tiene contrato de entry point de proceso, no tiene layout
  final y muchas veces tiene referencias unresolved. Corré el ejecutable linkeado, no la
  entrada relocatable.
- **Arquitectura y ABI tienen que coincidir.** Un object Mach-O x86-64 no se puede linkear
  dentro de un ejecutable ELF x86-64, y un object ARM64 no se puede linkear dentro de un
  programa x86-64. Importan el machine code, los tipos de relocation, la calling convention
  y el formato contenedor.
- **Objects stale crean bugs falsos.** Si cambió el header pero un `.o` no se reconstruyó,
  el linker puede combinar supuestos incompatibles. Los build systems existen en gran parte
  para evitar object files stale.
- **Debug y unwind metadata no son ruido.** Strippearlas puede achicar la salida, pero
  también puede quitar visibilidad del debugger o empeorar stack traces. Tratá tamaño y
  observabilidad como un trade-off.
- **Los errores de tipo de relocation son errores reales del target.** "Relocation
  truncated" o "unsupported relocation" significa que el code model, target o layout de link
  no puede representar la dirección en la forma pedida.

## En la práctica

- **Empezá con `file`.** Te dice si tenés ELF, Mach-O, COFF, la arquitectura correcta y un
  object relocatable vs un ejecutable.
- **Usá `nm` antes de leer bytes crudos.** Símbolos definidos vs undefined suele explicar
  fallas de link más rápido que el disassembly.
- **Usá section headers para orientarte.** Si `.text` es chico pero las debug sections son
  enormes, estás mirando metadata. Si `.bss` es grande pero el tamaño de archivo es chico,
  ese storage zero-filled está haciendo su trabajo.
- **Usá relocations para entender los "huecos."** Son la lista exacta de lugares que el
  linker debe parchear; los símbolos unresolved no son vagos, tienen offsets y tipos de
  relocation.
- **Mantené claro el borde del formato de object.** Esta nota es sobre object files
  relocatable en general. La próxima nota hace zoom en [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|ELF]] específicamente.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link|El pipeline: preprocess → compile → assemble → link]] · [[lowlevel/toolchain-and-linking/the-elf-format-sections-segments-headers|El formato ELF]] · [[lowlevel/toolchain-and-linking/symbols-definition-reference-and-resolution|Símbolos: definición, referencia y resolución]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declaraciones vs definiciones y linkage]]

## Fuentes

- **GCC Manual — "Overall Options" (`-c`)** — object files como salida cuando compilación/assembly frena antes del linking. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
- **GNU binutils — manual de `objdump`** — inspección de section headers, tablas de símbolos y relocations desde object files. https://sourceware.org/binutils/docs/binutils/objdump.html
- **GNU binutils — manual de `nm`** — letras de clase de símbolo como `T`, `D`, `C`, locales en minúscula y `U`. https://sourceware.org/binutils/docs/binutils/nm.html
- **System V ABI — especificación ELF** — el modelo canónico para object files ELF, sections, símbolos y relocations. https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html
- **Ian Lance Taylor — serie *Linkers*** — explicación orientada a linkers de por qué existen los object files y qué consumen los linkers. https://www.airs.com/blog/archives/38
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 7 — object files, relocations y linking como sistema visible para programadores. https://csapp.cs.cmu.edu/
