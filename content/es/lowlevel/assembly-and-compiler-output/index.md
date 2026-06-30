---
title: Assembly y Salida del Compilador
description: Leé y entendé x86-64 (con apéndice ARM64). Registros, instrucciones, stack frames, calling conventions y Compiler Explorer como herramienta diaria para ver exactamente qué emiten tu C y C++.
tags: [assembly, x86-64, arm64, compiler, metal]
order: 0
updated: 2026-06-21
---
# Assembly y Salida del Compilador

No necesitás *escribir* mucho assembly para beneficiarte enormemente de *leerlo*. El
objetivo de esta rama es fluidez: abrir [Compiler Explorer](https://godbolt.org),
pegar tu C o C++ y entender las instrucciones que salen. Nos enfocamos en **x86-64**
(coincide con el target del OS, la literatura y los defaults de Godbolt), cubrimos
registros, el core instruction set, stack frames y la calling convention System V, y
agregamos un **apéndice ARM64** por tema — porque ese es tu hardware diario de acá en
adelante.

> El compilador no es una caja negra. Una vez que podés leer su salida, "¿esto es
> rápido?" y "¿qué está haciendo realmente este código?" dejan de ser adivinanzas.

## Notas planificadas

- [[lowlevel/assembly-and-compiler-output/why-read-assembly-compiler-explorer|Por qué leer assembly — Compiler Explorer como herramienta diaria]]
- [[lowlevel/assembly-and-compiler-output/x86-64-registers-and-the-register-file|Registros x86-64 y el register file]]
- [[lowlevel/assembly-and-compiler-output/core-instruction-set-mov-arithmetic-lea|El core instruction set: `mov`, aritmética, `lea`]]
- [[lowlevel/assembly-and-compiler-output/addressing-modes-and-memory-operands|Addressing modes y operandos de memoria]]
- Control flow: jumps, condiciones y el registro de flags
- El stack a nivel assembly (`push`/`pop`, `rsp`/`rbp`)
- Stack frames: prólogo y epílogo de función
- La calling convention System V AMD64
- Cómo compilan las construcciones de C: loops, structs, `switch`
- Optimización: qué le hace `-O2` a tu código
- Inline assembly (y cuándo alcanzarlo)
- Apéndice ARM64: registros AArch64 y calling convention

## Fuentes núcleo

- **Compiler Explorer (godbolt.org)** — la herramienta diaria; mirá qué emite cualquier compilador. godbolt.org
- **Jonathan Bartlett — *Programming from the Ground Up*** (gratis) — assembly desde primeros principios.
- **Ed Jorgensen — *x86-64 Assembly Language Programming with Ubuntu*** (PDF gratis).
- **Felix Cloutier — referencia de instrucciones x86 y amd64** — el lookup rápido. felix.fr/x86 (e Intel SDM como autoridad).
- **ARM64:** Stephen Smith — *Programming with 64-Bit ARM Assembly*; docs ARM64 / Apple Silicon de Apple.

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/machine-model/index|Modelo de Máquina]]
