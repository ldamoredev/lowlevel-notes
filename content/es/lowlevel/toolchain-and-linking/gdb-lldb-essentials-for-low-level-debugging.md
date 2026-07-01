---
title: "gdb / lldb esenciales para debugging de bajo nivel"
description: El debugging de bajo nivel empieza con símbolos y un debugger: compilá con -g, poné breakpoints, step, inspeccioná frames, variables, memoria, registros y core dumps postmortem.
tags: [toolchain, debugging, gdb, lldb, registers, core-dumps]
order: 11
updated: 2026-06-22
---
# gdb / lldb esenciales para debugging de bajo nivel

Un debugger es un microscopio controlado para un programa corriendo. Compilás con debug
information, frenás la ejecución en un punto elegido, inspeccionás el stack frame actual,
avanzás una línea fuente o instrucción por vez, y leés memoria/registros cuando el estado a
nivel fuente no alcanza. `gdb` y `lldb` tienen sintaxis distinta, pero el workflow de bajo
nivel es el mismo: frenar, orientarte, inspeccionar, avanzar y probar la hipótesis.

> El reset: `-g` no vuelve el código "debug mode." Agrega metadata que deja al debugger
> mapear direcciones de máquina a source files, funciones, líneas, variables y tipos.

## Cómo funciona realmente

Los debuggers combinan varias capas de información. El ejecutable contiene machine code y,
cuando fue construido con `-g`, debug sections o metadata equivalente de plataforma. El OS
expone control sobre otro proceso mediante APIs de debugging como `ptrace` en sistemas
Unix-like o integración debugserver/dyld en macOS. El debugger usa ambas: control del OS
para frenar y leer el proceso, debug info para explicar qué significan esos bytes y
registros.

Los comandos núcleo mapean limpiamente entre GDB y LLDB:

| Tarea | GDB | LLDB |
|---|---|---|
| Arrancar programa | `run` | `run` |
| Break en función | `break accumulate` | `breakpoint set --name accumulate` |
| Backtrace | `bt` | `bt` |
| Imprimir variable | `print value` | `frame variable value` o `expression value` |
| Step over | `next` | `thread step-over` |
| Step into | `step` | `thread step-in` |
| Examinar memoria | `x/16xb ptr` | `memory read --size 1 --count 16 ptr` |
| Registros | `info registers` | `register read` |

Los breakpoints reemplazan "agregar printf y recompilar" por un punto de stop. Los
backtraces muestran cómo llegaste ahí. Las frame variables muestran estado local con info
de tipos. Las lecturas de memoria y registros son donde cae la abstracción: los punteros se
vuelven direcciones, los stack frames se vuelven bytes y los detalles de calling convention
de la rama de assembly se vuelven visibles.

## Core dumps y debugging postmortem

Un core dump es un snapshot de un proceso que crasheó. En vez de reproducir el crash en vivo,
cargás el ejecutable más el core file e inspeccionás el último estado:

```bash
gdb ./demo core
lldb --core core ./demo
```

Los core dumps dependen de política del OS. Linux suele necesitar `ulimit -c unlimited` y
puede rutear dumps mediante `systemd-coredump` o `/proc/sys/kernel/core_pattern`. macOS
tiene sus propios controles de crash reporting y core dumps. El concepto es estable: debug
info más un snapshot de memoria/registros te deja preguntar "¿dónde murió y cuál era el
estado?"

## Artefacto ejecutable: breakpoint, step, variables, registros

El ejemplo vive en
`examples/toolchain-and-linking/gdb-lldb-essentials-for-low-level-debugging/`. Compila con
`-g`, corre una vez normal y después corre LLDB en batch mode. El script redacta direcciones
y process ids volátiles para mantener comparable la salida mientras sigue usando output real
del debugger.

```c
#include <stdio.h>

static int accumulate(int value) {
    int doubled = value * 2;
    int biased = doubled + 5;
    return biased;
}

int main(void) {
    int result = accumulate(7);
    printf("debug result: %d\n", result);
    return 0;
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/gdb-lldb-essentials-for-low-level-debugging
./run.sh
```

Salida real de esta máquina:

```text
== compile with debug info ==
debug result: 19
== lldb batch session ==
Breakpoint 1: 2 locations.
    frame #0: 0xADDR demo`accumulate(value=7) at demo.c:4:19
(int) value = 7
    frame #0: 0xADDR demo`accumulate(value=7) at demo.c:5:18
(int) doubled = 14
    frame #0: 0xADDR demo`accumulate(value=7) at demo.c:6:12
(int) biased = 19
  * frame #0: 0xADDR demo`accumulate(value=7) at demo.c:6:12
     rip = 0xADDR  demo`accumulate + 25 at demo.c:6:12
     rsp = 0xADDR
     rbp = 0xADDR
Process PID exited with status = 0 (0xADDR)
```

El breakpoint frena en `accumulate`. La primera frame variable muestra el argumento. Dos
comandos step-over ejecutan las asignaciones, así `doubled` y `biased` se vuelven
inspeccionables. La línea de backtrace muestra el frame actual. La lectura de registros
muestra `rip`, `rsp` y `rbp`, que conectan directamente con control flow x86-64 y mecánica
de stack frames.

## Modos de falla y trade-offs

- **Sin debug info.** Sin `-g`, igual podés debuggear assembly y símbolos, pero líneas
  fuente, locals y tipos pueden faltar o degradarse.
- **La optimización cambia la realidad.** Con `-O2`, variables pueden optimizarse fuera,
  inlinearse o vivir solo en registros. Debuggear código optimizado es real pero menos
  directo.
- **Permisos de debugger.** macOS y setups Linux hardened pueden bloquear launch o attach
  salvo que se otorguen permisos de debugger.
- **Fuente stale vs binario.** Si el ejecutable no fue reconstruido desde el source que
  estás viendo, líneas y variables pueden engañarte.
- **Core dumps pueden estar deshabilitados.** Debugging postmortem requiere que los límites
  del OS y el routing de crash dumps permitan capturar cores.
- **Existen heisenbugs.** Timing, threads, señales y data races pueden cambiar de
  comportamiento cuando un debugger frena el proceso.

## En la práctica

- **Compilá primero un debug build.** `-O0 -g -Wall -Wextra` es el punto de partida simple.
- **Poné breakpoints sobre hipótesis angostas.** Frená cerca de la transición sospechosa, no
  al principio de todo el programa.
- **Usá backtrace antes de steppear.** Saber cómo llegaste ahí suele importar más que la
  línea actual.
- **Leé memoria cuando los punteros son sospechosos.** Variables fuente pueden esconder bugs
  de aliasing, lifetime y layout.
- **Leé registros cuando importan ABI o assembly.** Calls, returns, stack corruption y crash
  addresses son hechos a nivel registro.
- **Usá sanitizers antes del debugging heroico.** ASan/UBSan suelen señalar directo bugs de
  memoria y UB que llevaría más tiempo perseguir a mano.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/assembly-and-compiler-output/stack-frames-function-prologue-and-epilogue|Stack frames]] · [[lowlevel/assembly-and-compiler-output/system-v-amd64-calling-convention|System V AMD64 calling convention]] · [[lowlevel/pointers-and-memory/use-after-free-and-double-free|Use-after-free y double-free]] · [[lowlevel/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model|gcc vs clang y el modelo de compiler driver]]

## Fuentes

- **GDB Manual** — breakpoints, stepping, stack frames, memory examination, registros y core files. https://sourceware.org/gdb/current/onlinedocs/gdb.html/
- **LLDB Tutorial** — ejemplos oficiales de comandos para breakpoints, stepping, frames, variables y expressions. https://lldb.llvm.org/use/tutorial.html
- **LLDB GDB command map** — mapeo práctico entre comandos comunes de GDB y LLDB. https://lldb.llvm.org/use/map.html
- **GCC Debugging Options** — `-g` y generación de debug info desde el compiler driver. https://gcc.gnu.org/onlinedocs/gcc/Debugging-Options.html
- **Apple Debugging and LLDB docs** — comportamiento específico de LLDB/debugserver en macOS. https://developer.apple.com/documentation/xcode/debugging
