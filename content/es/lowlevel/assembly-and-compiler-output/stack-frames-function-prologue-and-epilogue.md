---
title: "Stack frames: prólogo y epílogo de función"
description: Un stack frame es la porción de stack storage por llamada que usa una función. Aprendé prólogo, epílogo, frame pointer, slots locales, calls y metadata de unwind desde salida real del compilador.
tags: [assembly, x86-64, stack-frame, prologue, epilogue]
order: 7
updated: 2026-06-30
---
# Stack frames: prólogo y epílogo de función

Un stack frame es la parte del stack que pertenece a una llamada activa de función. Ahí
una función puede guardar el frame pointer del caller, reservar storage local, spillar
registros y preservar el estado que debe restaurar antes de volver. El prólogo construye
ese frame; el epílogo lo desarma. Leer esos dos bordes te permite separar mecánica de
call frame del cálculo real.

> El reset: un stack frame no es un objeto de C. Es una región de memoria con forma de
> ABI alrededor de `rsp`, muchas veces anclada por `rbp`, que existe solo durante una
> llamada dinámica.

## Cómo funciona realmente

La forma clásica x86-64 con frame pointer es:

```asm
push    rbp
mov     rbp, rsp
sub     rsp, N
```

Y antes de volver:

```asm
add     rsp, N
pop     rbp
ret
```

Leelo como un protocolo chico:

| Instrucción | Rol |
|---|---|
| `push rbp` | guarda el frame pointer del caller |
| `mov rbp, rsp` | crea una base estable para este frame |
| `sub rsp, N` | reserva `N` bytes de espacio local/spill |
| `add rsp, N` | libera ese espacio local/spill |
| `pop rbp` | restaura el frame pointer del caller |
| `ret` | popea la return address hacia `rip` |

El compilador puede omitir parte de esto en código optimizado. Una leaf function mínima
puede no necesitar storage de stack. Una función compilada omitiendo frame pointer puede
usar `rsp` directamente y describir unwinding con metadata. Pero cuando ves esta forma,
estás viendo el contrato de entrada y salida de la función escrito en instrucciones.

## Un prólogo y epílogo reales

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/stack-frames-function-prologue-and-epilogue/`.

```c
static NOINLINE long add_bias(long value) {
    return value + 7;
}

long framed_total(long a, long b) {
    volatile long local = a + b;
    long adjusted = add_bias(local);

    return adjusted + local;
}
```

`volatile` fuerza un slot visible de stack para `local`. La llamada a `add_bias` evita
que toda la función colapse en una sola expresión aritmética.

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Este es el cuerpo
relevante de la función copiado de la salida real de:

```sh
gcc -S -O2 -fno-omit-frame-pointer -masm=intel demo.c -o demo.s
```

```asm
_framed_total:                          ## @framed_total
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	sub	rsp, 16
	add	rdi, rsi
	mov	qword ptr [rbp - 8], rdi
	mov	rdi, qword ptr [rbp - 8]
	call	_add_bias
	add	rax, qword ptr [rbp - 8]
	add	rsp, 16
	pop	rbp
	ret
	.cfi_endproc
```

Mapeá primero los argumentos: `a` llega en `rdi`, `b` llega en `rsi`, y el valor de
retorno sale en `rax`.

Ahora separá mecánica de negocio:

- `push rbp; mov rbp, rsp` establece un frame pointer visible.
- `sub rsp, 16` reserva espacio de stack. Acá se ven 8 bytes usados, pero las reglas de
  alineación de la ABI hacen que 16 sea una reserva natural.
- `add rdi, rsi` computa `a + b`.
- `mov qword ptr [rbp - 8], rdi` guarda `local` en el frame actual.
- `mov rdi, qword ptr [rbp - 8]` recarga `local` como primer argumento de `add_bias`.
- `call _add_bias` pushea una return address y transfiere control.
- `add rax, qword ptr [rbp - 8]` suma `local` al valor devuelto por el helper.
- `add rsp, 16; pop rbp; ret` libera el frame y vuelve al caller.

Las directivas `.cfi_*` no son instrucciones de CPU. Son metadata de unwind para
debuggers, profilers, maquinaria de excepciones y stack walkers. Describen dónde está la
canonical frame address y los registros guardados mientras la función corre.

El helper tiene un frame más chico porque no tiene slot local de stack:

```asm
_add_bias:                              ## @add_bias
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rdi + 7]
	pop	rbp
	ret
	.cfi_endproc
```

El patrón es el mismo, pero el medio es solo `lea rax, [rdi + 7]`. No aparece `sub rsp`
porque no hay storage local de stack para reservar.

## Frame pointer versus stack pointer

`rsp` es el tope móvil del stack. Cambia cuando la función reserva storage, pushea
valores temporales, llama a otra función o libera storage. `rbp` es un ancla estable
opcional. Con frame pointer, un local como `[rbp - 8]` conserva la misma dirección
aunque `rsp` cambie después.

Por eso los debug builds suelen preservar `rbp`: recorrer el stack e inspeccionar locals
es directo. Los optimized builds muchas veces lo omiten para liberar otro registro
general y se apoyan en unwind tables. El código sigue siendo válido; solo es menos
prolijo visualmente.

## Apéndice ARM64

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Cuerpo relevante de la función:

```asm
_framed_total:                          ; @framed_total
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #32
	.cfi_def_cfa_offset 32
	stp	x29, x30, [sp, #16]             ; 16-byte Folded Spill
	add	x29, sp, #16
	.cfi_def_cfa w29, 16
	.cfi_offset w30, -8
	.cfi_offset w29, -16
	add	x8, x1, x0
	str	x8, [sp, #8]
	ldr	x0, [sp, #8]
	bl	_add_bias
	ldr	x8, [sp, #8]
	add	x0, x8, x0
	ldp	x29, x30, [sp, #16]             ; 16-byte Folded Reload
	add	sp, sp, #32
	ret
	.cfi_endproc
```

AArch64 usa `sp` como stack pointer, `x29` como frame pointer convencional y `x30` como
link register. `bl _add_bias` escribe la return address en `x30`, así que una función
que llama a otra normalmente guarda `x29` y `x30` juntos con `stp`. El epílogo recarga
ambos con `ldp`, libera stack y vuelve con `ret`.

La misma idea está visible, pero cambia el vocabulario: x86-64 usa `call` para pushear
una return address al stack; ARM64 usa link register y lo spilla cuando hace falta.

## Modos de falla y trade-offs

- **Asumir que toda función tiene frame visible.** Leaf functions optimizadas pueden no
  tener prólogo, y omitir frame pointer elimina la cadena prolija de `rbp`.
- **Olvidar la alineación del stack.** El monto de `sub rsp, N` no habla solo de locals
  con nombre. También mantiene la alineación que esperan la ABI y las llamadas.
- **Confundir `.cfi_*` con instrucciones ejecutables.** Las directivas CFI importan para
  unwinding, pero no corren en la CPU como parte del cuerpo de la función.
- **Ignorar salidas alternativas.** Funciones reales pueden tener múltiples returns,
  caminos de error o tailcalls. Cada camino debe dejar stack y registros callee-saved en
  estado válido.
- **Usar red zone donde no corresponde.** System V AMD64 user code puede usar red zone,
  pero kernels y código con interrupciones normalmente no pueden depender de eso.
- **Tratar bytes de stack como lifetime segura.** Cuando la función vuelve, su frame ya
  no te pertenece. Los punteros hacia ahí quedan dangling.

## En la práctica

- **Encontrá primero el prólogo.** Te dice si hay frame pointer y cuánto stack se reserva.
- **Nombrá slots locales por offset.** `[rbp - 8]` es un slot, no un nombre de variable.
  El nombre mental sale de stores y loads cercanos.
- **Separá bookkeeping de ABI del cálculo.** `push`, `mov rbp, rsp`, `sub rsp`, CFI y el
  epílogo son mecánica de frame.
- **Chequeá calls antes de returns.** Un `call` real pushea una return address y hace que
  la alineación del stack importe.
- **Leelo junto al modelo crudo de stack.** El frame es la versión estructurada de las
  operaciones de [[lowlevel/assembly-and-compiler-output/stack-at-the-assembly-level|stack a nivel assembly]].

**Conecta con:** [[lowlevel/assembly-and-compiler-output/stack-at-the-assembly-level|El stack a nivel assembly]] · [[lowlevel/assembly-and-compiler-output/system-v-amd64-calling-convention|La calling convention System V AMD64]] · [[lowlevel/assembly-and-compiler-output/control-flow-jumps-conditions-and-flags|Control flow: jumps, condiciones y el registro de flags]] · [[lowlevel/pointers-and-memory/the-stack-frames-locals-and-automatic-storage|El stack: frames y automatic storage]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]]

## Fuentes

- **System V AMD64 ABI** — alineación de stack, reglas de call frame, red zone, register saving y expectativas de unwind. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 y Vol. 2 — semántica de `push`, `pop`, `call`, `ret`, addressing de stack e instrucciones relacionadas con frames. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3 — llamadas a procedimiento, stack frames, register saving y C a nivel máquina. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — ejemplos accesibles de stack frames x86-64. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Felix Cloutier — x86 and amd64 instruction reference** — lookup rápido de `push`, `pop`, `call`, `ret` y `leave`; verificá edge cases contra Intel SDM. https://www.felixcloutier.com/x86/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — `sp`, `x29`, `x30`, `bl`, `ret` y detalles de calling convention de plataformas Apple para la salida del apéndice. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
