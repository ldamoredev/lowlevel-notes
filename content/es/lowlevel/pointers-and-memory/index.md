---
title: Punteros y Memoria
description: El corazón de la programación de bajo nivel. Stack vs heap, malloc/free, aritmética de punteros, alineación y padding, los bugs clásicos de memoria y allocators propios — manejar memoria con intención.
tags: [pointers, memory, allocators, heap, c-layer]
order: 0
updated: 2026-06-30
---
# Punteros y Memoria

Esta es la rama donde bajo nivel hace click o no. Un puntero es simplemente una
dirección con un tipo; de esa idea salen todo lo que hace rápido a C y todo lo que lo
hace peligroso. Vamos profundo en **el stack y el heap, `malloc`/`free`, aritmética de
punteros, struct layout, los bugs clásicos (use-after-free, double-free, leaks,
overflows) y escribir tus propios allocators** para que dejes de tratar la memoria como
infinita y empieces a manejarla con intención.

> La memoria no es un detalle que el lenguaje te esconde acá — es el material con el que
> trabajás. Poseerla significa saber quién asigna, quién libera y qué está haciendo cada
> byte de un struct.

## Notas planificadas

- [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero — una dirección con un tipo]]
- [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Aritmética de punteros y el stride del tipo]]
- [[lowlevel/pointers-and-memory/the-stack-frames-locals-and-automatic-storage|El stack: frames, locales y automatic storage]]
- [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|El heap: `malloc`/`free` y el allocator por debajo]]
- [[lowlevel/pointers-and-memory/pointers-to-pointers-double-indirection|Punteros a punteros (doble indirección)]]
- [[lowlevel/pointers-and-memory/void-star-and-type-erasure|`void*` y type erasure]]
- [[lowlevel/pointers-and-memory/const-correctness-and-what-const-really-promises|const-correctness y qué promete realmente `const`]]
- [[lowlevel/pointers-and-memory/function-pointers|Function pointers]]
- [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Struct layout: alineación y padding]]
- [[lowlevel/pointers-and-memory/data-layout-and-cache-friendliness|Data layout y cache-friendliness]]
- [[lowlevel/pointers-and-memory/strict-aliasing-and-type-punning|Strict aliasing y type punning]]
- [[lowlevel/pointers-and-memory/use-after-free-and-double-free|Bug clásico: use-after-free y double-free]]
- [[lowlevel/pointers-and-memory/buffer-overflow-and-out-of-bounds-access|Bug clásico: buffer overflow y acceso out-of-bounds]]
- [[lowlevel/pointers-and-memory/memory-leaks-and-ownership-discipline|Bug clásico: memory leaks y disciplina de ownership]]
- [[lowlevel/pointers-and-memory/custom-allocators-arena-pool-and-bump|Allocators propios: arena, pool y bump]]

## Fuentes centrales

- **Ulrich Drepper — *What Every Programmer Should Know About Memory*** — el paper de memoria. https://www.akkadia.org/drepper/cpumemory.pdf
- **CS:APP — capítulos de dynamic memory allocation** — cómo funciona realmente `malloc`. https://csapp.cs.cmu.edu/
- **Richard Reese — *Understanding and Using C Pointers*** (O'Reilly) — punteros punta a punta. https://www.oreilly.com/library/view/understanding-and-using/9781449344535/
- **Ryan Fleury — *Untangling Lifetimes: The Arena Allocator*** — arena allocation como herramienta de diseño. https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator
- **Doug Lea — *A Memory Allocator (dlmalloc)*** y writeups de `malloc` de **Dan Luu** — internals reales de allocators. https://gee.cs.oswego.edu/dl/html/malloc.html · https://danluu.com/malloc-tutorial/

**Conecta con:** [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] · [[lowlevel/os-from-scratch/index|OS desde Cero]]
