---
title: "Addressing modes y operandos de memoria"
description: Los operandos de memoria x86-64 nombran direcciones con base, índice, escala y desplazamiento. Aprendé cómo accesos a arrays y structs compilan a operandos reales, y por qué la sintaxis de dirección no es lo mismo que un load.
tags: [assembly, x86-64, addressing-modes, memory, compiler-output]
order: 4
updated: 2026-06-30
---
# Addressing modes y operandos de memoria

Los operandos de memoria son donde el assembly deja de parecer "matemática de
registros" y empieza a parecer programas reales: arrays, structs, slots de stack,
globals y punteros. En x86-64, un operando de memoria no es un nombre de variable; es
una **expresión de dirección**. La CPU computa una effective address (dirección
efectiva), y después la instrucción decide si carga desde esa dirección, guarda en ella
o solo la computa con `lea`. Leer esa forma entre corchetes es el puente entre la
sintaxis de punteros de C y las instrucciones de máquina.

> El reset: `[rdi + 8*rsi + 16]` no es "el valor". Es una expresión de dirección. La
> instrucción alrededor decide si se leen bytes, se escriben bytes o no se toca memoria.

## Cómo funciona realmente

La fórmula común de direcciones de memoria x86-64 es:

```text
base + index * scale + displacement
```

En sintaxis Intel eso aparece dentro de corchetes:

```asm
qword ptr [rdi + 8*rsi + 16]
```

Leelo así:

| Pieza | Significado |
|---|---|
| `rdi` | registro base: normalmente un puntero |
| `rsi` | registro índice: muchas veces un índice de array |
| `8` | escala: tamaño de elemento para `long` / valores pointer-sized |
| `16` | displacement: offset fijo en bytes |
| `qword ptr` | tamaño de operando: ocho bytes |

La escala puede ser 1, 2, 4 u 8. El displacement es una constante signed. No todos los
operandos usan todas las piezas: `[rdi]`, `[rdi + 8]`, `[rdi + 8*rsi]` y
`[rip + symbol]` son formas normales. La expresión de dirección computa bytes, no
elementos; el compilador multiplica los índices de array por el tamaño del elemento.

## Un acceso real a array

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/addressing-modes-and-memory-operands/`.

```c
long touch_neighbors(long *xs, long i, long bias) {
    long current = xs[i];
    long ahead = xs[i + 2];
    long updated = current + bias;

    xs[i + 1] = updated;
    return updated + ahead;
}
```

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Este es el cuerpo
relevante de la función copiado de la salida real de:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_touch_neighbors:                       ## @touch_neighbors
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	rax, rdx
	add	rax, qword ptr [rdi + 8*rsi]
	mov	qword ptr [rdi + 8*rsi + 8], rax
	add	rax, qword ptr [rdi + 8*rsi + 16]
	pop	rbp
	ret
	.cfi_endproc
```

Mapeá primero los argumentos: `xs` está en `rdi`, `i` en `rsi`, `bias` en `rdx`, y el
valor de retorno sale en `rax`.

Ahora decodificá los operandos de memoria:

- `mov rax, rdx` copia `bias` al registro de retorno/acumulador.
- `add rax, qword ptr [rdi + 8*rsi]` carga `xs[i]` y lo suma a `bias`. `8*rsi`
  existe porque `sizeof(long) == 8` en este target.
- `mov qword ptr [rdi + 8*rsi + 8], rax` guarda `updated` en `xs[i + 1]`. El `+ 8`
  extra es un `long` más.
- `add rax, qword ptr [rdi + 8*rsi + 16]` carga `xs[i + 2]` y lo suma al valor de
  retorno. El `+ 16` extra son dos elementos `long`.

La distinción importante: `add rax, [memory]` lee memoria; `mov [memory], rax` escribe
memoria. La expresión entre corchetes es solo la dirección. La instrucción aporta la
acción.

## Qué puede ser un operando de memoria

x86-64 es flexible, pero no arbitrario. Muchas instrucciones pueden usar un operando
de memoria:

```asm
mov     rax, qword ptr [rdi]            ; load
mov     qword ptr [rdi], rax            ; store
add     rax, qword ptr [rdi + 8*rsi]    ; load y suma
sub     qword ptr [rdi], rax            ; read-modify-write de memoria
```

Pero la mayoría de las instrucciones **no** permite dos operandos de memoria. La CPU
normalmente no puede hacer esto en una sola instrucción:

```asm
mov     qword ptr [rdi], qword ptr [rsi] ; no es la forma normal de x86-64
```

Cargás a un registro y después guardás:

```asm
mov     rax, qword ptr [rsi]
mov     qword ptr [rdi], rax
```

Esta es una razón por la que los registros son el centro de la máquina. La memoria es
donde los datos viven más tiempo; los registros son donde la mayoría del cómputo se
vuelve posible.

## `lea` usa addressing sin memoria

La sintaxis de addressing mode también alimenta a `lea`:

```asm
lea     rax, [rdi + 8*rsi + 16]
```

Eso computa la dirección numérica `rdi + 8*rsi + 16` y la escribe en `rax`. No lee
desde memoria en esa dirección. En términos de C, `lea` está más cerca de computar
`&xs[i + 2]` que de leer `xs[i + 2]`.

Los compiladores lo usan de dos maneras:

- **Formación de punteros.** Computar una dirección que se va a usar después.
- **Aritmética barata.** Computar formas como `x + 4*y + 16` sin tocar flags.

Ese doble uso es por qué tenés que leer la instrucción, no solo los corchetes.
Corchetes dentro de `mov` o `add` normalmente significan memoria. Corchetes dentro de
`lea` significan aritmética sobre una expresión de dirección.

## Apéndice ARM64

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Cuerpo relevante de la función:

```asm
_touch_neighbors:                       ; @touch_neighbors
	.cfi_startproc
; %bb.0:
	add	x8, x0, x1, lsl #3
	ldr	x9, [x8]
	ldr	x10, [x8, #16]
	add	x9, x9, x2
	add	x0, x9, x10
	str	x9, [x8, #8]
	ret
	.cfi_endproc
```

AArch64 es más explícito. `x0` es `xs`, `x1` es `i` y `x2` es `bias`. La primera
instrucción computa un puntero base para `&xs[i]`: `x8 = x0 + (x1 << 3)`. Los loads y
el store después usan offsets inmediatos chicos: `[x8]` para `xs[i]`, `[x8, #8]` para
`xs[i + 1]` y `[x8, #16]` para `xs[i + 2]`.

El contraste es la lección. x86-64 plegó `base + index*8 + displacement` directo dentro
de operandos de memoria. ARM64 primero computó la base indexada y después usó
instrucciones load/store. Mismo C, vocabulario de addressing distinto.

## Modos de falla y trade-offs

- **La sintaxis de dirección no dereferencia por sí sola.** `[rdi + 8]` dentro de
  `mov rax, ...` carga; dentro de `lea rax, ...` no.
- **Los offsets son bytes.** `+ 16` significa dieciséis bytes, no dieciséis elementos.
  Para `long`, eso son dos elementos en esta plataforma.
- **Los operandos de memoria esconden latencia.** `add rax, [addr]` parece aritmética,
  pero puede esperar a cache o RAM. `add` registro-a-registro y `add` con memoria no
  tienen el mismo costo.
- **Read-modify-write es una actualización real de memoria.** Instrucciones como
  `add [addr], rax` leen y escriben memoria. Eso importa para aliasing, sharing y
  concurrencia.
- **La mayoría de las instrucciones tiene un operando de memoria.** Si fuente y destino
  viven en memoria, esperá un registro temporal.
- **C no chequea bounds.** Si `i` está mal, la expresión de dirección igual se computa.
  Undefined behavior (comportamiento indefinido) significa que el compilador no te debe
  un trap.

## En la práctica

- **Traducí acceso a array a bytes.** `xs[i + 2]` se vuelve base `xs`, índice `i`,
  escala `sizeof *xs`, displacement `2 * sizeof *xs`.
- **Nombrá las piezas.** Al leer `[rdi + 8*rsi + 16]`, decí "puntero base, índice
  escalado, offset fijo" antes de preocuparte por la instrucción alrededor.
- **Mirá operandos de memoria en hot code.** Un loop con muchos operandos
  `[base + index*scale]` puede estar limitado por loads/stores, aunque la aritmética
  parezca chica.
- **Conectalo con punteros.** La aritmética de punteros en C es por elementos; el
  addressing de máquina es por bytes. El compilador traduce entre
  [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|aritmética de punteros y stride]]
  y estas expresiones de dirección.
- **Separá `lea` en tu cabeza.** `lea` usa sintaxis de dirección para producir una
  dirección entera; no es un load.

**Conecta con:** [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|El core instruction set: mov, aritmética, lea]] · [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|Registros x86-64 y el register file]] · [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Aritmética de punteros y stride]] · [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Layout de structs: alineación y padding]]

## Fuentes

- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 y Vol. 2 — addressing modes x86-64, operandos de memoria y semántica de instrucciones. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Felix Cloutier — x86 and amd64 instruction reference** — lookup rápido de formas de instrucción que aceptan operandos de memoria y semántica de `lea`. https://www.felixcloutier.com/x86/
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3 — representación a nivel máquina de arrays, punteros y referencias de memoria. https://csapp.cs.cmu.edu/
- **System V AMD64 ABI** — roles de registros usados para mapear argumentos antes de leer los operandos de memoria. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — ejemplos accesibles de operandos de memoria, arrays y addressing modes. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Arm Architecture Reference Manual + Apple ARM64 docs** — formas de addressing load/store ARM64 y detalles de ABI de plataforma para la salida del apéndice. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
