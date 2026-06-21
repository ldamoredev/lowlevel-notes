---
title: Lo que Tenés que Saber
description: Las ideas que sostienen la programación de bajo nivel — el puñado de verdades que, una vez internalizadas, hacen que el resto del atlas encaje.
tags: [orientation, fundamentals]
order: 2
updated: 2026-06-21
---
# Lo que Tenés que Saber

Si no recordás nada más de este atlas, recordá esto. Cada idea se expande a través
de las ramas; juntas son la columna vertebral de la mentalidad de bajo nivel.

## Las ideas

- **Ahora el runtime sos vos.** Sin GC, sin bounds checks, sin normalización de enteros. La máquina ejecuta los bytes exactamente — incluido lo incorrecto.
- **Un puntero es una dirección con un tipo.** Todo lo poderoso y todo lo peligroso de C sale de esa sola frase.
- **La memoria es una jerarquía, y la distancia es tiempo.** Un cache miss puede costar ~100× un hit. La performance depende más de los patrones de acceso que del código ingenioso.
- **El undefined behavior (comportamiento indefinido) es un contrato que firmaste.** El compilador puede asumir que nunca pasa — y va a optimizar como si no pasara.
- **El stack (memoria automática) y el heap (memoria dinámica) son mundos distintos.** Uno es automático y rápido; el otro es manual y queda en tus manos. Saber cuál es cuál evita la mayoría de los bugs de memoria.
- **El compilador no es una caja negra.** Leé su assembly y "qué está haciendo esto / es rápido?" dejan de ser adivinanzas.
- **El linking (etapa de enlace) es real.** La compilación separada, los símbolos y el loader (cargador) son donde vive la mitad de los errores confusos.
- **La disciplina escala hacia abajo.** TDD, contratos y sanitizers importan *más* en C, no menos — porque el modo de falla es corrupción, no una excepción.

## Dónde viven

- [[lowlevel/machine-model/index|Modelo de Máquina]] — el reset del runtime, la jerarquía de memoria.
- [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] — punteros, stack vs heap, bugs.
- [[lowlevel/c-from-the-metal/index|C desde el Metal]] — undefined behavior, el sistema de tipos.
- [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] — leer el compilador.
- [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] — linking y el pipeline de build.
- [[lowlevel/craftsmanship-low-level/index|Craftsmanship de Bajo Nivel]] — disciplina en el metal.

Ver también: [[lowlevel/start-here|Empezá por Acá]] · [[lowlevel/index|Índice Completo]]
