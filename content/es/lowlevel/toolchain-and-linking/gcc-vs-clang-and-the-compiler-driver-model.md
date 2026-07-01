---
title: "gcc vs clang y el modelo de compiler driver"
description: gcc y clang son comandos driver: parsean tus flags, eligen herramientas frontend/backend, agregan runtime objects y bibliotecas, y orquestan compile, assemble y link.
tags: [toolchain, gcc, clang, compiler-driver, frontend, backend]
order: 8
updated: 2026-06-22
---
# gcc vs clang y el modelo de compiler driver

`gcc` y `clang` son los comandos que tipeás, pero el comando es un **driver**. Decide qué
fases hacen falta, elige defaults de target, invoca compiler frontends, selecciona assembler
y linker, y agrega runtime objects y bibliotecas para C hosted. GCC y Clang tienen internos
y ecosistemas distintos, pero desde la shell exponen el mismo modelo mental: un comando
driver orquesta muchas herramientas de menor nivel. Cuando pasás `-v` o `-###`, el driver
deja de estar callado.

> El reset: no imagines `gcc main.c` como un blob único de compilador. Imaginalo como un
> driver que lee inputs y flags, y después produce un pipeline concreto para tu target.

## Cómo funciona realmente

GCC es a la vez una colección de compiladores y una interfaz driver. Para C, el driver de
GCC normalmente invoca componentes de preprocessing/compilación como `cc1`, un assembler
como `as`, y un paso de link mediante `collect2`/`ld`. Clang es a la vez un driver
compatible con GCC y un compiler frontend basado en LLVM; su paso interno de compilación
suele aparecer como `clang -cc1`, con LLVM IR y code generation de LLVM detrás. Clang suele
usar un assembler integrado, aunque también puede usar herramientas externas.

El driver elige fases desde inputs y opciones:

| Forma del comando | Trabajo del driver |
|---|---|
| `gcc -E main.c` | solo preprocess |
| `gcc -S main.c` | compile a assembly |
| `gcc -c main.c` | compile y assemble a `.o` |
| `gcc main.o helper.o -o app` | link de objects existentes |
| `clang -target x86_64-unknown-linux-gnu -c x.c` | compile para un target triple específico |

Flags como `-I`, `-D`, `-O2`, `-g`, `-Wall`, `-Wextra`, `-std=c17`, `-fPIC`, `-shared`,
`-L`, `-l` y `-Wl,option` pertenecen a partes distintas del pipeline. El driver los enruta.
`-I` y `-D` afectan preprocessing. `-O2`, `-g` y `-std=` afectan compilación. `-L`, `-l` y
muchos flags `-Wl,` afectan linking. Por eso una misma línea de comandos puede mezclar
opciones de lenguaje, codegen y linker sin que invoques cada herramienta a mano.

## Por qué el driver agrega cosas

Un programa C hosted necesita más que tus object files. Necesita startup code que entre al
runtime C, prepare `argc`/`argv`/estado de environment, llame initializers, llame `main` y
salga correctamente. También necesita libc y compiler runtime helpers para operaciones que
el target no inlinea directamente. El driver sabe qué startup objects y bibliotecas matchean
el target. Los comandos directos con `ld` suelen fallar porque saltean ese conocimiento.

Esto importa más adelante para el proyecto de OS. Código user-space hosted normalmente
debería dejar que el driver agregue defaults. Kernels freestanding hacen lo opuesto: usan un
cross-compiler y flags como `-ffreestanding`, `-nostdlib` y un linker script porque el
runtime hosted es el runtime equivocado.

## Artefacto ejecutable: compará drivers e inspeccioná `-###`

El ejemplo vive en
`examples/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model/`. Construye el
mismo programa mediante `gcc` y `clang`, y después le pide a Clang que imprima un trace de
fase compile-only. En este host macOS, `/usr/bin/gcc` es Apple clang vestido como driver
compatible con GCC, lo cual ya es un dato útil de plataforma.

```c
#include <stdio.h>

static int twice(int value) {
    return value * 2;
}

int main(void) {
    printf("driver model: %d\n", twice(4));
    return 0;
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model
./run.sh
```

Salida real de esta máquina:

```text
== build with gcc driver ==
driver model: 8
== build with clang driver ==
driver model: 8
== gcc driver identity on this host ==
Apple clang version 15.0.0 (clang-1500.3.9.4)
Target: x86_64-apple-darwin24.6.0
== clang driver phase trace (-###, compile only) ==
"-cc1"
"-triple" "x86_64-apple-macosx14.5.0"
"-emit-obj"
```

`-###` imprime los comandos que el driver correría sin correrlos realmente. El excerpt
muestra la invocación escondida del frontend `-cc1`, el target triple y el pedido de emitir
un object file. En Linux con GCC real instalado, `gcc -v` mostraría el `cc1` propio de GCC,
`as` y el path del linker; el modelo de driver es el mismo aunque la implementación cambie.

## Modos de falla y trade-offs

- **Asumir que `gcc` significa GNU GCC en todos lados.** En macOS, `/usr/bin/gcc` suele ser
  Apple clang. Siempre chequeá `--version` cuando el comportamiento importa.
- **Mandar un flag a la fase equivocada.** Flags de linker suelen necesitar `-Wl,`; flags
  de preprocessor no hacen nada en link time; `-lfoo` no hace nada durante `-c`.
- **Llamar `ld` directo.** Te salteás startup objects, bibliotecas por default, decisiones
  de search de bibliotecas y compiler runtime helpers salvo que los agregues vos.
- **Depender de compatibilidad perfecta GCC/Clang.** Clang acepta muchos flags de GCC, pero
  warnings, extensions, soporte de sanitizers, detalles de inline assembly y diagnostics
  pueden diferir.
- **Confusión de cross-target.** El target triple controla ABI, formato de object, tipos de
  relocation, include paths por default y expectativas de bibliotecas.
- **Mismatch freestanding vs hosted.** Un build de kernel no debería traer por accidente el
  runtime C del host; una app normal no debería omitirlo por accidente.

## En la práctica

- **Usá `-v` cuando importan los defaults.** Muestra include paths, tool paths, decisiones
  de target y detalles de invocación del linker.
- **Usá `-###` con Clang para un trace limpio del driver.** Es especialmente útil al enseñar
  o debuggear enrutamiento de fases.
- **Preferí el driver para links ordinarios.** Dejá que `gcc`/`clang` llamen al linker salvo
  que estés escribiendo deliberadamente un comando de link de bajo nivel.
- **Escribí warnings portables primero.** `-Wall -Wextra -Wpedantic` y un `-std=` elegido
  ayudan a hacer visibles temprano las diferencias GCC/Clang.
- **Sé explícito para trabajo de OS.** Prefijo de cross-compiler, target triple,
  `-ffreestanding`, `-nostdlib` y un linker script no son decoración; definen el entorno.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/toolchain-and-linking/the-pipeline-preprocess-compile-assemble-link|El pipeline: preprocess → compile → assemble → link]] · [[lowlevel/toolchain-and-linking/static-linking-and-archives|Static linking y archives]] · [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Por qué leer assembly]] · [[lowlevel/os-from-scratch/index|OS desde Cero]]

## Fuentes

- **GCC Manual — Overall Options** — `-E`, `-S`, `-c`, `-v` y control de fases del driver. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html
- **GCC Internals — `collect2`** — por qué GCC puede envolver o aumentar la invocación final del linker. https://gcc.gnu.org/onlinedocs/gccint/Collect2.html
- **Clang command guide** — opciones del driver Clang, selección de target, `-###` y comportamiento compatible con GCC. https://clang.llvm.org/docs/CommandGuide/clang.html
- **LLVM documentation — Clang design** — background de arquitectura frontend/driver/LLVM. https://clang.llvm.org/docs/DriverInternals.html
- **Manual de GNU `ld`** — el linker que el driver termina configurando en sistemas estilo GNU. https://sourceware.org/binutils/docs/ld/
- **OSDev Wiki — GCC Cross-Compiler** — por qué el trabajo de OS freestanding usa una cross-toolchain explícita en vez de defaults del host. https://wiki.osdev.org/GCC_Cross-Compiler
