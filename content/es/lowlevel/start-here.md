---
title: Empezá por Acá
description: El camino de lectura por el Low-Level Atlas — el orden que te lleva desde el modelo de máquina hasta un OS construido desde cero sin saltearte los pasos que sostienen todo.
tags: [orientation, path]
order: 1
updated: 2026-06-21
---
# Empezá por Acá

Este atlas tiene un hilo conductor: pasar de *"sé programar"* a *"entiendo la
máquina"*. Podés leer cualquier nota por separado, pero las ramas están ordenadas
como una bajada y una subida. Este es el camino.

## El camino

1. **Reseteá tu modelo mental** — [[lowlevel/machine-model/index|Modelo de Máquina]]. Cómo funciona de verdad el hardware por debajo del runtime. Casi nada de código; todo lo demás cuelga de acá.
2. **Bajá a C** — [[lowlevel/c-from-the-metal/index|C desde el Metal]], después [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]. El lenguaje y la administración manual de memoria que te hacen responsable de todo.
3. **Mirá el metal** — [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] y [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]]. Lo que emite el compilador, y el pipeline que lo construye y lo debuggea.
4. **(Opcional) C++** — [[lowlevel/modern-cpp/index|C++ Moderno]]. RAII (gestión de recursos ligada al ciclo de vida del objeto) y el resto. Saltealo en la primera pasada si querés; el OS es C-first.
5. **Subí de vuelta a sistemas** — [[lowlevel/systems-programming/index|Programación de Sistemas]] y [[lowlevel/concurrency-and-performance/index|Concurrencia y Performance]]. Hablale al kernel; hacelo rápido.
6. **Construí la máquina** — [[lowlevel/os-from-scratch/index|OS desde Cero]]. El proyecto que consume todo lo anterior.

Atravesando todo: [[lowlevel/craftsmanship-low-level/index|Craftsmanship de Bajo Nivel]]
— la disciplina de testing y review que mantiene honesto el trabajo bare-metal.

## Cómo leerlo

- Las notas son **atómicas y respaldadas por fuentes**. Cada índice de rama lista sus notas planificadas y las fuentes canónicas detrás de ellas.
- El target del OS es **x86-64 vía cross-compiler (compilador cruzado) + QEMU** — host-agnostic, así corre igual en Intel y Apple Silicon.
- El assembly es **x86-64 primario** con un **apéndice ARM64** por nota.

Ver también: [[lowlevel/must-know|Lo que Tenés que Saber]] · [[lowlevel/index|Índice Completo]]
