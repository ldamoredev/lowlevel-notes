---
title: "Registros x86-64 y el register file"
description: El register file x86-64 es el workspace chico y nombrado del compilador. Aprendé los registros de propósito general, aliases de sub-registros, roles de ABI y cómo trazar valores en salida real del compilador.
tags: [assembly, x86-64, registers, abi, compiler-output]
order: 2
updated: 2026-06-30
---
# Registros x86-64 y el register file

El assembly se vuelve legible cuando dejás de ver una pared de mnemónicos y empezás a
rastrear dónde viven los valores. En x86-64, la superficie principal de trabajo es el
**register file**: un set chico de slots de almacenamiento nombrados dentro de la CPU
que las instrucciones leen y escriben directamente. El compilador decide todo el tiempo
qué valores vivos de C merecen esos slots y cuáles tienen que derramarse a memoria. Si
podés reconocer `rdi`, `rsi`, `rax`, `rsp` y compañía, la salida del compilador deja de
ser ruido y empieza a ser un boceto de dataflow.

> El reset: una variable de C no es un lugar. Un registro sí. Cuando corre código
> optimizado, el compilador puede mantener un valor en un registro, moverlo a otro,
> recomputarlo o borrarlo por completo.

## Cómo funciona realmente

x86-64 expone dieciséis **registros de propósito general (GPRs)** de 64 bits. Son
"generales" porque la mayoría del trabajo entero, de punteros, direcciones y control
flow puede usarlos, pero no son todos socialmente iguales. La ISA le da comportamiento
especial a algunos registros, y la ABI les da trabajos convencionales a muchos.

Para targets x86-64 Unix-like, la ABI System V AMD64 te da este mapa práctico de
lectura:

| Registro | Qué reconocer primero |
|---|---|
| `rax` | valor de retorno entero/puntero; acumulador scratch común |
| `rdi`, `rsi` | 1er y 2º argumento entero/puntero |
| `rdx`, `rcx` | 3er y 4º argumento entero/puntero |
| `r8`, `r9` | 5º y 6º argumento entero/puntero |
| `r10`, `r11` | registros scratch caller-saved |
| `rbx`, `r12`–`r15` | registros callee-saved; una función debe restaurarlos si los usa |
| `rbp` | frame pointer por convención; callee-saved |
| `rsp` | stack pointer; no es un registro scratch normal |

También hay registros que vas a leer constantemente pero no tratás como slots de datos
ordinarios: `rip` es el instruction pointer, `rflags` contiene bits de condición, y los
registros `xmm`/`ymm`/`zmm` llevan datos floating-point y SIMD. Esta nota se enfoca en
los GPRs porque son lo primero que necesitás al leer código entero y de punteros.

## Anchos y aliases

Cada GPR x86-64 tiene nombres solapados en anchos más chicos. Son aliases del mismo
registro físico, no almacenamiento separado:

| 64-bit | 32-bit | 16-bit | 8-bit low |
|---|---|---|---|
| `rax` | `eax` | `ax` | `al` |
| `rbx` | `ebx` | `bx` | `bl` |
| `rcx` | `ecx` | `cx` | `cl` |
| `rdx` | `edx` | `dx` | `dl` |
| `rdi` | `edi` | `di` | `dil` |
| `rsi` | `esi` | `si` | `sil` |
| `r8` | `r8d` | `r8w` | `r8b` |

El borde filoso es el ancho de escritura. Una escritura de 32 bits como `mov eax, ...`
o `xor eax, eax` zero-extiende al registro completo de 64 bits, así que la mitad alta
de `rax` queda en cero. Una escritura de 8 o 16 bits como `mov al, ...` o
`mov ax, ...` cambia solo ese slice y deja intactos los bits altos. Por eso x86-64
optimizado muchas veces usa `eax` aunque el código alrededor después lea `rax`:
escribir `eax` es una forma barata de producir un valor de 64 bits conocido y limpio.

`rsp` merece su propia advertencia. Tiene forma de GPR, pero apunta al tope del stack.
Tratarlo como scratch corrompe el call stack, return addresses, almacenamiento local y
reglas de alineación de la ABI. Cuando ves `add rsp, 48` o `sub rsp, 48`, leé eso como
bookkeeping del stack frame, no como aritmética ordinaria.

## Trazá una función real

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/x86-64-registers-and-the-register-file/`. La
función está diseñada para hacer visibles los seis registros de argumento enteros:

```c
long mix_six(long a, long b, long c, long d, long e, long f) {
    long acc = a + b * 2;
    acc ^= c;
    acc += d * 4;
    acc -= e;
    return acc + f;
}
```

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Este es el cuerpo
relevante de la función copiado de la salida real de:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_mix_six:                               ## @mix_six
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rdi + 2*rsi]
	xor	rax, rdx
	lea	rax, [rax + 4*rcx]
	sub	rax, r8
	add	rax, r9
	pop	rbp
	ret
	.cfi_endproc
```

Leelo como flujo de registros:

- `rdi` es `a`, `rsi` es `b`, `rdx` es `c`, `rcx` es `d`, `r8` es `e` y `r9` es `f`.
- `lea rax, [rdi + 2*rsi]` calcula `a + b * 2` y pone el valor corriente en `rax`.
- `xor rax, rdx` aplica `^ c`; `rdx` es una entrada, no un registro mágico de
  multiplicación/división en este contexto.
- `lea rax, [rax + 4*rcx]` suma `d * 4` manteniendo el resultado en `rax`.
- `sub rax, r8` y `add rax, r9` consumen el quinto y sexto registro de argumento.
- `ret` retorna con el resultado ya en `rax`.

Ese es todo el hábito de lectura de registros de
[[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Compiler Explorer como herramienta diaria]]: encontrá la función, mapeá los registros
de la ABI y después seguí el valor que se vuelve retorno.

## Register pressure y spills

El register file es rápido porque es chico. Cuando el compilador tiene más valores
vivos que registros, **derrama** (spill) algunos valores a memoria, normalmente dentro
del stack frame actual, y después los recarga. Esto no es "el compilador siendo tonto";
es el presupuesto finito de registros haciéndose visible.

Vas a ver pressure cuando:

- muchos valores deben seguir vivos durante una expresión o loop largo,
- una llamada a función obliga a preservar registros caller-saved en algún lado,
- el código vectorial compite por registros SIMD,
- la salida debug-friendly de `-O0` mantiene variables con forma de source en slots de
  stack, o
- inline assembly le dice al compilador que muchos registros quedan clobbered.

La regla de decisión es simple: al leer assembly optimizado, el movimiento entre
registros es normal; el tráfico repetido a memoria dentro de un hot loop es lo que
conviene cuestionar.

## Apéndice ARM64

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Cuerpo relevante de la función:

```asm
_mix_six:                               ; @mix_six
	.cfi_startproc
; %bb.0:
	add	x8, x0, x1, lsl #1
	eor	x8, x8, x2
	add	x8, x8, x3, lsl #2
	sub	x8, x8, x4
	add	x0, x8, x5
	ret
	.cfi_endproc
```

AArch64 tiene un set de GPRs más grande y más regular: `x0`–`x30` son registros de 64
bits, con `w0`–`w30` como sus vistas de 32 bits. Los argumentos enteros llegan en
`x0`–`x7`, y los valores de retorno salen en `x0`. Acá el compilador usa `x8` como
acumulador scratch y mueve el resultado final a `x0` con el último `add`.

El contraste es útil: la misma función C que usó `rdi`, `rsi`, `rdx`, `rcx`, `r8`,
`r9` y `rax` en x86-64 usa `x0`–`x5` más `x8` en ARM64. El significado de alto nivel
es estable; el register file y la calling convention son específicos del target.

## Modos de falla y trade-offs

- **La ABI importa.** System V AMD64 usa `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9` para
  los primeros seis argumentos enteros/puntero. Windows x64 usa otro orden y menos
  registros de argumento (`rcx`, `rdx`, `r8`, `r9`). Misma familia de ISA, ABI distinta.
- **Los registros no equivalen a variables.** Un registro puede tener distintos valores
  de source en momentos distintos, y un valor de source puede moverse por varios
  registros o no existir nunca como registro.
- **Las escrituras parciales muerden.** Las escrituras de 32 bits zero-extienden; las de
  8/16 bits no. Assembly escrito a mano e inline assembly tienen que ser explícitos
  sobre qué ancho están definiendo.
- **`rsp` no es almacenamiento libre.** Si corrompés el stack pointer, `ret`, variables
  locales, registros salvados y supuestos de alineación pueden romperse todos a la vez.
- **Caller-saved vs callee-saved es un contrato.** Una función puede clobber registros
  caller-saved libremente, pero debe restaurar los callee-saved antes de retornar.
  Inline assembly tiene que decirle al compilador qué clobber.
- **`-O0` miente sobre pressure.** Los builds de debug suelen guardar cada variable con
  forma de source en el stack. Usá `-O2` cuando preguntás cómo el código optimizado usa
  el register file.

## En la práctica

- **Anotá primero los registros de argumento.** En System V x86-64, escribí mentalmente
  `a/b/c/d/e/f` sobre `rdi/rsi/rdx/rcx/r8/r9` antes de decodificar cada instrucción.
- **Seguí un valor vivo.** Rastreá el valor que termina en `rax`; eso muchas veces
  explica toda la función.
- **Tratá `rbp`/`rsp` como maquinaria de frame.** Describen dónde está el almacenamiento
  de stack, no el cómputo de negocio.
- **Mirá spills en hot loops.** Loads/stores a `[rsp + ...]` o `[rbp - ...]` dentro de
  un loop pueden significar register pressure, preservación por ABI o código orientado
  a debug.
- **Tené cerca la nota de machine-model.** Esta nota es la versión mirando assembly de
  [[lowlevel/machine-model/registers-and-the-isa|registros y la ISA]]: el mismo
  almacenamiento finito, ahora con roles de ABI y salida real del compilador pegados.

**Conecta con:** [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Por qué leer assembly — Compiler Explorer como herramienta diaria]] · [[lowlevel/machine-model/registers-and-the-isa|Registros y la ISA]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|La CPU: fetch–decode–execute]] · [[lowlevel/machine-model/how-source-becomes-execution|Cómo el source se vuelve ejecución]] · [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero]]

## Fuentes

- **System V AMD64 ABI** — la calling convention Unix x86-64: registros de argumento, registros de retorno y reglas caller-saved/callee-saved. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 — modelo autoritativo de registros x86-64, comportamiento de sub-registros, `rip`, `rflags` y familias de registros SIMD. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Felix Cloutier — x86 and amd64 instruction reference** — lookup rápido de instrucciones como `lea`, `xor`, `sub`, `add` y `ret`; verificá contra Intel SDM cuando la precisión importa. https://www.felixcloutier.com/x86/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3 — representación a nivel máquina de programas C y dataflow a nivel registros. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — referencia accesible de registros e instrucciones x86-64 con ejemplos ejecutables. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Compiler Explorer** — feedback rápido para revisar cómo un compilador real asigna registros bajo flags y targets concretos. https://godbolt.org/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — semántica AArch64 de registros más detalles de ABI en plataformas Apple para la salida de `clang -arch arm64`. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
