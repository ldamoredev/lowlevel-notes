---
title: "`cmake` (cuando make no alcanza)"
description: CMake es un generador de builds: describís targets y requirements una vez, y después escribe build files nativos como Makefiles o archivos Ninja para una plataforma.
tags: [toolchain, cmake, build-systems, make, targets]
order: 10
updated: 2026-06-22
---
# `cmake` (cuando make no alcanza)

CMake no es un compilador ni la herramienta de build que normalmente ejecuta cada comando
de compile. Es un **build generator** (generador de builds). Describís targets, archivos
fuente, include paths, compile features, bibliotecas, reglas de install y opciones en
`CMakeLists.txt`; CMake configura un build directory y escribe build files nativos para
Make, Ninja, Xcode, Visual Studio u otro backend. El valor aparece cuando el proyecto supera
Makefiles mantenidos a mano: múltiples plataformas, dependencias opcionales, archivos
generados, tests, installs y límites de bibliotecas.

> El reset: make ejecuta un dependency graph. CMake escribe ese grafo para un generator
> elegido desde un modelo de targets más alto.

## Cómo funciona realmente

CMake tiene dos fases principales. La fase configure/generate lee `CMakeLists.txt`, detecta
compiladores y features de plataforma, evalúa opciones y escribe archivos de build system
dentro de un build directory separado. La fase build delega al backend generado:

```bash
cmake -S . -B build
cmake --build build
```

El CMake moderno está orientado a targets. Evitás pilas globales de flags y en su lugar
pegás requirements a targets:

| Comando CMake | Significado |
|---|---|
| `add_library(message STATIC message.c)` | construye un target de biblioteca |
| `add_executable(demo main.c)` | construye un target ejecutable |
| `target_include_directories(message PUBLIC ...)` | consumidores de `message` heredan include paths |
| `target_link_libraries(demo PRIVATE message)` | `demo` linkea contra `message` sin exportar esa dependencia |
| `target_compile_options(demo PRIVATE -Wall -Wextra)` | agrega flags de compile a un target |

Los builds out-of-source son la norma: los sources quedan limpios, mientras que build files
generados, objects, caches y binarios viven en `build/`. Eso hace fácil tirar un build
directory, configurar Debug y Release lado a lado, o construir el mismo source tree con
distintos compiladores.

## Cuándo supera a make escrito a mano

Make es excelente cuando querés control exacto sobre un grafo chico. CMake se vuelve
atractivo cuando el grafo debe adaptarse: feature checks, descubrimiento de bibliotecas,
flags específicos de plataforma, layout de instalación, integración de tests, generación de
proyectos IDE, toolchain files de cross-compilation y packaging. También es la lingua franca
de muchas dependencias C/C++, así que entender el modelo de targets importa incluso si
preferís herramientas más simples para proyectos chicos.

CMake no es gratis. Tiene su propio lenguaje, modelo de cache, comportamiento dependiente
del generator y patrones globales viejos que todavía aparecen en tutoriales. La regla de
decisión es simple: si tu build es un ejecutable y dos objects, un Makefile es más claro. Si
necesitás targets portables entre máquinas y toolchains, CMake se gana su lugar.

## Artefacto ejecutable: modelo de targets más fallback local

El ejemplo vive en `examples/toolchain-and-linking/cmake-when-make-isnt-enough/`. Contiene
un `CMakeLists.txt` real, pero `cmake` no está instalado en este entorno local, así que el
script cae honestamente a compilar y correr los mismos sources con `cc`.

```cmake
cmake_minimum_required(VERSION 3.16)

project(cmake_demo C)

add_library(message STATIC message.c)
target_include_directories(message PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(message PRIVATE -Wall -Wextra)

add_executable(demo main.c)
target_link_libraries(demo PRIVATE message)
target_compile_options(demo PRIVATE -Wall -Wextra)
```

El código C es ordinario:

```c
#include <stdio.h>

#include "message.h"

int main(void) {
    printf("%s result: %d\n", message_label(), message_value());
    return 0;
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/cmake-when-make-isnt-enough
./run.sh
```

Salida real de esta máquina:

```text
== cmake configure/build if available ==
cmake: not installed on this machine
== fallback compile/run with same sources ==
cmake target result: 42
```

En una máquina con CMake instalado, los comandos buscados son `cmake -S . -B build`, después
`cmake --build build`, y después `./build/demo`. Eso verificaría el mismo grafo de targets
en vez de la línea de compile fallback.

## Modos de falla y trade-offs

- **In-source builds ensucian el árbol.** Preferí `cmake -S . -B build` para que los archivos
  generados sean descartables.
- **Flags globales se filtran.** Patrones viejos como editar `CMAKE_C_FLAGS` globalmente
  hacen más difícil componer targets. Preferí target properties.
- **El cache puede mentir.** `CMakeCache.txt` registra paths y opciones detectadas. Cuando
  el build se comporta imposible, borrá el build directory y reconfigurá.
- **Existen diferencias entre generators.** Backends Make, Ninja, Xcode y Visual Studio no
  exponen comportamiento idéntico.
- **Puede ser demasiada herramienta.** Para un ejercicio chico de una plataforma, CMake
  puede oscurecer más de lo que ayuda.
- **Cross-compilation necesita toolchain files explícitos.** Adivinar desde el host está mal
  para un kernel de OS o target embedded.

## En la práctica

- **Pensá en targets.** Bibliotecas y ejecutables son dueños de sus includes, compile
  definitions, flags y link dependencies.
- **Separá source y build directories.** `build/` debería poder borrarse sin miedo.
- **Usá CMake cuando la presión de portabilidad sea real.** Checks de plataforma y discovery
  de dependencias son donde paga su costo.
- **No escondas los básicos del compilador.** Igual necesitás entender objects, archives,
  shared libraries y linker flags porque CMake va a generar esos comandos.
- **Para el proyecto de OS, usá un toolchain file.** Targets freestanding necesitan un
  cross-compiler, supuestos de sysroot y comportamiento de linker que no matchean el host.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/toolchain-and-linking/make-and-the-build-dependency-graph|`make` y el grafo de dependencias de build]] · [[lowlevel/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model|gcc vs clang y el modelo de compiler driver]] · [[lowlevel/os-from-scratch/index|OS desde Cero]]

## Fuentes

- **CMake Tutorial** — introducción oficial al workflow configure/build y creación de targets. https://cmake.org/cmake/help/latest/guide/tutorial/index.html
- **Manual `cmake-buildsystem(7)` de CMake** — target properties, usage requirements y modelo de buildsystem. https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html
- **Comando `add_library` de CMake** — creación de targets de biblioteca, opciones static/shared y listas de sources. https://cmake.org/cmake/help/latest/command/add_library.html
- **Comando `target_link_libraries` de CMake** — link dependencies de targets y propagación PUBLIC/PRIVATE/INTERFACE. https://cmake.org/cmake/help/latest/command/target_link_libraries.html
- **Manual CMake Toolchains** — cross-compilation y modelo de toolchain files. https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html
