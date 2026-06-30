---
title: "La calling convention System V AMD64"
description: La ABI System V AMD64 define cómo las funciones x86-64 pasan argumentos, devuelven valores, alinean el stack y preservan registros. Aprendé el mapa de registros y argumentos en stack desde salida real del compilador.
tags: [assembly, x86-64, abi, calling-convention, system-v]
order: 8
updated: 2026-06-30
---
# La calling convention System V AMD64

Una calling convention es el contrato que permite que funciones compiladas por separado
se llamen entre sí. Dice dónde van los argumentos, por dónde vuelven los valores, qué
registros debe preservar el callee, cómo se alinea el stack y quién limpia argumentos
pasados por stack. La ABI System V AMD64 es la convención dominante para targets x86-64
tipo Unix. Sin eso, `gcc`, `clang`, libc, debuggers y tu assembly escrito a mano no
estarían de acuerdo en cómo funciona una llamada.

> El reset: una llamada a función no es "pasar variables". Es un protocolo de registros
> y stack. Los nombres de parámetros de C ya no existen cuando el callee empieza a correr.

## Cómo funciona realmente

Para argumentos enteros y punteros, los primeros seis argumentos llegan en este orden:

| Argumento | Registro |
|---|---|
| 1 | `rdi` |
| 2 | `rsi` |
| 3 | `rdx` |
| 4 | `rcx` |
| 5 | `r8` |
| 6 | `r9` |

Los argumentos enteros/punteros adicionales se pasan por stack. Los retornos enteros
vuelven en `rax`. Los argumentos floating-point y vectoriales tienen sus propias reglas
mediante registros `xmm` y lógica de clasificación; esta nota se queda en el camino
entero porque es el primero que leés todo el tiempo.

La separación de register saving importa:

| Clase de registro | Significado |
|---|---|
| caller-saved | el caller debe guardarlo si lo necesita después de una llamada |
| callee-saved | el callee debe restaurarlo antes de volver si lo modifica |

Registros enteros caller-saved comunes: `rax`, `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`,
`r10` y `r11`. Registros callee-saved comunes: `rbx`, `rbp` y `r12` a `r15`. `rsp` debe
quedar restaurado por el callee.

## Una llamada real con ocho argumentos

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/system-v-amd64-calling-convention/`.

```c
static NOINLINE long abi_mix(long a, long b, long c, long d,
                             long e, long f, long g, long h) {
    return a + 2 * b - c + 3 * d + e - 4 * f + g + h;
}
```

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Este es el call site
relevante copiado de la salida real de:

```sh
gcc -S -O2 -fno-omit-frame-pointer -masm=intel demo.c -o demo.s
```

```asm
	mov	rdi, qword ptr [rbp - 64]
	mov	rsi, qword ptr [rbp - 56]
	mov	rdx, qword ptr [rbp - 48]
	mov	rcx, qword ptr [rbp - 40]
	mov	r8, qword ptr [rbp - 32]
	mov	r9, qword ptr [rbp - 24]
	push	qword ptr [rbp - 8]
	push	qword ptr [rbp - 16]
	call	_abi_mix
	add	rsp, 16
```

Los primeros seis argumentos se cargan en `rdi`, `rsi`, `rdx`, `rcx`, `r8` y `r9`. Los
argumentos siete y ocho se pushean al stack antes de `call`. Los pushes aparecen en
orden inverso porque las direcciones del stack crecen hacia abajo: después de ambos
pushes, el argumento siete queda más cerca de la return address en la posición que el
callee espera. Después de la llamada, `add rsp, 16` saca esos dos argumentos de 8 bytes
del stack del caller.

Dentro del callee:

```asm
_abi_mix:                               ## @abi_mix
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
	pop	rbp
	ret
	.cfi_endproc
```

El callee lee `a` a `f` desde registros. Los argumentos séptimo y octavo están en
offsets positivos desde `rbp`: `[rbp + 16]` y `[rbp + 24]`. Con esta forma de frame,
`[rbp]` contiene el `rbp` guardado del caller, `[rbp + 8]` contiene la return address, y
los dos slots siguientes contienen argumentos pasados por stack.

El retorno queda en `rax`. Por eso el caller puede pasarlo enseguida a `printf` moviendo
`rax` al segundo registro de argumento de la siguiente llamada:

```asm
	lea	rdi, [rip + L_.str]
	mov	rsi, rax
	xor	eax, eax
	call	_printf
```

El `xor eax, eax` antes de `printf` también viene de la ABI: para funciones variádicas,
System V AMD64 usa `al` para comunicar cuántos registros vectoriales se usaron para
varargs floating-point. Acá no hay ninguno, así que el compilador lo setea a cero.

## Alineación y ownership del stack

La ABI también exige alineación del stack en fronteras de llamada. Por eso muchas veces
ves ajustes de stack que parecen "extra". El compilador no solo reserva locals; mantiene
la alineación que esperan el código llamado, spills, instrucciones vectoriales y rutinas
de biblioteca.

Las responsabilidades se dividen así:

| Responsabilidad | Dueño |
|---|---|
| ubicar argumentos | caller |
| pushear return address | instrucción `call` |
| preservar registros callee-saved | callee |
| limpiar argumentos de stack en esta ABI | caller |
| ubicar retorno entero | callee, en `rax` |

Una vez que sabés el contrato, los listings de assembly dejan de parecer arbitrarios.
Las elecciones de registros no son caprichos; son la ABI.

## Apéndice ARM64

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Callee ARM64 relevante:

```asm
_abi_mix:                               ; @abi_mix
	.cfi_startproc
; %bb.0:
	add	x8, x0, x1, lsl #1
	add	x9, x3, x3, lsl #1
	sub	x8, x8, x2
	add	x8, x8, x9
	add	x8, x8, x4
	sub	x8, x8, x5, lsl #2
	add	x8, x8, x6
	add	x0, x8, x7
	ret
	.cfi_endproc
```

Apple ARM64 pasa los primeros ocho argumentos enteros/punteros en `x0` a `x7`, y devuelve
enteros en `x0`. Este ejemplo tiene exactamente ocho argumentos, así que ARM64 no necesita
argumentos en stack. El contraste con System V AMD64 sirve: x86-64 tenía seis registros de
argumentos enteros y usó stack para `g` y `h`; ARM64 mantuvo los ocho en registros.

El call site muestra esa carga de registros directamente:

```asm
	ldur	x0, [x29, #-8]
	ldur	x1, [x29, #-16]
	ldur	x2, [x29, #-24]
	ldur	x3, [x29, #-32]
	ldr	x4, [sp, #40]
	ldr	x5, [sp, #32]
	ldr	x6, [sp, #24]
	ldr	x7, [sp, #16]
	bl	_abi_mix
```

`bl` branch with link: transfiere control y escribe la return address en `x30`.

## Modos de falla y trade-offs

- **Usar la ABI equivocada.** Windows x64, System V AMD64 y Apple ARM64 no usan los
  mismos registros de argumentos ni reglas de stack. El assembly manual debe apuntar a la
  ABI correcta.
- **Olvidar registros caller-saved.** Si necesitás `rax`, `rdi`, `rsi`, `rdx`, `rcx`,
  `r8`, `r9`, `r10` o `r11` después de una llamada, guardalos vos.
- **Olvidar registros callee-saved.** Si tu función assembly modifica `rbx`, `rbp` o
  `r12` a `r15`, restauralos antes de volver.
- **Desalinear el stack.** El código de biblioteca puede asumir alineación ABI. Una mala
  secuencia manual de call puede crashear lejos del error.
- **Ignorar reglas de funciones variádicas.** Llamadas como `printf` tienen detalles
  extra de ABI, como el conteo de registros vectoriales en `al` en System V AMD64.
- **Asumir que sobreviven nombres de parámetros.** Assembly tiene registros, slots de
  stack y metadata de debug. La máquina no conoce nombres de C como `a` o `g`.

## En la práctica

- **Arrancá por el mapa de registros.** Antes de leer una función, asigná `rdi`, `rsi`,
  `rdx`, `rcx`, `r8` y `r9` a parámetros fuente.
- **Buscá argumentos de stack arriba de la return address.** Con frame pointer, argumentos
  enteros pasados por stack suelen aparecer como `[rbp + 16]`, `[rbp + 24]`, etc.
- **Tratá las llamadas como puntos de clobber.** Los registros caller-saved pueden cambiar
  a través de una llamada.
- **Leé `rax` como camino de retorno.** Los valores enteros vuelven por `rax` en System V
  AMD64.
- **Mantenelo junto a stack frames.** La calling convention explica por qué prólogos,
  epílogos, alineación de stack y registros guardados tienen las formas que tienen.

**Conecta con:** [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|Registros x86-64 y el register file]] · [[lowlevel/assembly-and-compiler-output/stack-frames-function-prologue-and-epilogue|Stack frames: prólogo y epílogo de función]] · [[lowlevel/assembly-and-compiler-output/stack-at-the-assembly-level|El stack a nivel assembly]] · [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|El core instruction set: mov, aritmética, lea]] · [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]]

## Fuentes

- **System V AMD64 ABI** — calling convention autoritativa para paso de argumentos, retornos, alineación de stack, clases de registros y detalles de varargs. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 y Vol. 2 — semántica de instrucciones para `call`, `ret`, operaciones de stack y comportamiento de registros enteros. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3 — llamadas a procedimiento, uso de registros, stack frames y ejemplos de C a nivel máquina. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — ejemplos legibles de llamadas a función x86-64 y argumentos pasados por stack. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Agner Fog — Calling conventions for different C++ compilers and operating systems** — comparación práctica de ABIs entre plataformas. https://www.agner.org/optimize/calling_conventions.pdf
- **Apple — Writing ARM64 code for Apple platforms** — detalles de calling convention Apple ARM64 para el contraste del apéndice. https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
