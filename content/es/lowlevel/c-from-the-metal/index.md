---
title: C desde el Metal
description: C para alguien que ya programa. Las partes peligrosas — un sistema de tipos débil, undefined behavior (comportamiento indefinido), el preprocessor, array-to-pointer decay — y por qué "una capa fina sobre la máquina" te hace responsable de todo.
tags: [c, undefined-behavior, types, c-layer]
order: 0
updated: 2026-06-21
---
# C desde el Metal

Ya sabés programar. C no te enseña eso — te saca la red de seguridad y te entrega la
máquina. Esta rama trata las partes de C que sorprenden a un ingeniero con experiencia
viniendo de un lenguaje administrado: **un sistema de tipos que chequea apenas algo,
undefined behavior (comportamiento indefinido) que el compilador explota, un
preprocessor textual, arrays que se convierten silenciosamente en punteros, y una
biblioteca estándar que asume que sabés lo que estás haciendo.**

> C es una abstracción fina y con fugas sobre el hardware. Ese es su poder y su peligro:
> casi no hay nada entre tu fuente y las instrucciones, así que cada garantía en la que te
> apoyabas ahora es tu trabajo.

## Notas planificadas

- [[lowlevel/c-from-the-metal/why-c-still-matters|Por qué C todavía importa — una capa fina sobre la máquina]]
- [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|El sistema de tipos de C es débil (y lo que te cuesta)]]
- [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: el contrato que no sabías que firmaste]]
- [[lowlevel/c-from-the-metal/more-ub-signed-overflow-aliasing-and-sequencing|Más UB: signed overflow, aliasing y sequencing]]
- El preprocessor: macros, includes, compilación condicional
- Translation units, declaraciones vs definiciones y linkage
- Arrays y array-to-pointer decay
- Structs, unions y bitfields
- Los strings son solo `char*` (y las consecuencias)
- Integer promotions y conversiones implícitas
- La biblioteca estándar mínima (esenciales de libc)
- Build y ejecución de un programa C desde cero (gcc/clang)
- Leer el estándar C y cppreference

## Fuentes centrales

- **Jens Gustedt — *Modern C*** (PDF gratis) — la columna; enseña C como debería escribirse hoy. gustedt.gitlabpages.inria.fr/modern-c
- **Kernighan & Ritchie — *The C Programming Language* (K&R)** — el original canónico y terso.
- **Robert Seacord — *Effective C*** — C moderno con foco en seguridad.
- **Peter van der Linden — *Expert C Programming: Deep C Secrets*** — una mina de quirks y UB.
- **Beej's Guide to C** — amigable y gratis. beej.us/guide/bgc · **cppreference (C)** — la referencia. en.cppreference.com/w/c

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]]
