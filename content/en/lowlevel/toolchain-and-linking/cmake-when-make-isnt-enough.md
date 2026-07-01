---
title: "`cmake` (when make isn't enough)"
description: CMake is a build generator: you describe targets and requirements once, then it writes native build files such as Makefiles or Ninja files for a platform.
tags: [toolchain, cmake, build-systems, make, targets]
order: 10
updated: 2026-06-22
---
# `cmake` (when make isn't enough)

CMake is not a compiler and not the build tool that usually executes each compile command.
It is a **build generator**. You describe targets, source files, include paths, compile
features, libraries, install rules, and options in `CMakeLists.txt`; CMake configures a
build directory and writes native build files for Make, Ninja, Xcode, Visual Studio, or
another backend. The value appears when the project outgrows hand-maintained Makefiles:
multiple platforms, optional dependencies, generated files, tests, installs, and library
boundaries.

> The reset: make executes a dependency graph. CMake writes that graph for a chosen
> generator from a higher-level target model.

## How it really works

CMake has two main phases. The configure/generate phase reads `CMakeLists.txt`, detects
compilers and platform features, evaluates options, and writes build-system files under a
separate build directory. The build phase delegates to the generated backend:

```bash
cmake -S . -B build
cmake --build build
```

Modern CMake is target-oriented. You avoid global piles of flags and instead attach
requirements to targets:

| CMake command | Meaning |
|---|---|
| `add_library(message STATIC message.c)` | build a library target |
| `add_executable(demo main.c)` | build an executable target |
| `target_include_directories(message PUBLIC ...)` | consumers of `message` inherit include paths |
| `target_link_libraries(demo PRIVATE message)` | `demo` links to `message` without exporting that dependency |
| `target_compile_options(demo PRIVATE -Wall -Wextra)` | attach compile flags to one target |

Out-of-source builds are the norm: source files stay clean, while generated build files,
objects, caches, and binaries live in `build/`. That makes it easy to throw away a build
directory, configure Debug and Release side by side, or build the same source tree with
different compilers.

## When it beats hand-written make

Make is excellent when you want exact control over a small graph. CMake becomes attractive
when the graph must adapt: feature checks, library discovery, platform-specific flags,
installation layout, test integration, IDE project generation, cross-compilation toolchain
files, and packaging. It is also the lingua franca for many C/C++ dependencies, so knowing
the target model matters even if you prefer simpler tools for tiny projects.

CMake is not free. It has its own language, cache model, generator-specific behavior, and
old global-style patterns that still appear in tutorials. The decision rule is simple: if
your build is one executable and two objects, a Makefile is clearer. If you need portable
targets across machines and toolchains, CMake earns its keep.

## Executable artifact: target model plus local fallback

The example lives in `examples/toolchain-and-linking/cmake-when-make-isnt-enough/`. It
contains a real `CMakeLists.txt`, but `cmake` is not installed in this local environment, so
the script truthfully falls back to compiling and running the same sources with `cc`.

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

The C code is ordinary:

```c
#include <stdio.h>

#include "message.h"

int main(void) {
    printf("%s result: %d\n", message_label(), message_value());
    return 0;
}
```

Run it:

```bash
cd examples/toolchain-and-linking/cmake-when-make-isnt-enough
./run.sh
```

Real output from this machine:

```text
== cmake configure/build if available ==
cmake: not installed on this machine
== fallback compile/run with same sources ==
cmake target result: 42
```

On a machine with CMake installed, the intended commands are `cmake -S . -B build`, then
`cmake --build build`, then `./build/demo`. That would verify the same target graph instead
of the fallback compile line.

## Failure modes & trade-offs

- **In-source builds pollute the tree.** Prefer `cmake -S . -B build` so generated files are
  disposable.
- **Global flags leak.** Old patterns such as editing `CMAKE_C_FLAGS` globally make targets
  harder to compose. Prefer target properties.
- **Cache state can lie.** `CMakeCache.txt` records detected paths and options. When the
  build behaves impossibly, delete the build directory and reconfigure.
- **Generator differences exist.** Make, Ninja, Xcode, and Visual Studio backends do not
  expose identical behavior.
- **It can be too much tool.** For a tiny one-platform exercise, CMake may obscure more than
  it helps.
- **Cross-compilation needs explicit toolchain files.** Guessing from the host is wrong for
  an OS kernel or embedded target.

## In practice

- **Think in targets.** Libraries and executables own their includes, compile definitions,
  flags, and link dependencies.
- **Keep source and build directories separate.** `build/` should be safe to delete.
- **Use CMake when portability pressure is real.** Platform checks and dependency discovery
  are where it pays for itself.
- **Do not hide compiler basics.** You still need to understand objects, archives, shared
  libraries, and linker flags because CMake will generate those commands.
- **For the OS project, use a toolchain file.** Freestanding targets need a cross-compiler,
  sysroot assumptions, and linker behavior that do not match the host.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/toolchain-and-linking/make-and-the-build-dependency-graph|`make` and the build dependency graph]] · [[lowlevel/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model|gcc vs clang and the compiler driver model]] · [[lowlevel/os-from-scratch/index|OS from Scratch]]

## Sources

- **CMake Tutorial** — official introduction to configure/build workflow and target creation. https://cmake.org/cmake/help/latest/guide/tutorial/index.html
- **CMake `cmake-buildsystem(7)` manual** — target properties, usage requirements, and the buildsystem model. https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html
- **CMake `add_library` command** — library target creation, static/shared choices, and source lists. https://cmake.org/cmake/help/latest/command/add_library.html
- **CMake `target_link_libraries` command** — target link dependencies and PUBLIC/PRIVATE/INTERFACE propagation. https://cmake.org/cmake/help/latest/command/target_link_libraries.html
- **CMake Toolchains manual** — cross-compilation and toolchain-file model. https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html
