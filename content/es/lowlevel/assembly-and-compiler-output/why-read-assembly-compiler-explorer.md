---
title: "Por qué leer assembly — Compiler Explorer como herramienta diaria"
description: El assembly es la forma más directa de comprobar qué emitió realmente el compilador. Usá Compiler Explorer y flujos locales con -S/objdump para responder preguntas de performance y comportamiento sin adivinar.
tags: [assembly, compiler-explorer, x86-64, arm64, compiler-output]
order: 1
updated: 2026-06-22
---
# Por qué leer assembly — Compiler Explorer como herramienta diaria

El compilador no es una caja negra; es un generador de machine code con salida
inspeccionable. Leer esa salida responde dos preguntas diarias: "¿qué hace realmente
este código?" y "¿esto probablemente es rápido?" sin folklore. No necesitás escribir
mucho assembly (código ensamblador) para ganar muchísimo leyéndolo. Un poco de fluidez
convierte la salida del compilador en otra superficie de debugging, al lado de tests,
logs y un profiler.

> El reset: C no es lo que corre la CPU. C es lo que consume el compilador. El assembly
> es la primera forma legible de lo que la CPU va a ejecutar.

## Cómo funciona realmente

[Compiler Explorer](https://godbolt.org) es el loop de feedback más rápido para esta
habilidad: pegás una función C chica, elegís compilador, elegís target y flags, y leés
el panel de assembly. El hábito útil es achicar el experimento. Cambiá una línea, un
tipo o un flag, y mirá cómo cambian las instrucciones emitidas.

Leé un panel de Compiler Explorer en este orden:

- **Compilador y flags primero.** `gcc -O2 -masm=intel` y `clang -O0` son experimentos
  distintos. Mantené visibles los flags cuando compartís un resultado.
- **Coloreado fuente-a-assembly.** El resaltado mapea líneas de source a rangos de
  instrucciones. No prueba una correspondencia uno-a-uno, pero te dice qué expresión C
  causó qué región de salida.
- **Límite de función después.** Encontrá el label de la función que te importa e
  ignorá startup, constantes, tablas de excepciones y ruido de librería.
- **Registros antes que mnemónicos.** Identificá primero los registros de argumento de
  entrada y el registro de retorno de salida. Eso ancla el resto de la lectura.
- **Hot path al final.** Para preguntas de performance, mirá el cuerpo del loop,
  branches, loads/stores, llamadas y spills. El setup lindo fuera del hot path rara vez
  importa.

Esto conecta directo con [[lowlevel/machine-model/registers-and-the-isa|registros y la ISA]]: el compilador tiene que expresar tu código de alto nivel usando un set fijo de
registros, un instruction set (ISA), operandos de memoria y una calling convention
(convención de llamada). Leer assembly es leer esa traducción.

## Una función mínima, compilada

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/why-read-assembly-compiler-explorer/`. La
función central es deliberadamente aburrida:

```c
long weighted_score(long packets, long cost) {
    return packets * 3 + cost * 5 + 7;
}
```

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Este es el cuerpo
relevante de la función copiado de la salida real de:

```sh
gcc -S -O2 -masm=intel demo.c -o demo.s
```

```asm
_weighted_score:                        ## @weighted_score
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	lea	rax, [rdi + 2*rdi]
	lea	rcx, [rsi + 4*rsi]
	add	rax, rcx
	add	rax, 7
	pop	rbp
	ret
	.cfi_endproc
```

Arrancá por los registros, no por la astucia. Bajo la calling convention de C usual
en x86-64 en esta plataforma, los primeros dos argumentos enteros llegan en `rdi` y
`rsi`, y el valor de retorno sale en `rax`. Es el mismo mapa mental de
[[lowlevel/machine-model/registers-and-the-isa|registros y la ISA]].

Las instrucciones interesantes:

- `push rbp`, `mov rbp, rsp`, `pop rbp` son el prólogo/epílogo con frame pointer que
  este compilador conserva por defecto para este target. Con otros flags pueden
  desaparecer.
- `lea rax, [rdi + 2*rdi]` calcula `packets * 3`. Acá `lea` es aritmética; no carga
  desde memoria.
- `lea rcx, [rsi + 4*rsi]` calcula `cost * 5` en un registro scratch.
- `add rax, rcx` y `add rax, 7` terminan la expresión.
- `ret` vuelve al caller, con la respuesta ya en `rax`.

Ese es el premio: el source decía "multiplicar por constantes"; el compilador emitió
aritmética de generación de direcciones. No necesitaste adivinar si esto se volvía
instrucciones `imul` reales. Lo miraste.

## Reproducilo localmente

Compiler Explorer sirve para explorar rápido. Tu compilador local es la autoridad final
para el binario que realmente estás entregando.

```sh
cd examples/assembly-and-compiler-output/why-read-assembly-compiler-explorer

gcc -O0 -Wall -Wextra demo.c -o demo
./demo

gcc -S -O2 -masm=intel demo.c -o demo.s
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Usá `-S` cuando querés el archivo de assembly del compilador. Usá `objdump` cuando ya
tenés un object file (archivo objeto) o ejecutable y querés desensamblar los bytes:

```sh
gcc -g -O2 demo.c -o demo
objdump -d demo
objdump -d -S demo
```

`-g` le pide al compilador que incluya debug info (información de depuración), y
`objdump -S` usa esa información para intercalar source y disassembly cuando puede. En
macOS quizás uses `llvm-objdump` u `otool -tvV`; el flujo es el mismo: compilá con
debug info, desensamblá el binario y alineá source con instrucciones.

Dos familias de sintaxis aparecen todo el tiempo:

| | Sintaxis Intel | Sintaxis AT&T |
|---|---|---|
| Orden de operandos | destino primero: `add rax, rcx` | fuente primero: `addq %rcx, %rax` |
| Registros | `rax`, `rdi` | `%rax`, `%rdi` |
| Inmediatos | `7` | `$7` |
| Memoria | `[rdi + 2*rdi]` | `(%rdi,%rdi,2)` |
| Tamaño | suele inferirse de los operandos | suele ir en el sufijo del mnemónico: `q`, `l`, `w`, `b` |

Este atlas usa sintaxis Intel para x86-64 porque coincide con configuraciones comunes
de Compiler Explorer y deja los operandos de memoria visualmente cerca de la fórmula de
addressing mode.

## Apéndice ARM64

El mismo `demo.c` también se cross-compiló en esta máquina con:

```sh
clang -S -O2 -arch arm64 demo.c -o demo.arm64.s
```

Cuerpo relevante de la función:

```asm
_weighted_score:                        ; @weighted_score
	.cfi_startproc
; %bb.0:
	add	x8, x0, x0, lsl #1
	add	x9, x1, x1, lsl #2
	add	x8, x8, x9
	add	x0, x8, #7
	ret
	.cfi_endproc
```

AArch64 hace que el dataflow se sienta más regular. Los primeros dos argumentos
enteros llegan en `x0` y `x1`; el valor de retorno también sale en `x0`.
`add x8, x0, x0, lsl #1` significa "sumá `x0` con `x0 << 1`", así que calcula
`packets * 3`. `add x9, x1, x1, lsl #2` calcula `cost * 5`. Después la función suma
los parciales, suma `7` y retorna.

Fijate las diferencias con x86-64: no hay setup `rbp`/`rsp` en esta leaf function
(función hoja) mínima, hay un register file de propósito general más grande, operandos
shifted-register integrados en la instrucción aritmética, y `x0` sirve como primer
argumento y como valor de retorno. La misma expresión C se volvió instrucciones
distintas porque cambió la [[lowlevel/machine-model/registers-and-the-isa|ISA]] target.

## Modos de falla y trade-offs

- **El assembly no es único.** Compilador, versión del compilador, target triple, ABI,
  CPU tuning y flags importan. `gcc -O2`, Apple clang `-O2` y GCC en Linux pueden
  producir respuestas distintas y correctas.
- **No sobre-interpretes `-O0`.** La salida sin optimizar está hecha para debuggear, no
  para representar lo que elegiría el optimizador. Suele derramar variables al stack y
  preservar control flow con forma de source.
- **`-O2` cambia la forma, no el contrato.** El optimizador puede plegar constantes,
  reordenar trabajo, inlinear llamadas y borrar código muerto mientras el comportamiento
  observable obedezca la máquina abstracta de C. Si tu C tiene undefined behavior
  (comportamiento indefinido), ese contrato ya está roto.
- **Los nombres C++ están mangled.** Una función C llamada `weighted_score` queda
  reconocible. Una función C++ puede volverse un símbolo largo y codificado salvo que
  uses `extern "C"` para el borde del experimento.
- **Los símbolos tienen forma de plataforma.** Mach-O suele prefijar símbolos C con `_`
  (`_weighted_score`); ELF/Linux normalmente muestra `weighted_score`. Aprendé la
  convención antes de asumir que estás mirando otra función.
- **El mapeo a source necesita debug info.** Usá `-g` con `objdump -S` cuando querés
  líneas de source al lado del disassembly. Sin debug info, direcciones y símbolos
  suelen ser todo lo que tenés.

## En la práctica

- **Tené Compiler Explorer abierto mientras diseñás cambios C chicos.** Es
  especialmente bueno para revisar conversiones enteras, structs pasados por valor,
  loops chicos, llamadas a funciones y si un helper se inlineó.
- **Mové una variable por vez.** Si la salida cambia demasiado, achicá el ejemplo. El
  assembly se lee bien cuando el experimento es chico.
- **Leé registros primero.** Ver `rdi`/`rsi`/`rax` o `x0`/`x1`/`x0` transforma ruido en
  un boceto de dataflow.
- **Usá assembly local para afirmaciones finales.** Compiler Explorer es un banco de
  pruebas. El compilador, flags, plataforma y binario que entregás son la verdad de base.
- **Conectá el assembly con el pipeline.** `-S` es solo una parada en el camino de source
  a ejecución; object files, linking, loading y comportamiento en runtime son las capas
  siguientes de [[lowlevel/machine-model/how-source-becomes-execution|cómo el source se vuelve ejecución]].

**Conecta con:** [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] · [[lowlevel/machine-model/registers-and-the-isa|Registros y la ISA]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|La CPU: fetch–decode–execute]] · [[lowlevel/machine-model/how-source-becomes-execution|Cómo el source se vuelve ejecución]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **Compiler Explorer** — la herramienta diaria para comparar source con salida del compilador entre compiladores, flags y targets. https://godbolt.org/
- **GCC Manual + GNU Binutils `objdump` manual** — `-S` para assembly emitido por el compilador, y después `objdump -d/-S` para desensamblar objects y binarios existentes. https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html · https://sourceware.org/binutils/docs/binutils/objdump.html
- **System V AMD64 ABI** — la calling convention de registros detrás de `rdi`, `rsi` y `rax` en targets Unix-like x86-64. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Jonathan Bartlett — *Programming from the Ground Up*** — assembly desde primeros principios, útil para construir el modelo mental de "la CPU ejecuta instrucciones". https://savannah.nongnu.org/projects/pgubook/
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** — referencia accesible de registros, instrucciones y calling convention x86-64 con ejemplos. https://open.umn.edu/opentextbooks/textbooks/x86-64-assembly-language-programming-with-ubuntu
- **Intel SDM + Felix Cloutier x86 reference** — Intel es la autoridad; Felix Cloutier es el lookup rápido para `lea`, `add`, `ret` y compañía. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html · https://www.felixcloutier.com/x86/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — semántica AArch64 autoritativa más detalles de ABI en plataformas Apple para la salida de `clang -arch arm64`. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
