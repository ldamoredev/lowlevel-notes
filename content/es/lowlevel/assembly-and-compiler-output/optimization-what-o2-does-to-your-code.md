---
title: "Optimización: qué le hace `-O2` a tu código"
description: La salida optimizada del compilador no es código fuente con menos líneas. Aprendé cómo -O2 elimina trabajo muerto, pliega constantes, mantiene valores en registros, reescribe branches y conserva comportamiento observable.
tags: [assembly, compiler, optimization, o2, x86-64]
order: 10
updated: 2026-06-30
---
# Optimización: qué le hace `-O2` a tu código

`-O2` le pide al compilador preservar el comportamiento observable del programa, no la
forma del source. Los locals pueden desaparecer, las constantes se pueden plegar, la
aritmética redundante puede borrarse, los branches pueden volverse conditional moves y
los slots de stack pueden transformarse en lifetimes de registros. Leer assembly
optimizado significa seguir valores y side effects, no esperar una traducción línea por
línea desde C.

> El reset: el código optimizado es el resultado de una prueba. Si el compilador puede
> probar que cierto trabajo es innecesario bajo las reglas de C, puede removerlo o
> reescribirlo.

## Cómo funciona realmente

En `-O0`, los compiladores suelen priorizar debuggability y salida parecida al source.
Mantienen muchos locals en slots de stack, emiten branches obvios y evitan la mayoría de
las reescrituras. En `-O2`, el compilador invierte más trabajo en mejorar código normal
de runtime.

Transformaciones comunes de `-O2`:

| Transformación | Qué ves en assembly |
|---|---|
| constant folding | `40 + 2` se vuelve inmediato `42` |
| dead-code elimination | valores que no pueden afectar comportamiento desaparecen |
| register allocation | locals se vuelven registros en vez de slots `[rbp - offset]` |
| strength reduction | multiplicar por 2 se vuelve shift o `lea` |
| branch conversion | un `if` simple puede volverse `cmov` o `csel` |
| inlining | una llamada chica puede desaparecer dentro del caller |

El límite legal es la abstract machine de C. Accesos `volatile`, llamadas con side
effects desconocidos, atomics, I/O y memoria observable limitan al optimizador. Undefined
behavior amplía el margen porque el compilador no necesita preservar comportamiento que C
no define.

## Una traza real `-O0` versus `-O2`

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/optimization-what-o2-does-to-your-code/`.

```c
NOINLINE long optimize_me(long x, long y) {
    long twice = x * 2;
    long dead = y * 0;
    long folded = 40 + 2;

    if (twice > y) {
        return twice + folded + dead;
    }

    return y - twice + folded + dead;
}
```

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Esta es la salida
relevante en `-O0`:

```sh
gcc -S -O0 -fno-omit-frame-pointer -masm=intel demo.c -o demo.O0.s
```

```asm
_optimize_me:                           ## @optimize_me
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	qword ptr [rbp - 16], rdi
	mov	qword ptr [rbp - 24], rsi
	mov	rax, qword ptr [rbp - 16]
	shl	rax, 1
	mov	qword ptr [rbp - 32], rax
	imul	rax, qword ptr [rbp - 24], 0
	mov	qword ptr [rbp - 40], rax
	mov	qword ptr [rbp - 48], 42
	mov	rax, qword ptr [rbp - 32]
	cmp	rax, qword ptr [rbp - 24]
	jle	LBB0_2
```

`-O0` conserva locals con forma de source: `x`, `y`, `twice`, `dead`, `folded` y el slot
de retorno aparecen como ubicaciones de stack. Incluso emite `imul ... , 0` para
`dead = y * 0` porque el objetivo de `-O0` no es razonar agresivamente sobre trabajo
inútil.

Ahora la misma función en `-O2`:

```sh
gcc -S -O2 -fno-omit-frame-pointer -masm=intel demo.c -o demo.O2.s
```

```asm
_optimize_me:                           ## @optimize_me
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rdi + rdi]
	sub	rsi, rax
	cmovge	rax, rsi
	add	rax, 42
	pop	rbp
	ret
	.cfi_endproc
```

Mapeá los argumentos: `x` está en `rdi`, `y` en `rsi`, y el retorno sale en `rax`.

Después leé el dataflow optimizado:

- `lea rax, [rdi + rdi]` computa `twice = x * 2`.
- `dead = y * 0` desaparece porque siempre es cero y no tiene side effect.
- `folded = 40 + 2` se volvió el inmediato `42`.
- `sub rsi, rax` computa `y - twice`.
- `cmovge rax, rsi` selecciona el valor del else cuando la aritmética/flags indican que
  `twice <= y`. El `if` fuente ya no aparece como branch.
- `add rax, 42` suma la constante plegada una sola vez después de la selección.

Es la misma función, pero no la misma forma. El optimizador encontró una expresión
equivalente más chica.

## Por qué el branch se volvió `cmov`

El source tenía:

```c
if (twice > y) {
    return twice + 42;
}

return y - twice + 42;
```

La secuencia optimizada computa barato las dos bases candidatas: `twice` en `rax` y
`y - twice` en `rsi`. Después `cmovge` copia condicionalmente `rsi` hacia `rax` en vez de
saltar. Esto puede evitar un branch misprediction en lógica de decisión chica.

Eso no significa que `cmov` siempre sea mejor. Para alternativas caras, branches
predecibles o código con side effects de memoria, un branch puede ser la salida correcta.
`-O2` es un paquete de heurísticas y análisis, no una promesa de usar un único patrón de
instrucción.

## Apéndice ARM64

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Cuerpo relevante de la función:

```asm
_optimize_me:                           ; @optimize_me
	.cfi_startproc
; %bb.0:
	lsl	x8, x0, #1
	subs	x9, x1, x8
	csel	x8, x8, x9, lt
	add	x0, x8, #42
	ret
	.cfi_endproc
```

ARM64 cuenta la misma historia con otro vocabulario. `x0` es `x`, `x1` es `y`, y el
retorno sale en `x0`. `lsl x8, x0, #1` computa `x * 2`. `subs x9, x1, x8` computa
`y - twice` y setea flags. `csel x8, x8, x9, lt` selecciona condicionalmente el candidato
correcto. `add x0, x8, #42` suma la constante plegada.

La lección cross-architecture es esa: desaparecen variables fuente; queda flujo de
valores.

## Modos de falla y trade-offs

- **Debuggear código optimizado es distinto.** Las variables pueden estar optimized out,
  mezcladas o vivas solo en registros durante parte de la función.
- **Undefined behavior importa más en `-O2`.** Si tu C depende de signed overflow,
  punteros inválidos, out-of-bounds o violaciones de strict aliasing, la optimización
  puede producir salida sorprendente pero legal.
- **No todo trabajo puede removerse.** Operaciones `volatile`, atomics, llamadas con side
  effects y escrituras observables limitan reescrituras.
- **Inlining cambia stack traces.** Las llamadas pueden desaparecer, lo cual ayuda al
  runtime y confunde expectativas ingenuas de backtrace.
- **Branchless no es automáticamente más rápido.** Conditional moves/selects evitan
  branches pero pueden computar más valores que un branch.
- **Las versiones de compilador difieren.** La secuencia exacta de instrucciones no es
  una API estable. Leé la intención, no solo la lista de mnemonics.

## En la práctica

- **Usá `-O2` para preguntas de performance.** La salida `-O0` sirve para mecánica, pero
  suele mentir sobre presión real de registros e instruction count.
- **Seguí valores, no locals fuente.** Preguntá de dónde sale el return value, no a dónde
  se fue el nombre de variable.
- **Buscá trabajo eliminado.** Si algo desapareció, preguntá si tenía algún efecto
  observable bajo las reglas de C.
- **Compará una función en ambos niveles.** Un diff local `-O0`/`-O2` es una de las formas
  más rápidas de aprender transformaciones del compilador.
- **Conectalo con UB.** La optimización es donde el contrato de
  [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|undefined behavior]]
  se vuelve muy concreto.

**Conecta con:** [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Por qué leer assembly — Compiler Explorer como herramienta diaria]] · [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|El core instruction set: mov, aritmética, lea]] · [[lowlevel/assembly-and-compiler-output/control-flow-jumps-conditions-and-flags|Control flow: jumps, condiciones y el registro de flags]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: el contrato]] · [[lowlevel/c-from-the-metal/more-ub-signed-overflow-aliasing-and-sequencing|Más UB: signed overflow, aliasing y sequencing]]

## Fuentes

- **LLVM Clang command guide** — niveles de optimización, opciones de code generation y cómo Clang trata `-O0` a `-O3`. https://clang.llvm.org/docs/CommandGuide/clang.html
- **GCC Optimize Options** — comportamiento documentado de niveles de optimización GCC y flags activados por `-O2`. https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, caps. 3 y 5 — código máquina optimizado, conditionals, procedures y razonamiento de performance. https://csapp.cs.cmu.edu/
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 y Vol. 2 — semántica de instrucciones para `lea`, `cmov`, aritmética y flags. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Felix Cloutier — x86 and amd64 instruction reference** — lookup rápido de `cmov`, `lea`, `sub` y comportamiento de flags; verificá edge cases contra Intel SDM. https://www.felixcloutier.com/x86/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — `csel`, shifts, aritmética que setea flags y detalles de ABI ARM64 para la salida del apéndice. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
