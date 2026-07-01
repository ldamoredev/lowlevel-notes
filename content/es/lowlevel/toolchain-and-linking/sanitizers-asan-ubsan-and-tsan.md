---
title: "Sanitizers: ASan, UBSan y TSan"
description: Los sanitizers son modos de instrumentación del compilador: ASan detecta muchos bugs de memoria, UBSan detecta undefined behavior y TSan detecta data races con checks de runtime.
tags: [toolchain, sanitizers, asan, ubsan, tsan, debugging]
order: 12
updated: 2026-06-22
---
# Sanitizers: ASan, UBSan y TSan

Los sanitizers son checks de runtime insertados por el compilador. En vez de esperar que
memory corruption, undefined behavior o data races aparezcan como síntomas raros, construís
un binario especial que observa el programa mientras corre. AddressSanitizer (ASan) se
enfoca en memory safety, UndefinedBehaviorSanitizer (UBSan) se enfoca en violaciones de
reglas del lenguaje, y ThreadSanitizer (TSan) se enfoca en data races. No prueban
corrección, pero vuelven muchos bugs de bajo nivel ruidosos y locales.

> El reset: un sanitizer no es un debugger externo. El compilador instrumenta tu programa,
> linkea un sanitizer runtime y el runtime reporta violaciones mientras el programa ejecuta.

## Cómo funciona realmente

Los sanitizers agregan checks alrededor de operaciones que el compilador puede ver. ASan
rodea allocations con red zones, mantiene shadow memory poisoned y reporta out-of-bounds,
use-after-free y algunos bugs de lifetime de stack. UBSan inserta checks para operaciones
como signed overflow, shifts inválidos, supuestos null/dangling, violaciones de alignment y
casts malos según opciones habilitadas. TSan instrumenta accesos a memoria y operaciones de
sincronización para detectar accesos conflictivos no sincronizados desde distintos threads.

| Sanitizer | Detecta | Flag típico |
|---|---|---|
| ASan | heap/stack/global out-of-bounds, use-after-free, patrones de double-free | `-fsanitize=address` |
| UBSan | signed overflow, shifts inválidos, alignment, null, issues de vptr/cast según lenguaje | `-fsanitize=undefined` |
| TSan | data races entre threads | `-fsanitize=thread -pthread` |

El costo es deliberado. ASan suele usar alrededor de 2x memoria y overhead relevante de CPU.
TSan es más pesado porque rastrea historial de accesos entre threads. UBSan puede ser liviano
en modo recover o fallar fuerte con opciones trap. Los builds con sanitizers son para
desarrollo, CI, fuzzing y debugging; normalmente no son tu binario release de producción
salvo que elijas ese trade-off.

## Artefacto ejecutable: hacé ruidosos los bugs

El ejemplo vive en `examples/toolchain-and-linking/sanitizers-asan-ubsan-and-tsan/`. Tiene
un archivo C chico por sanitizer. El script captura fallas esperadas de sanitizer y sigue
corriendo para que se vean los tres reportes.

Demo ASan:

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *values = malloc(3 * sizeof(int));
    if (values == NULL) {
        return 1;
    }

    values[0] = 10;
    values[1] = 20;
    values[2] = 30;
    printf("asan before bug: %d\n", values[1]);
    fflush(stdout);

    values[3] = 40;

    free(values);
    return 0;
}
```

Demo UBSan:

```c
#include <limits.h>
#include <stdio.h>

int main(void) {
    int value = INT_MAX;
    int overflowed = value + 1;
    printf("ubsan result: %d\n", overflowed);
    return 0;
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/sanitizers-asan-ubsan-and-tsan
./run.sh
```

Salida real de esta máquina:

```text
== ASan: heap buffer overflow ==
asan exit status: 1
asan before bug: 20
==PID==ERROR: AddressSanitizer: heap-buffer-overflow on address 0xADDR at pc 0xADDR bp 0xADDR sp 0xADDR
asan_demo:x86_64+0xADDR) in main+0xADDR
== UBSan: signed integer overflow ==
ubsan exit status: 0
ubsan_demo.c:6:28: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
ubsan result: -2147483648
== TSan: data race ==
tsan exit status: 66
WARNING: ThreadSanitizer: data race (pid=PID)
tsan_demo:x86_64+0xADDR) in worker+0xADDR
tsan counter: 2000
```

El script normaliza direcciones y PIDs volátiles, pero los reportes son output real de
sanitizers en esta máquina. ASan frena en el write de heap un elemento más allá de la
allocation. UBSan reporta signed integer overflow pero continúa en modo recover, así que el
resultado wrappeado igual se imprime. TSan reporta una race sobre el contador global no
sincronizado y sale con el exit code configurado del sanitizer.

## Modos de falla y trade-offs

- **La instrumentación ve paths ejecutados.** Si el camino malo nunca corre, el sanitizer no
  puede reportarlo.
- **No todos los bugs están cubiertos.** ASan no prueba que toda memoria sea segura; UBSan
  no cubre todo undefined behavior posible; TSan reporta data races, no todos los bugs de
  concurrencia.
- **El overhead es real.** Binarios sanitizados son más lentos y grandes, especialmente los
  builds TSan.
- **La optimización puede ayudar o dañar diagnostics.** Usá un nivel debuggeable como
  `-O1 -g` o `-O0 -g` según sanitizer y reproducción.
- **Los sanitizers pueden conflictuar.** ASan y TSan generalmente no se combinan en un mismo
  binario. Construí configuraciones separadas.
- **La disponibilidad de runtime depende de plataforma.** Apple clang, Clang upstream y GCC
  tienen matrices de soporte distintas para sanitizers.

## En la práctica

- **Corré ASan temprano y seguido.** Es el camino más rápido para detectar muchos bugs de
  memoria C antes de que sean crashes misteriosos.
- **Mantené UBSan en CI.** Signed overflow y shifts inválidos suelen señalar supuestos
  incorrectos incluso cuando el programa "parece andar."
- **Usá TSan en tests con threads.** Es pesado, pero las data races son demasiado sutiles
  para debuggearlas solo por inspección.
- **Compilá todo lo relevante con el sanitizer.** Código instrumentado llamando grandes
  regiones no instrumentadas puede perder contexto.
- **Combiná sanitizers con debuggers.** Dejá que el sanitizer encuentre la violación y usá
  LLDB o GDB cuando necesites inspeccionar estado alrededor.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/toolchain-and-linking/gdb-lldb-essentials-for-low-level-debugging|gdb / lldb esenciales]] · [[lowlevel/pointers-and-memory/buffer-overflow-and-out-of-bounds-access|Buffer overflow y out-of-bounds access]] · [[lowlevel/pointers-and-memory/use-after-free-and-double-free|Use-after-free y double-free]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior]]

## Fuentes

- **Clang AddressSanitizer documentation** — flags de build ASan, comportamiento de runtime y clases de bugs soportadas. https://clang.llvm.org/docs/AddressSanitizer.html
- **Clang UndefinedBehaviorSanitizer documentation** — checks UBSan, modos recover/trap y flags. https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
- **Clang ThreadSanitizer documentation** — uso de TSan, overhead, plataformas soportadas y reportes de races. https://clang.llvm.org/docs/ThreadSanitizer.html
- **GCC Instrumentation Options** — flags de sanitizers en GCC y opciones relacionadas de runtime. https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html
- **Google Sanitizers Wiki** — notas prácticas sobre runtimes de sanitizers y diagnostics. https://github.com/google/sanitizers/wiki
