---
title: Índice de Bajo Nivel
description: Todas las ramas del Low-Level Atlas de un vistazo, agrupadas por fase — desde el modelo de máquina hasta el metal y de vuelta a un OS construido desde cero.
tags: [index, map]
order: 0
updated: 2026-06-21
---
# Índice de Bajo Nivel

Todo el atlas en una sola página. Diez ramas a través de siete fases, bajando desde
el modelo de máquina hasta el metal y subiendo de vuelta hacia un sistema operativo
funcional.

## 01 · Fundamentos

- [[lowlevel/machine-model/index|Modelo de Máquina]] — CPU, jerarquía de memoria, representación de datos; "ahora el runtime sos vos".

## 02 · La Capa C

- [[lowlevel/c-from-the-metal/index|C desde el Metal]] — el sistema de tipos flaco, undefined behavior (comportamiento indefinido), una capa fina sobre la máquina.
- [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] — stack (memoria automática) vs heap (memoria dinámica), `malloc`/`free`, los bugs clásicos, custom allocators.

## 03 · Bajar al Metal

- [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] — leer x86-64 (apéndice ARM64) y lo que emite tu compilador.
- [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] — preprocess→compile→assemble→link, ELF, debuggers, sanitizers.

## 04 · C++ (opcional)

- [[lowlevel/modern-cpp/index|C++ Moderno]] — RAII (gestión de recursos ligada al ciclo de vida del objeto), smart pointers, move semantics, la STL. Opcional; el OS es C-first.

## 05 · Sistemas

- [[lowlevel/systems-programming/index|Programación de Sistemas]] — syscalls (llamadas al sistema), procesos, `mmap`, señales, la capa POSIX.
- [[lowlevel/concurrency-and-performance/index|Concurrencia y Performance]] — atomics, memory ordering, lock-free, SIMD, benchmarking.

## 06 · El Proyecto

- [[lowlevel/os-from-scratch/index|OS desde Cero]] — de bootloader a shell en x86-64 bajo QEMU.

## ★ · Siempre activo

- [[lowlevel/craftsmanship-low-level/index|Craftsmanship de Bajo Nivel]] — TDD, fuzzing, contratos y review para código memory-unsafe.

## Referencia

- [[lowlevel/reference-registry|Registro de Referencias]] — libros, specs y herramientas canónicas citadas a lo largo del atlas.
