---
title: Modelo de Máquina
description: Cómo funciona de verdad una computadora por debajo del runtime — CPU, registros, jerarquía de memoria, representación de datos y cómo el código fuente se vuelve ejecución. La rama que resetea "ahora el runtime sos vos".
tags: [machine-model, cpu, memory, foundations]
order: 0
updated: 2026-06-21
---
# Modelo de Máquina

Durante años un runtime atrapó tus errores: un garbage collector liberó tu memoria,
una VM normalizó tus enteros, las excepciones desenrollaron tu stack (pila de
llamadas). Esta rama saca
ese piso. Antes de cualquier C, assembly o kernel, reconstruís el modelo mental de la
máquina desnuda — **la CPU, sus registros, la jerarquía de memoria y cómo los bits se
vuelven comportamiento** — para que todo lo que venga después tenga algo concreto a
lo que agarrarse.

> El reset: ya no hay ningún runtime cubriéndote la espalda. La máquina hace
> exactamente lo que dicen los bytes, incluido lo incorrecto, a la velocidad de unos
> pocos nanosegundos por cache miss. Esta rama casi no tiene código — es el sustrato.

## Notas planificadas

- [[lowlevel/machine-model/you-are-the-runtime-now|Ahora el runtime sos vos]] — lo que escondía el piso administrado
- [[lowlevel/machine-model/the-cpu-fetch-decode-execute|La CPU: fetch–decode–execute y el clock]]
- Registros y la instruction set architecture (ISA)
- La jerarquía de memoria: registros → L1/L2/L3 → RAM → disco
- Cache lines, localidad y por qué un miss cuesta ~100× un hit
- Stack vs heap (memoria dinámica): las dos regiones de memoria donde vas a vivir
- Bits, bytes, words y direcciones
- Complemento a dos y representación de enteros
- Endianness: orden de bytes y por qué te muerde
- IEEE 754 floating point (y por qué `0.1 + 0.2 != 0.3`)
- Cómo el fuente se vuelve ejecución: compile → load → run
- El espacio de direcciones del proceso y memoria virtual (primera mirada)

## Fuentes centrales

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)** — la columna de esta rama. csapp.cs.cmu.edu
- **Charles Petzold — *Code: The Hidden Language of Computer Hardware and Software*** — construye la máquina desde relays hacia arriba; intuición pura.
- **Ben Eater — *Build an 8-bit computer from scratch*** — una CPU en breadboards; el modelo mental más profundo. eater.net/8bit
- **Agner Fog — microarchitecture manuals** — detalle avanzado sobre cómo ejecutan las CPUs reales. agner.org/optimize

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]
