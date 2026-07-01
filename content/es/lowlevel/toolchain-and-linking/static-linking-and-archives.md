---
title: "Static linking y archives (`.a`)"
description: Los static archives son bibliotecas de object files relocatable; el linker extrae solo los miembros necesarios para resolver símbolos unresolved, y el orden de bibliotecas importa.
tags: [toolchain, linker, static-linking, archives, libraries]
order: 5
updated: 2026-06-22
---
# Static linking y archives (`.a`)

Una static library (biblioteca estática) no es un formato binario especial con un único
cuerpo de código mergeado. Es un **archive**: una tabla de miembros `.o` relocatable más un
índice de símbolos que esos miembros definen. Durante un link estático, el linker escanea
objects y archives, extrae solo los miembros que satisfacen símbolos unresolved que ya vio,
y después trata esos miembros extraídos como object files comunes. Esa regla de extracción
es la razón por la que el orden `main.o -lthing` vs `-lthing main.o` puede decidir si el
link tiene éxito.

> El reset: un archive se busca, no se linkea entero por default. Los object files se
> incluyen siempre cuando los nombrás; los miembros de un archive se incluyen solo cuando
> responden una necesidad actual.

## Cómo funciona realmente

El static linking consume objects relocatable y produce un ejecutable que ya contiene una
copia del código de biblioteca requerido. Para un object file común, la regla es simple: si
nombrás `helper.o` en la línea de comandos, el linker lo incluye. Para un archive como
`libcalc.a`, el linker abre el índice del archive y extrae solo miembros cuyas definiciones
resuelven símbolos externos undefined actuales.

La convención Unix usual es `libNAME.a`, usada desde el driver como `-lNAME`. El driver pasa
el search path de bibliotecas con `-Ldir`; el linker busca `libNAME.so` o `libNAME.a` según
los defaults y flags de la plataforma. Cuando se selecciona el static archive, el índice de
símbolos deja que el linker salte al miembro que define un símbolo necesario en vez de leer
todos los objects a ciegas.

La regla left-to-right es la parte que muerde. Si el linker ve `main.o` primero, registra
referencias undefined como `calc_add` y `calc_mul`. Cuando después escanea `libcalc.a`,
extrae `add.o` y `mul.o` porque esos miembros definen los símbolos pendientes. Si ve
`-lcalc` antes de `main.o`, todavía no hay referencias pendientes, entonces el archive no
aporta nada; después aparece `main.o`, los símbolos quedan unresolved y el archive no se
vuelve a escanear automáticamente.

| Tipo de input | Comportamiento del linker |
|---|---|
| `main.o` | siempre se incluye cuando lo nombrás |
| `libcalc.a(add.o)` | se extrae solo si resuelve un símbolo undefined actual |
| `libcalc.a(mul.o)` | misma regla de miembro de archive |
| `libc` | normalmente la agrega el compiler driver para C hosted |

El static linking te da una copia autocontenida del código de biblioteca seleccionado, pero
no borra las reglas de ABI. Los objects todavía tienen que coincidir en arquitectura target,
formato de object, calling convention, modelo de relocation y supuestos de runtime. Un
archive Linux/ELF no se puede linkear dentro de un ejecutable macOS/Mach-O solo porque ambos
contengan machine code x86-64.

## Artefacto ejecutable: extracción de archive y orden

El ejemplo vive en
`examples/toolchain-and-linking/static-linking-and-archives/`. Construye dos objects de
biblioteca, los empaqueta en `libcalc.a`, muestra los símbolos del archive y corre el
programa nativo. El script también chequea el orden no portable `-lcalc main.o` en este host
para que la nota sea honesta sobre el comportamiento de la plataforma.

`main.c` usa declaraciones desde `calc.h`:

```c
#include <stdio.h>

#include "calc.h"

int main(void) {
    int sum = calc_add(2, 5);
    int product = calc_mul(sum, 3);
    printf("static archive result: %d\n", product);
    return 0;
}
```

Los miembros del archive definen los símbolos:

```c
#include "calc.h"

int calc_add(int left, int right) {
    return left + right;
}
```

```c
#include "calc.h"

int calc_mul(int left, int right) {
    return left * right;
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/static-linking-and-archives
./run.sh
```

Salida real de esta máquina:

```text
== native archive and run ==
__.SYMDEF SORTED
add.o
mul.o
== symbols inside archive members ==

add.o:
0000000000000000 T _calc_add

mul.o:
0000000000000000 T _calc_mul
== library before object on this Apple ld ==
static archive result: 21
== portable order: object before library ==
static archive result: 21
== portability note ==
GNU/ELF linkers commonly require the portable order above for static archives.
```

Esta corrida es en macOS/Mach-O, así que los símbolos C tienen guion bajo inicial y Apple
`ld` acepta este caso chico `-lcalc main.o`. No generalices eso a Linux/ELF. La búsqueda de
archives estilo GNU es la regla más estricta descrita arriba: poné los objects que crean
referencias unresolved antes de los archives que las satisfacen. En Linux/ELF, el mismo
`nm` normalmente imprime `calc_add` y `calc_mul` sin guion bajo, y el orden no portable
suele fallar con un diagnostic de undefined reference.

## Modos de falla y trade-offs

- **Orden incorrecto de bibliotecas.** La extracción desde static archives depende del
  orden. Poné los objects que crean referencias antes de los archives que las satisfacen.
- **Dependencias circulares entre archives.** Si `liba.a` y `libb.a` se necesitan entre sí,
  un escaneo puede no alcanzar. Linkers GNU tienen opciones de grupo como
  `--start-group`/`--end-group` con costo; otra solución es diseñar bordes de archive más
  limpios.
- **Definiciones strong duplicadas.** Si dos objects incluidos definen el mismo símbolo
  externo, el link falla. Los archives pueden esconder esto hasta que se extrae un miembro.
- **Ejecutables más grandes.** Static linking copia código de biblioteca seleccionado en
  cada programa. Puede ayudar al deploy, pero también duplica código entre muchos procesos.
- **Fricción para updates.** Una biblioteca dynamic puede parchearse en un lugar; un
  programa linkeado estáticamente debe relinkearse y redistribuirse para tomar fixes.
- **Restricciones de licencia y ABI.** Algunas bibliotecas imponen obligaciones de
  distribución o esperan dynamic linking. Static no significa "libre de política."

## En la práctica

- **Leé líneas de link de izquierda a derecha.** Cuando falla un static link, mové `-lfoo`
  después de los objects que referencian `foo`, y después inspeccioná con `nm` antes de
  cambiar fuente.
- **Usá `ar -t` y `nm libx.a`.** Te dicen qué miembros existen y qué símbolos definen
  realmente.
- **Preferí ownership explícito de objects.** Los headers declaran; un `.c` es dueño de
  cada definición; los archives empaquetan objects sin cambiar esa regla.
- **Usá static linking deliberadamente.** Es excelente para runtimes freestanding,
  envelopes de deploy chicos, rescue tools y bibliotecas de soporte para proyectos de OS.
  Es menos cómodo para updates de seguridad rápidos y bibliotecas del sistema compartidas.
- **Dejá que el compiler driver invoque el linker.** `gcc main.o -L. -lcalc` agrega las
  piezas hosted del runtime C; con `ld` directo tenés que pasarlas vos.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/toolchain-and-linking/symbols-definition-reference-and-resolution|Símbolos: definición, referencia y resolución]] · [[lowlevel/toolchain-and-linking/object-files-and-whats-inside-them|Object files y qué tienen adentro]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declaraciones vs definiciones y linkage]]

## Fuentes

- **GNU binutils — manual de `ar`** — creación de archives, listado de miembros y comportamiento del índice de símbolos para static libraries. https://sourceware.org/binutils/docs/binutils/ar.html
- **GNU `ld` manual — búsqueda de archives** — cómo los miembros de archives se extraen para resolver símbolos undefined y por qué importa el orden. https://sourceware.org/binutils/docs/ld/
- **Ian Lance Taylor — serie *Linkers*** — vista de implementación de linker sobre escaneo de archives y resolución de símbolos. https://www.airs.com/blog/archives/38
- **David Drysdale — *Beginner's Guide to Linkers*** — explicación práctica de static libraries y la regla left-to-right del link. https://www.lurklurk.org/linkers/linkers.html
- **John R. Levine — *Linkers and Loaders*** — static linking, bibliotecas, resolución de símbolos y contexto de loader. https://linker.iecc.com/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 7 — archivos archive y static linking desde el lado visible para programadores. https://csapp.cs.cmu.edu/
