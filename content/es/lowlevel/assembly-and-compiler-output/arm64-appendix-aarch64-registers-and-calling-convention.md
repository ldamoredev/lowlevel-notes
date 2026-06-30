---
title: "Apéndice ARM64: registros AArch64 y calling convention"
description: AArch64 tiene registros x0-x30, vistas w de 32 bits, paso de argumentos x0-x7, x29 como frame pointer, x30 como link register y sp como stack pointer. Aprendé la forma de llamada Apple ARM64 desde salida real.
tags: [assembly, arm64, aarch64, calling-convention, registers]
order: 12
updated: 2026-06-30
---
# Apéndice ARM64: registros AArch64 y calling convention

AArch64 es el instruction set ARM de 64 bits que vas a ver en Apple Silicon y muchos
servidores modernos. Sigue siendo una máquina de registros, pero su vocabulario es más
regular que x86-64: los registros de propósito general son `x0` a `x30`, las vistas de
32 bits son `w0` a `w30`, los argumentos suelen arrancar en `x0`, y la return address
vive en el link register `x30` hasta que haga falta spillar. Si x86-64 se siente
históricamente estratificado, ARM64 se siente deliberadamente regular.

> El reset: en AArch64, `x0` y `w0` son dos vistas del mismo registro. `x0` es de 64 bits;
> `w0` es la vista baja de 32 bits, y escribir `w0` pone en cero la mitad alta de `x0`.

## Cómo funciona realmente

Los registros que leés primero:

| Registro | Rol |
|---|---|
| `x0`-`x7` | primeros argumentos enteros/punteros y retorno en `x0` |
| `w0`-`w7` | vistas bajas de 32 bits de `x0`-`x7` |
| `x8`-`x15` | temporales comunes y roles scratch/indirect-result según ABI |
| `x19`-`x28` | registros generales callee-saved |
| `x29` | frame pointer (`fp`) por convención |
| `x30` | link register (`lr`), contiene return address después de `bl` |
| `sp` | stack pointer |

Las llamadas usan `bl target`: branch with link. Salta a `target` y escribe la return
address en `x30`. Una función non-leaf normalmente guarda `x29` y `x30` en su stack
frame:

```asm
stp	x29, x30, [sp, #offset]
add	x29, sp, #offset
...
ldp	x29, x30, [sp, #offset]
ret
```

A diferencia de `call` en x86-64, `bl` no pushea la return address al stack por sí solo.
El callee spilla `x30` solo cuando necesita preservarlo a través de otra llamada.

## Una llamada AArch64 real con argumentos en stack

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/arm64-appendix-aarch64-registers-and-calling-convention/`.

```c
NOINLINE long arm64_mix(long a, long b, long c, long d, long e,
                        long f, long g, long h, long i, long j) {
    return a + 2 * b - c + 3 * d + e - 4 * f + g + h - i + j;
}
```

La función tiene diez argumentos enteros. Apple ARM64 pasa los primeros ocho en `x0` a
`x7`; el noveno y décimo se pasan por stack.

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Callee relevante:

```asm
_arm64_mix:                             ; @arm64_mix
	.cfi_startproc
; %bb.0:
	ldp	x9, x8, [sp]
	add	x10, x0, x1, lsl #1
	add	x11, x3, x3, lsl #1
	sub	x10, x10, x2
	add	x10, x10, x11
	add	x10, x10, x4
	sub	x10, x10, x5, lsl #2
	add	x10, x10, x6
	add	x10, x10, x7
	sub	x9, x10, x9
	add	x0, x9, x8
	ret
	.cfi_endproc
```

Mapealo:

- `a` a `h` llegan en `x0` a `x7`.
- `ldp x9, x8, [sp]` carga `i` y `j` desde los primeros dos slots de argumentos en stack.
- `x10` y `x11` son scratch registers elegidos por el compilador.
- `add x10, x0, x1, lsl #1` computa `a + 2*b`.
- `sub x10, x10, x5, lsl #2` resta `4*f`.
- `add x0, x9, x8` escribe el valor final de retorno en `x0`.
- `ret` vuelve a la dirección que está en `x30`.

El call site muestra el lado del caller:

```asm
	ldur	x0, [x29, #-8]
	ldur	x1, [x29, #-16]
	ldur	x2, [x29, #-24]
	ldur	x3, [x29, #-32]
	ldur	x4, [x29, #-40]
	ldr	x5, [sp, #48]
	ldr	x6, [sp, #40]
	ldr	x7, [sp, #32]
	ldr	x8, [sp, #24]
	ldr	x9, [sp, #16]
	stp	x8, x9, [sp]
	bl	_arm64_mix
```

El caller carga argumentos uno a ocho en `x0`-`x7`, ubica argumentos nueve y diez en
`[sp]` con `stp`, y llama con `bl`. Después de la llamada, el retorno está en `x0`.

## Stack frames y link register

`main` es non-leaf porque llama a `arm64_mix` y `printf`, así que guarda frame pointer y
link register:

```asm
	sub	sp, sp, #112
	stp	x29, x30, [sp, #96]
	add	x29, sp, #96
	...
	ldp	x29, x30, [sp, #96]
	add	sp, sp, #112
	ret
```

Las instrucciones pair store/load son idiomáticas en ARM64. `stp` guarda un par de
registros; `ldp` carga un par. Guardar `x30` es lo que permite que `main` sobreviva
llamadas anidadas: cada `bl` sobrescribe `x30`, así que las funciones non-leaf preservan
el viejo link register en un lugar seguro.

## Comparación con x86-64

El mismo C compilado para x86-64 System V usa otro contrato:

```asm
_arm64_mix:                             ## @arm64_mix
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rsi, [rdi + 2*rsi]
	sub	rsi, rdx
	lea	rax, [rcx + 2*rcx]
	add	rax, rsi
	add	rax, r8
	shl	r9, 2
	sub	rax, r9
	add	rax, qword ptr [rbp + 16]
	add	rax, qword ptr [rbp + 24]
	sub	rax, qword ptr [rbp + 32]
	add	rax, qword ptr [rbp + 40]
	pop	rbp
	ret
	.cfi_endproc
```

x86-64 System V tiene seis registros de argumentos enteros, así que argumentos siete a
diez se leen desde slots de stack. ARM64 tenía ocho registros de argumentos y solo usó el
stack para argumentos nueve y diez. El C era idéntico; cambió la frontera ABI.

## Modos de falla y trade-offs

- **Mezclar `x` y `w` sin cuidado.** Escribir `w0` pone en cero los 32 bits altos de
  `x0`. Es útil, pero sorprende si esperabas una escritura parcial.
- **Olvidar que `x30` es volátil a través de calls.** Cada `bl` sobrescribe el link
  register. Las funciones non-leaf deben preservarlo si quieren volver correctamente.
- **Tratar `sp` como registro general.** AArch64 tiene restricciones alrededor de `sp` en
  algunas formas de instrucción y exige alineación del stack.
- **Asumir hábitos de stack x86.** ARM64 no pushea return address con `bl`; usa link
  register.
- **Ignorar diferencias de ABI de plataforma.** Linux AArch64 y Apple ARM64 son cercanas,
  pero las ABIs de plataforma definen detalles como red zones, registros reservados y
  varargs.
- **Leer `csel` como branch.** Conditional select computa dataflow sin cambiar control
  flow.

## En la práctica

- **Mapeá `x0`-`x7` primero.** Para funciones chicas, casi toda la historia de argumentos
  está en esos ocho registros.
- **Mirá argumentos de stack en `[sp]`.** Cuando los argumentos exceden la ventana de
  registros, el caller pone el resto en el stack.
- **Seguí `x0` para leer retornos.** Los retornos enteros/punteros salen en `x0`.
- **Buscá pares `x29`/`x30`.** `stp x29, x30` y `ldp x29, x30` son setup y teardown de
  frame para funciones non-leaf.
- **Usá salida ARM64 como segunda opinión.** Compararla con x86-64 suele aclarar qué vino
  de C y qué vino de la ABI target.

**Conecta con:** [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|Registros x86-64 y el register file]] · [[lowlevel/assembly-and-compiler-output/system-v-amd64-calling-convention|La calling convention System V AMD64]] · [[lowlevel/assembly-and-compiler-output/stack-frames-function-prologue-and-epilogue|Stack frames: prólogo y epílogo de función]] · [[lowlevel/assembly-and-compiler-output/how-c-constructs-compile-loops-structs-switch|Cómo compilan las construcciones de C: loops, structs, switch]] · [[lowlevel/machine-model/registers-and-the-isa|Registros y la ISA]]

## Fuentes

- **Apple — Writing ARM64 code for Apple platforms** — calling convention AArch64 en plataformas Apple, roles de registros, alineación de stack y notas de ABI. https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
- **Arm Architecture Reference Manual for A-profile architecture** — semántica autoritativa de instrucciones y registros AArch64. https://developer.arm.com/documentation/ddi0487/latest
- **Arm Procedure Call Standard for AArch64 (AAPCS64)** — reglas de procedure call, clases de registros, paso de argumentos, retornos y restricciones de stack. https://github.com/ARM-software/abi-aa
- **Stephen Smith — *Programming with 64-Bit ARM Assembly Language*** — ejemplos prácticos de assembly AArch64 y explicaciones de calling convention. https://link.springer.com/book/10.1007/978-1-4842-5881-1
- **System V AMD64 ABI** — fuente de contraste para la comparación x86-64 de esta nota. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Compiler Explorer (Godbolt)** — comparación rápida cross-target del mismo source C bajo compiladores x86-64 y ARM64. https://godbolt.org/
