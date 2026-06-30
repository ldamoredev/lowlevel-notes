---
title: "Control flow: jumps, condiciones y el registro de flags"
description: El control flow condicional en x86-64 es flags más saltos. Aprendé cómo cmp/test setean estado, cómo jcc lee esos flags y cómo if/else y tailcalls aparecen en salida real del compilador.
tags: [assembly, x86-64, control-flow, flags, jumps]
order: 5
updated: 2026-06-30
---
# Control flow: jumps, condiciones y el registro de flags

Los branches son cómo un flujo lineal de instrucciones se vuelve decisión. En C
escribís `if (x > limit)`; en x86-64 la CPU normalmente ve una instrucción que setea
bits de estado, seguida por otra instrucción que salta o cae al próximo bloque según
esos bits. La condición no es un valor en un registro normal. Es dataflow oculto a
través de `rflags`.

> El reset: `cmp a, b` no guarda un booleano. Setea flags como si hubiera computado
> `a - b`. El siguiente salto condicional decide si esos flags significan "andá a
> otro lado".

## Cómo funciona realmente

El control flow x86-64 se arma con un vocabulario chico:

| Pieza | Modelo mental | Ejemplos comunes |
|---|---|---|
| label | dirección de instrucción con nombre | `LBB0_2:` |
| `jmp` | branch incondicional | siempre setea `rip` al target |
| `cmp a, b` | setea flags desde `a - b` | compara sin conservar el resultado |
| `test a, b` | setea flags desde `a & b` | chequeo común de cero/null: `test rdi, rdi` |
| `jcc label` | branch condicional | `je`, `jne`, `jl`, `jle`, `jg`, `ja`, `jb` |

El instruction pointer (`rip`) normalmente avanza a la siguiente instrucción. Un salto
escribe otra próxima dirección en ese control flow. Un `jmp` incondicional lo hace
siempre. Un salto condicional primero lee condition flags.

Los flags que más vas a leer son:

| Flag | Significado después de aritmética/compare |
|---|---|
| `ZF` | zero flag: el resultado fue cero |
| `SF` | sign flag: el bit alto del resultado quedó seteado |
| `OF` | overflow flag: hubo overflow signed |
| `CF` | carry flag: hubo carry/borrow unsigned |

Las comparaciones signed y unsigned usan el mismo `cmp`, pero distintos mnemonics de
salto. Para `<=` signed, x86 usa `jle`, que depende de `ZF`, `SF` y `OF`. Para `<=`
unsigned, usa `jbe`, que depende de `CF` y `ZF`.

## Un branch real

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/control-flow-jumps-conditions-and-flags/`.

```c
static NOINLINE long above_limit(long x, long limit) {
    return (x - limit) * 3;
}

static NOINLINE long at_or_below_limit(long x, long limit) {
    return (limit - x) * 2;
}

long branch_score(long x, long limit) {
    if (x > limit) {
        return above_limit(x, limit);
    }

    return at_or_below_limit(x, limit);
}
```

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Este es el cuerpo
relevante de la función copiado de la salida real de:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_branch_score:                          ## @branch_score
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	cmp	rdi, rsi
	jle	LBB0_2
## %bb.1:
	pop	rbp
	jmp	_above_limit                    ## TAILCALL
LBB0_2:
	pop	rbp
	jmp	_at_or_below_limit              ## TAILCALL
	.cfi_endproc
```

Mapeá primero los argumentos: `x` llega en `rdi`, `limit` llega en `rsi`, y el valor de
retorno sale en `rax`.

Ahora leé la decisión:

- `cmp rdi, rsi` setea flags como si la CPU hubiera computado `rdi - rsi`, o sea
  `x - limit`.
- `jle LBB0_2` significa "saltá si es menor-o-igual signed". Si `x <= limit`, la
  ejecución va al segundo camino.
- Si el salto no se toma, el control cae en `%bb.1`, el camino `x > limit`.
- `jmp _above_limit` y `jmp _at_or_below_limit` son tailcalls. El compilador restaura
  `rbp` y salta al helper en vez de llamarlo y volver por un stack frame extra.

Los helpers son intencionalmente chicos, así el branch queda aislado:

```asm
_above_limit:                           ## @above_limit
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	sub	rdi, rsi
	lea	rax, [rdi + 2*rdi]
	pop	rbp
	ret
	.cfi_endproc
```

```asm
_at_or_below_limit:                     ## @at_or_below_limit
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	sub	rsi, rdi
	lea	rax, [rsi + rsi]
	pop	rbp
	ret
	.cfi_endproc
```

Fijate que el compilador no materializó un booleano de C. No hay un local `is_greater`
en memoria. La comparación vive en flags durante el tiempo justo para que `jle` la
consuma.

## Los flags son dataflow oculto

Los flags hacen que el assembly sea compacto, pero también hacen fácil perder
dependencias. Estas dos instrucciones están conectadas aunque no se pase ningún
registro con nombre entre ellas:

```asm
cmp     rdi, rsi
jle     LBB0_2
```

Si aparece otra instrucción que escribe flags entre ambas, el significado cambia:

```asm
cmp     rdi, rsi
add     rax, rcx      ; también actualiza flags
jle     LBB0_2        ; ahora lee flags de add, no de cmp
```

Los compiladores son cuidadosos con esto. Vos, leyendo assembly, también tenés que
serlo. Tratá `rflags` como un registro real que muchas instrucciones escriben
implícitamente.

Dos patrones aparecen todo el tiempo:

```asm
cmp     rdi, 0
je      was_zero
```

```asm
test    rdi, rdi
je      was_zero
```

Ambos pueden implementar un chequeo de cero. `test rdi, rdi` computa el AND bit a bit
solo para flags, así que si `rdi` es cero, `ZF` queda en 1. Es una forma corta e
idiomática para chequeos de null/cero.

## Apéndice ARM64

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Cuerpo relevante de la función:

```asm
_branch_score:                          ; @branch_score
	.cfi_startproc
; %bb.0:
	cmp	x0, x1
	b.le	LBB0_2
; %bb.1:
	b	_above_limit
LBB0_2:
	b	_at_or_below_limit
	.cfi_endproc
```

ARM64 tiene la misma forma general: compare, branch condicional, fallthrough, branch.
`x0` es `x`, `x1` es `limit`, y `cmp x0, x1` setea condition flags ARM desde `x0 - x1`.
`b.le` salta en menor-o-igual signed. Los `b` incondicionales son el equivalente ARM64
de los tailcall jumps de la salida x86-64.

Los helpers muestran otro vocabulario de instrucciones, pero el mismo split de
control flow:

```asm
_above_limit:                           ; @above_limit
	.cfi_startproc
; %bb.0:
	sub	x8, x0, x1
	add	x0, x8, x8, lsl #1
	ret
	.cfi_endproc
```

```asm
_at_or_below_limit:                     ; @at_or_below_limit
	.cfi_startproc
; %bb.0:
	sub	x8, x1, x0
	lsl	x0, x8, #1
	ret
	.cfi_endproc
```

La lección cruza arquitecturas: las condiciones normalmente no son objetos en heap,
locals o booleanos explícitos. Son estado temporal de máquina, consumido enseguida por
branches o conditional selects.

## Modos de falla y trade-offs

- **Los saltos signed y unsigned son distintos.** `jle` es signed. `jbe` es unsigned.
  Los mismos bits pueden producir otro borde de control flow según el tipo de C.
- **Los flags son estado frágil.** `add`, `sub`, `cmp`, `test`, `and`, `or`, `xor` y
  muchas otras instrucciones actualizan flags. Un salto lee los flags relevantes más
  recientes, no la comparación que a vos te parece más cercana.
- **Fallthrough también es un camino.** Si `jle` no se toma, la ejecución sigue en la
  próxima instrucción. Leé siempre el target del salto y el bloque que cae por debajo.
- **Los tailcalls borran un frame.** `jmp _above_limit` transfiere control sin pushear
  una nueva return address. Acá es correcto, pero cambia cómo se ve el stack en un
  debugger.
- **Undefined behavior en C le da margen al optimizador.** Si una condición depende de
  comportamiento que el estándar de C no define, el compilador puede remover o
  reescribir el branch de formas legales pero sorprendentes.
- **Branch prediction importa para performance.** Un salto condicional puede ser barato
  si se predice bien y caro si se predice mal. En hot code muchas veces se cambian
  branches por conditional moves, máscaras o table lookups.

## En la práctica

- **Leé `cmp` como resta sin storage.** `cmp rdi, rsi` significa "setear flags para
  `rdi - rsi`."
- **Nombrá la signedness.** Decí "`jle` menor-o-igual signed" o "`jbe` below-or-equal
  unsigned" hasta que la diferencia quede pegada.
- **Trazá primero el fallthrough.** Los branches condicionales siempre tienen dos
  destinos: el label y la próxima instrucción.
- **Prestá atención a `test reg, reg`.** Normalmente significa "¿este registro es
  cero?" sin modificar el registro.
- **Conectá branches con tipos fuente.** Signedness de C, integer promotions y
  undefined behavior influyen qué salto puede usar el compilador. Leelo junto con
  [[lowlevel/c-from-the-metal/integer-promotions-and-implicit-conversions|integer promotions e implicit conversions]]
  y [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|undefined behavior]].

**Conecta con:** [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Por qué leer assembly — Compiler Explorer como herramienta diaria]] · [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|Registros x86-64 y el register file]] · [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|El core instruction set: mov, aritmética, lea]] · [[lowlevel/assembly-and-compiler-output/stack-at-the-assembly-level|El stack a nivel assembly]] · [[lowlevel/machine-model/registers-and-the-isa|Registros y la ISA]]

## Fuentes

- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 y Vol. 2 — semántica autoritativa de `cmp`, `test`, `jcc`, `jmp`, flags y `ret`. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Felix Cloutier — x86 and amd64 instruction reference** — lookup rápido para saltos condicionales y efectos sobre flags; verificá edge cases contra Intel SDM. https://www.felixcloutier.com/x86/
- **System V AMD64 ABI** — roles de registros y convenciones call/return usadas para leer la salida del ejemplo. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3 — condition codes, branches y control flow a nivel máquina. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — ejemplos accesibles de jumps, comparisons y condition codes. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Arm Architecture Reference Manual + Apple ARM64 docs** — condition flags ARM64, conditional branches y detalles de ABI de plataforma para la salida del apéndice. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
