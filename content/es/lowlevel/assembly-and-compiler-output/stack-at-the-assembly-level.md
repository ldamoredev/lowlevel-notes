---
title: "El stack a nivel assembly (`push`/`pop`, `rsp`/`rbp`)"
description: El stack es una región que crece hacia abajo y se administra con rsp, con rbp muchas veces como frame pointer. Aprendé push/pop, stack slots, red zone y cómo se ve un call frame real en salida del compilador.
tags: [assembly, x86-64, stack, rsp, rbp]
order: 6
updated: 2026-06-30
---
# El stack a nivel assembly (`push`/`pop`, `rsp`/`rbp`)

El stack es memoria común con una convención fuerte: crece hacia abajo, `rsp` nombra
el tope actual, y las llamadas a funciones lo usan para return addresses, registros
guardados y storage temporal que entra en un call frame. `rbp` muchas veces es un frame
pointer: un ancla estable que hace fácil nombrar slots de stack mientras `rsp` se
mueve. Cuando podés leer `[rbp - 32]`, `push rbp`, `pop rbp` y `ret`, los stack frames
dejan de ser una abstracción del debugger y pasan a ser estado de máquina visible.

> El reset: el stack no es storage mágico. Es memoria direccionada por registros. Una
> "variable local" en assembly muchas veces son bytes en un offset desde `rsp` o `rbp`.

## Cómo funciona realmente

En x86-64, el stack pointer arquitectónico es `rsp`. Por convención, el stack crece
hacia direcciones menores:

```asm
push    rbp     ; rsp -= 8; [rsp] = viejo rbp
pop     rbp     ; rbp = [rsp]; rsp += 8
```

Las llamadas a función también usan el stack:

| Instrucción | Efecto sobre el stack |
|---|---|
| `call target` | pushea la return address y después salta a `target` |
| `ret` | popea una return address hacia `rip` |
| `push reg` | resta 8 a `rsp`, guarda `reg` |
| `pop reg` | carga desde `[rsp]`, suma 8 a `rsp` |

Un prólogo clásico con frame pointer es:

```asm
push    rbp
mov     rbp, rsp
sub     rsp, 32
```

Después de eso, los locals suelen estar en offsets negativos desde `rbp`, mientras que
argumentos y estado guardado del caller están en offsets positivos o en registros según
la ABI. En optimizaciones altas, los compiladores pueden omitir `rbp`, usar solo
`rsp`, mantener locals en registros o usar la red zone. La idea fuente "esta función
tiene locals" no fuerza una única forma de stack.

## Una función real con forma de stack

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/stack-at-the-assembly-level/`.

```c
static NOINLINE long combine_slots(long first, long second, long sum, long diff) {
    return first + second + sum + diff;
}

long stack_example(long a, long b) {
    volatile long first = a;
    volatile long second = b;
    volatile long sum = a + b;
    volatile long diff = a - b;

    return combine_slots(first, second, sum, diff);
}
```

Los locals `volatile` están para enseñar: fuerzan al compilador a materializar slots de
stack en vez de mantener todo en registros.

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Este es el cuerpo
relevante de la función copiado de la salida real de:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_stack_example:                         ## @stack_example
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	qword ptr [rbp - 32], rdi
	mov	qword ptr [rbp - 24], rsi
	lea	rax, [rsi + rdi]
	mov	qword ptr [rbp - 16], rax
	sub	rdi, rsi
	mov	qword ptr [rbp - 8], rdi
	mov	rdi, qword ptr [rbp - 32]
	mov	rsi, qword ptr [rbp - 24]
	mov	rdx, qword ptr [rbp - 16]
	mov	rcx, qword ptr [rbp - 8]
	pop	rbp
	jmp	_combine_slots                  ## TAILCALL
	.cfi_endproc
```

Mapeá primero los argumentos: `a` llega en `rdi`, `b` llega en `rsi`, y el valor de
retorno saldrá en `rax`.

Ahora leé el frame:

- `push rbp` guarda el frame pointer del caller y mueve `rsp` ocho bytes hacia abajo.
- `mov rbp, rsp` hace que `rbp` sea una base estable para esta función.
- `[rbp - 32]`, `[rbp - 24]`, `[rbp - 16]` y `[rbp - 8]` son slots locales de stack.
- `lea rax, [rsi + rdi]` computa `sum = a + b` sin leer memoria.
- `sub rdi, rsi` computa `diff = a - b`, sobrescribiendo `rdi`.
- Los cuatro `mov` de carga recargan locals en los primeros cuatro registros de
  argumento enteros: `rdi`, `rsi`, `rdx` y `rcx`.
- `pop rbp` restaura el frame pointer del caller.
- `jmp _combine_slots` es un tailcall: el control se transfiere al helper sin pushear
  otra return address.

Hay una sutileza: esta función guarda debajo de `rbp` sin un `sub rsp, 32`. Como
`rsp == rbp` después del prólogo, esos slots viven justo debajo del stack pointer
actual. En targets System V AMD64, código tipo leaf puede usar la **red zone** de 128
bytes: espacio debajo de `rsp` que signal/interrupt handlers no deberían pisar en
código normal de user mode. El compilador la usa acá porque los locals mueren antes
del tailcall.

El helper recibe los valores en registros y no necesita locals en stack:

```asm
_combine_slots:                         ## @combine_slots
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rdi + rsi]
	add	rax, rdx
	add	rax, rcx
	pop	rbp
	ret
	.cfi_endproc
```

`ret` popea la return address que dejó el caller original. Como `_stack_example`
tailcalleó a `_combine_slots`, el helper vuelve directo a quien llamó a
`stack_example`.

## `push`, `pop`, `rsp` y `rbp`

La separación mental útil es:

| Registro | Rol |
|---|---|
| `rsp` | stack pointer actual; cambia cuando se pushea, popea, reserva o libera stack storage |
| `rbp` | frame pointer opcional; base estable para direccionar slots de stack |
| `rip` | instruction pointer; recibe la return address en `ret` |

`rsp` tiene que restaurarse antes de volver. Si una función resta a `rsp`, tiene que
sumar lo mismo en todos los caminos normales de retorno. Si pushea registros
callee-saved, tiene que popear o restaurarlos. Si la ABI exige alineación de stack
antes de un call, el compilador ajusta `rsp` para cumplirla.

`rbp` es opcional en código optimizado. Los debug builds suelen conservarlo porque hace
más fácil recorrer el stack y mostrar locals. Los optimized builds pueden liberarlo
como otro registro general y describir unwinding con metadata en vez de una cadena
visible de frame pointers.

## Apéndice ARM64

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Cuerpo relevante de la función:

```asm
_stack_example:                         ; @stack_example
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #32
	.cfi_def_cfa_offset 32
	str	x0, [sp, #24]
	str	x1, [sp, #16]
	add	x8, x1, x0
	str	x8, [sp, #8]
	sub	x8, x0, x1
	str	x8, [sp]
	ldr	x0, [sp, #24]
	ldr	x1, [sp, #16]
	ldr	x2, [sp, #8]
	ldr	x3, [sp], #32
	b	_combine_slots
	.cfi_endproc
```

AArch64 usa `sp` como stack pointer y `x29` como frame pointer convencional cuando se
necesita un frame pointer. Esta función no arma `x29`; simplemente aloca 32 bytes con
`sub sp, sp, #32`, guarda cuatro locals de 8 bytes, los recarga en registros de
argumento `x0` a `x3`, y desaloca con el load post-indexed:

```asm
ldr	x3, [sp], #32
```

Eso carga `diff` desde `[sp]` hacia `x3`, y después suma 32 a `sp`. El
`b _combine_slots` final es el tailcall ARM64.

El helper es solo registros:

```asm
_combine_slots:                         ; @combine_slots
	.cfi_startproc
; %bb.0:
	add	x8, x1, x0
	add	x8, x8, x2
	add	x0, x8, x3
	ret
	.cfi_endproc
```

Mismo C, vocabulario de stack distinto: x86-64 muestra `push`/`pop` alrededor de
`rbp`; ARM64 ajusta `sp` explícitamente con `sub` y un `ldr` post-indexed.

## Modos de falla y trade-offs

- **El stack crece hacia abajo.** Más storage de stack significa direcciones menores,
  no mayores.
- **`rbp` puede desaparecer.** El código optimizado muchas veces omite el frame
  pointer. No asumas que todo local se direcciona como `[rbp - offset]`.
- **La red zone es específica de la ABI.** System V AMD64 user code tiene una red zone
  de 128 bytes; Windows x64 no. Kernels y código low-level con interrupciones muchas
  veces deshabilitan o evitan asumir red zone.
- **La alineación del stack importa.** Las ABIs exigen una alineación particular antes
  de calls. Assembly manual que desalinea `rsp` puede crashear en código de librería
  que espera argumentos de stack o spills vectoriales alineados.
- **Los tailcalls cambian la forma del stack.** Un tailcall usa `jmp` en vez de `call`,
  así que el callee vuelve al caller original. Eso puede acortar backtraces.
- **La lifetime del stack es automática, no segura.** Devolver un puntero a un local
  de stack sigue siendo undefined behavior en C. Los bytes pueden quedar un rato, pero
  la lifetime terminó.

## En la práctica

- **Identificá primero el prólogo.** `push rbp; mov rbp, rsp` normalmente significa que
  se está estableciendo un frame pointer visible.
- **Traducí offsets a slots.** `[rbp - 8]`, `[rbp - 16]` y `[rbp - 24]` son ubicaciones
  cercanas de 8 bytes en el frame actual o en la red zone.
- **Separá storage de valor.** Un stack slot es una dirección. El valor se carga o se
  guarda con una instrucción como `mov`, `str` o `ldr`.
- **Chequeá si hay una llamada real.** `call` pushea una nueva return address; tailcall
  `jmp` no.
- **Uní assembly de stack con lifetimes de C.** Automatic storage duration en C se
  vuelve storage de máquina con forma de stack cuando el compilador decide que necesita
  memoria. Conectalo con [[lowlevel/pointers-and-memory/the-stack-frames-locals-and-automatic-storage|stack frames y automatic storage]].

**Conecta con:** [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|Registros x86-64 y el register file]] · [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|El core instruction set: mov, aritmética, lea]] · [[lowlevel/assembly-and-compiler-output/addressing-modes-and-memory-operands|Addressing modes y operandos de memoria]] · [[lowlevel/assembly-and-compiler-output/control-flow-jumps-conditions-and-flags|Control flow: jumps, condiciones y el registro de flags]] · [[lowlevel/pointers-and-memory/the-stack-frames-locals-and-automatic-storage|El stack: frames y automatic storage]]

## Fuentes

- **System V AMD64 ABI** — calling convention de registros, alineación de stack, reglas de call frame y definición de red zone. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 y Vol. 2 — semántica autoritativa de `push`, `pop`, `call`, `ret`, `mov`, addressing de stack y `rsp`/`rbp`. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3 — stack frames, procedures, register saving y llamadas a función a nivel máquina. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — ejemplos accesibles de stack frames y procedures en assembly x86-64. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Felix Cloutier — x86 and amd64 instruction reference** — lookup rápido de `push`, `pop`, `call`, `ret` e instrucciones relacionadas; verificá edge cases contra Intel SDM. https://www.felixcloutier.com/x86/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — `sp`, `x29`, addressing load/store ARM64 y detalles de ABI de plataforma para la salida del apéndice. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
