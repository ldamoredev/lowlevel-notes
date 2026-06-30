---
title: "Inline assembly (y cuándo alcanzarlo)"
description: Inline assembly es un contrato con el compilador, no solo texto pegado en el stream de instrucciones. Aprendé constraints, clobbers, volatile, memory barriers y por qué intrinsics o builtins suelen ser más seguros.
tags: [assembly, inline-asm, compiler, constraints, clobbers]
order: 11
updated: 2026-06-30
---
# Inline assembly (y cuándo alcanzarlo)

Inline assembly permite que el source C contenga assembly específico del target, pero lo
difícil no es el mnemonic. Lo difícil es decirle al compilador qué lee, qué escribe y qué
clobberea ese assembly. Si ese contrato está mal, el optimizador puede mover, remover,
duplicar o reordenar código alrededor de formas que rompen tu intención. Alcanzá inline
assembly solo cuando un builtin, intrinsic, API de biblioteca o archivo `.s` separado no
sea una herramienta mejor.

> El reset: inline asm no es magia invisible. Es una frontera con el compilador que tiene
> inputs, outputs, clobbers y side effects declarados.

## Cómo funciona realmente

GCC/Clang extended asm tiene esta forma:

```c
__asm__ volatile (
    "template"
    : outputs
    : inputs
    : clobbers
);
```

Las piezas significan:

| Pieza | Significado |
|---|---|
| template | texto assembly emitido para el assembler del target |
| outputs | lvalues de C que el asm escribe |
| inputs | valores de C que el asm lee |
| clobbers | registros, flags o efectos de memoria no descriptos de otra forma |
| `volatile` | dice que el asm tiene side effects y no debe borrarse como dead |

El compilador no entiende la semántica de tu template de assembly. Entiende el contrato
que escribís alrededor. Constraints y clobbers son el puente entre optimización de C y
texto crudo de instrucciones.

## Un demo real de memory clobber

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/inline-assembly-and-when-to-reach-for-it/`.

```c
static NOINLINE long plain_twice(const long *value) {
    long first = *value;
    long second = *value;
    return first + second;
}

static NOINLINE long barrier_twice(const long *value) {
    long first = *value;
    __asm__ volatile("" ::: "memory");
    long second = *value;
    return first + second;
}
```

El string de inline asm está vacío. No emite ninguna instrucción de máquina. Lo
interesante es el clobber `"memory"`: le dice al compilador que la memoria puede haber
cambiado en ese punto, así que no debe reutilizar una carga anterior como si la memoria
no hubiera cambiado.

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Esta es la salida
relevante:

```sh
gcc -S -O2 -fno-omit-frame-pointer -masm=intel demo.c -o demo.s
```

```asm
_plain_twice:                           ## @plain_twice
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	rax, qword ptr [rdi]
	add	rax, rax
	pop	rbp
	ret
	.cfi_endproc
```

`plain_twice` carga una vez y duplica el registro. El compilador probó que dos loads
ordinarios desde el mismo `const long *` pueden representarse como un load más
`add rax, rax`.

Ahora la versión con un memory clobber en inline asm:

```asm
_barrier_twice:                         ## @barrier_twice
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	mov	rax, qword ptr [rdi]
	## InlineAsm Start
	## InlineAsm End
	add	rax, qword ptr [rdi]
	pop	rbp
	ret
	.cfi_endproc
```

No aparece ninguna instrucción entre `InlineAsm Start` e `InlineAsm End`, pero el
compilador igual vuelve a cargar memoria después de la barrera. El inline asm cambió la
optimización porque el contrato dijo que la memoria podía haber cambiado.

## Qué no significan `volatile` y `"memory"`

`asm volatile` significa que el compilador no debería borrar el asm como dead y debería
preservar su posición relativa respecto de otros asm volatile. No es un CPU memory fence
por sí solo. El clobber `"memory"` es una barrera de compilador para accesos a memoria:
limita reordenamientos del compilador alrededor del asm. Tampoco es un hardware fence por
sí solo.

Para orden real entre threads, usá atomics de C o instrucciones de fence mediante
intrinsics/builtins bien definidos salvo que estés escribiendo runtime low-level, kernel
o código de plataforma y conozcas el memory model.

## Apéndice ARM64

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Salida relevante:

```asm
_plain_twice:                           ; @plain_twice
	.cfi_startproc
; %bb.0:
	ldr	x8, [x0]
	lsl	x0, x8, #1
	ret
	.cfi_endproc
```

```asm
_barrier_twice:                         ; @barrier_twice
	.cfi_startproc
; %bb.0:
	ldr	x8, [x0]
	; InlineAsm Start
	; InlineAsm End
	ldr	x9, [x0]
	add	x0, x9, x8
	ret
	.cfi_endproc
```

Misma idea: el asm vacío no emitió instrucción target, pero el clobber `"memory"` forzó
un segundo load. ARM64 usa `x0` para el argumento puntero y el retorno, con `x8`/`x9`
como scratch registers elegidos por el compilador.

## Cuándo alcanzar inline asm

Las buenas razones son estrechas:

- Necesitás una instrucción target que no está disponible vía C, builtin o intrinsic.
- Estás escribiendo runtime low-level, kernel, context switch, syscall stub o frontera de
  port I/O.
- Necesitás una barrera de compilador y entendés que no es un hardware fence.
- Estás experimentando e inspeccionando explícitamente salida del compilador.

Las malas razones son comunes:

- Querés forzar código "más rápido" sin medir.
- Querés esconder undefined behavior de C del optimizador.
- No querés aprender el intrinsic o builtin disponible.
- Estás intentando escribir bloques grandes de assembly dentro de C en vez de usar un
  archivo assembly separado con una frontera ABI clara.

## Modos de falla y trade-offs

- **Constraints incorrectos miscompilan código.** Si el compilador no sabe qué lee o
  escribe el asm, puede asignar registros superpuestos o reutilizar valores viejos.
- **Clobbers faltantes son bugs.** Si tu asm cambia flags, memoria o registros no listados
  como outputs, declaralo.
- **`volatile` no es un memory model.** Evita algunas eliminaciones/reordenamientos del
  asm en sí; no da atomicidad ni ordering cross-core.
- **Los dialectos de assembly son específicos de compilador/target.** GCC/Clang extended
  asm no es ISO C, y MSVC tiene otras reglas.
- **Inline asm bloquea optimización.** El compilador no puede ver a través de tu template,
  así que puede perder oportunidades de scheduling, register allocation o vectorización.
- **La portabilidad cae rápido.** Incluso x86-64 y ARM64 necesitan templates, nombres de
  registros, constraints y a veces semántica distinta.

## En la práctica

- **Preferí builtins e intrinsics primero.** Dejan que el compilador entienda la operación.
- **Mantené el asm mínimo.** Una instrucción o una frontera estrecha se constriñe mejor
  que un mini-programa escondido en un string.
- **Declará el contrato completo.** Outputs, inputs, `cc`, `memory` y operandos tied son
  la interfaz real.
- **Inspeccioná salida optimizada.** Inline asm debe chequearse en `-O2`, donde el
  optimizador va a poner a prueba tu contrato.
- **Mové assembly grande fuera de línea.** Un `.s` separado más un prototipo C normal suele
  dar una frontera ABI más limpia.

**Conecta con:** [[lowlevel/assembly-and-compiler-output/optimization-what-o2-does-to-your-code|Optimización: qué le hace -O2 a tu código]] · [[lowlevel/assembly-and-compiler-output/system-v-amd64-calling-convention|La calling convention System V AMD64]] · [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|Registros x86-64 y el register file]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: el contrato]] · [[lowlevel/concurrency-and-performance/index|Concurrencia y Performance]]

## Fuentes

- **GCC — Extended Asm** — sintaxis autoritativa para constraints, clobbers, asm volatile y memory clobbers en GCC extended asm. https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
- **Clang Language Extensions — Inline Assembly** — compatibilidad de Clang y comportamiento target-specific para inline asm estilo GNU. https://clang.llvm.org/docs/LanguageExtensions.html#inline-assembly
- **System V AMD64 ABI** — roles de registros y fronteras ABI que inline asm debe respetar en targets x86-64 tipo Unix. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 y Vol. 2 — semántica de instrucciones y flags para templates x86-64. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Arm Architecture Reference Manual + Apple ARM64 docs** — registros, instrucciones y calling convention ARM64 para inline asm target-specific. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
- **cppreference — Atomics** — alternativa a nivel C para ordering entre threads que normalmente debería reemplazar fences escritos a mano con asm. https://en.cppreference.com/w/c/atomic
