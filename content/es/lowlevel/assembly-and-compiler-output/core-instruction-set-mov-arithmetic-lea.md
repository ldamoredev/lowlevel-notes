---
title: "El core instruction set: `mov`, aritmética, `lea`"
description: Las primeras instrucciones x86-64 para leer con fluidez son movimiento de datos, aritmética entera y lea. Aprendé cómo salida real del compilador usa mov, add/sub, imul y lea para expresar C.
tags: [assembly, x86-64, instructions, mov, lea]
order: 3
updated: 2026-06-30
---
# El core instruction set: `mov`, aritmética, `lea`

La mayor parte del código entero optimizado se arma con un vocabulario chico repetido
a alta velocidad: copiar un valor, computar sobre registros, quizá tocar memoria,
retornar. No necesitás tener todo el manual x86-64 en la cabeza para empezar a leer
salida del compilador. Si `mov`, `add`/`sub`, `imul`, `xor` y `lea` te resultan
familiares, una cantidad sorprendente de C se vuelve legible. El truco es leer las
instrucciones como **cambios de estado**: qué registro o ubicación de memoria se
escribe, y qué operandos se leyeron.

> El reset: una instrucción no es una línea de C. Es una transición de estado en la
> CPU: lee operandos, escribe un destino, avanza `rip` y quizá actualiza flags.

## Cómo funciona realmente

En sintaxis Intel, el destino va primero:

```asm
add     rax, rcx      ; rax = rax + rcx
mov     rax, rdi      ; rax = rdi
sub     rax, 12       ; rax = rax - 12
```

Ese hábito de destino-primero es lo primero que tenés que internalizar. `mov rax, rdi`
copia desde `rdi` hacia `rax`; no borra `rdi`. La mayoría de las instrucciones enteras
tienen la misma forma: el destino también es una de las entradas, y el resultado lo
sobrescribe.

El core mínimo:

| Instrucción | Modelo mental | Notas |
|---|---|---|
| `mov dst, src` | copiá bits desde `src` hacia `dst` | `dst` puede ser registro o memoria; no memoria-a-memoria |
| `add dst, src` | `dst += src` | actualiza condition flags |
| `sub dst, src` | `dst -= src` | actualiza condition flags |
| `imul dst, src` | multiplicación signed | forma común de dos operandos: `dst *= src` |
| `xor dst, src` | xor bit a bit | `xor eax, eax` es un idiom común para cero |
| `lea dst, [address]` | computá una expresión de dirección | no hace load de memoria; no actualiza flags |

`mov` es una copia, no una mudanza. `lea` es lo opuesto de lo que su nombre sugiere a
muchos al principio: computa la **effective address** (dirección efectiva) dentro de
los corchetes y escribe ese entero en un registro. No dereferencia memoria. A los
compiladores les encanta `lea` porque el hardware de address-generation de x86-64 puede
computar formas como `base + index*2/4/8 + displacement` sin cambiar flags.

## Una traza real del compilador

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea/`.

```c
long core_ops(long x, long y, long *out) {
    long product = x * y;
    long adjusted = product + x * 8;

    *out = product;
    return adjusted - y + 12;
}
```

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Este es el cuerpo
relevante de la función copiado de la salida real de:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_core_ops:                              ## @core_ops
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rsi + 8]
	imul	rax, rdi
	sub	rax, rsi
	imul	rsi, rdi
	mov	qword ptr [rdx], rsi
	add	rax, 12
	pop	rbp
	ret
	.cfi_endproc
```

Mapeá primero los registros: por la ABI, `x` llega en `rdi`, `y` en `rsi`, `out` en
`rdx`, y el valor de retorno sale en `rax`.

El compilador reescribió la expresión algebraicamente. En vez de computar literalmente
`product + x * 8 - y + 12`, computa `x * (y + 8) - y + 12`. Las instrucciones son:

- `mov rbp, rsp` copia el stack pointer actual al frame pointer. Es bookkeeping de
  prólogo, no el cálculo de negocio.
- `lea rax, [rsi + 8]` calcula `y + 8` y lo escribe en `rax`. No se toca memoria.
- `imul rax, rdi` calcula `(y + 8) * x` en `rax`.
- `sub rax, rsi` resta el `y` original.
- `imul rsi, rdi` calcula el `product = y * x` separado porque la función tiene que
  guardarlo vía `out`.
- `mov qword ptr [rdx], rsi` guarda ocho bytes desde `rsi` en la memoria apuntada por
  `out`.
- `add rax, 12` termina el valor de retorno.

Por eso leer instrucciones gana frente a imaginarlas. El source tiene `product` y
`adjusted` nombrados; el assembly optimizado tiene vidas de registros y una forma
algebraica equivalente.

## `lea` es aritmética con sintaxis de dirección

`lea` significa "load effective address", pero en código optimizado muchas veces es
solo matemática entera:

```asm
lea     rax, [rdi + 2*rdi]      ; rax = rdi * 3
lea     rcx, [rsi + 4*rsi]      ; rcx = rsi * 5
lea     rax, [rax + 8]          ; rax = rax + 8
```

La sintaxis con corchetes describe una expresión de dirección. Con `mov rax, [rdi + 8]`,
la CPU usa esa dirección para leer memoria. Con `lea rax, [rdi + 8]`, la CPU escribe en
`rax` la expresión numérica de dirección en sí. Misma sintaxis de dirección, instrucción
distinta.

Dos consecuencias importan todo el tiempo:

- `lea` no actualiza `rflags`, así que sirve cerca de comparaciones y branches.
- `lea` no puede expresar matemática arbitraria; la escala está limitada a 1, 2, 4 u
  8, con un registro base, un registro índice y un displacement opcional.

La próxima nota profundiza en esas formas de operandos de memoria; acá el punto es
simple: cuando ves `lea`, preguntate "¿esto es cálculo de dirección o aritmética?"

## Apéndice ARM64

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Cuerpo relevante de la función:

```asm
_core_ops:                              ; @core_ops
	.cfi_startproc
; %bb.0:
	mul	x8, x1, x0
	add	x9, x1, #8
	str	x8, [x2]
	neg	x8, x1
	madd	x8, x9, x0, x8
	add	x0, x8, #12
	ret
	.cfi_endproc
```

AArch64 usa un vocabulario más load/store. `x0`, `x1` y `x2` llevan `x`, `y` y `out`.
`mul x8, x1, x0` calcula el producto. `str x8, [x2]` lo guarda en `*out`.
`madd x8, x9, x0, x8` es multiply-add fusionado: `x8 = x9 * x0 + x8`, acá usando `x8`
como `-y`. La misma expresión C mapea a otro instruction set: ARM64 tiene `madd`;
x86-64 usó `imul` más `sub`/`add`.

## Modos de falla y trade-offs

- **No leas mnemónicos como líneas de source.** Un compilador puede plegar, reordenar o
  reescribir expresiones preservando el comportamiento observable. La salida de
  `core_ops` computa una forma algebraica equivalente, no el orden de líneas del source.
- **`mov` copia.** El registro fuente sigue teniendo su valor viejo. Suena obvio hasta
  que empezás a leer código que reutiliza registros agresivamente.
- **Los operandos de memoria necesitan tamaño.** `qword ptr [rdx]` le dice al assembler
  que es un store de ocho bytes. Los operandos de registro suelen implicar tamaño; la
  memoria sola normalmente no.
- **La mayoría de las instrucciones x86-64 permite como máximo un operando de memoria.**
  `mov [a], [b]` no es la forma normal. Cargá a un registro y después guardá.
- **Los flags importan.** `add`, `sub`, `imul` y `xor` pueden afectar condition flags;
  `lea` no. Si la siguiente instrucción es un branch condicional, los flags pueden ser
  el dataflow oculto.
- **Signedness casi siempre es un tema de C hasta que deja de serlo.** `add`/`sub` son
  los mismos bits para enteros signed y unsigned; comparaciones, división, chequeos de
  overflow y widening son donde signedness se vuelve visible.

## En la práctica

- **Arrancá por la escritura.** En cada instrucción, preguntá qué destino cambia.
- **Tratá `lea` como "computá esta expresión de corchetes".** Después decidí si el
  resultado se usa como puntero o como aritmética entera.
- **Comentá semántica, no transliteración.** "guardar product en `*out`" sirve; "mover
  `rsi` a memoria" solo repite el mnemónico.
- **Esperá álgebra del compilador.** Si una secuencia optimizada no se parece al C,
  chequeá si es una expresión equivalente antes de asumir misterio.
- **Emparejá esta nota con el mapa de registros.** Las instrucciones recién se vuelven
  legibles cuando conocés los roles de [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|registros x86-64 y el register file]].

**Conecta con:** [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Por qué leer assembly — Compiler Explorer como herramienta diaria]] · [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|Registros x86-64 y el register file]] · [[lowlevel/machine-model/registers-and-the-isa|Registros y la ISA]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|La CPU: fetch–decode–execute]] · [[lowlevel/c-from-the-metal/integer-promotions-and-implicit-conversions|Promociones enteras y conversiones implícitas]]

## Fuentes

- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 y Vol. 2 — semántica autoritativa de `mov`, instrucciones aritméticas, flags y `lea`. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Felix Cloutier — x86 and amd64 instruction reference** — lookup rápido de `mov`, `add`, `sub`, `imul`, `xor`, `lea` y `ret`; verificá edge cases contra Intel SDM. https://www.felixcloutier.com/x86/
- **System V AMD64 ABI** — roles de registros usados para leer los argumentos y el valor de retorno de la función. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3 — representación a nivel máquina de expresiones C y condition codes. https://csapp.cs.cmu.edu/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — ejemplos accesibles para instrucciones enteras y lectura a nivel registros. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Arm Architecture Reference Manual + Apple ARM64 docs** — semántica de instrucciones ARM64 y detalles de ABI de plataforma para la salida del apéndice. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
