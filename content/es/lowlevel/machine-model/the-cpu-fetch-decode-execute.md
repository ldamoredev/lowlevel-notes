---
title: "La CPU: fetch–decode–execute y el clock"
description: La CPU es una máquina de estado temporizada: busca bytes en el instruction pointer, los decodifica en trabajo, los ejecuta, escribe resultados y avanza o redirige el control.
tags: [machine-model, cpu, clock, x86-64]
order: 2
updated: 2026-06-21
---
# La CPU: fetch–decode–execute y el clock

Una CPU no está "corriendo C". Está corriendo un loop diminuto en hardware: buscar
los bytes de la próxima instrucción, decodificar qué significan, ejecutar la
operación, escribir el resultado y decidir qué instrucción viene después. El clock le
da ritmo a ese loop, pero no significa "una instrucción por tick". Cuando podés
señalar `rip` y decir "los próximos bytes salen de ahí", el código fuente empieza a
volverse una cosa física.

> El reset: ejecutar es solo estado cambiando en el tiempo. Los registros guardan
> valores, la memoria guarda bytes, `rip` apunta a la próxima instrucción y el clock
> mantiene a la máquina avanzando por ese estado.

## El loop que la CPU nunca deja de correr

El modelo clásico es **fetch -> decode -> execute -> write-back**. Es la historia
mínima útil de un procesador:

| Etapa | Qué pasa |
|---|---|
| Fetch | Lee bytes de instrucción desde la memoria en la dirección del program counter |
| Decode | Convierte esos bytes en una operación: `add`, `mov`, `ret`, un branch, un load |
| Execute | Usa la ALU, la unidad de load/store, la unidad de branch u otro hardware para hacer el trabajo |
| Write-back | Confirma el resultado visible en un registro, memoria, flags o el próximo `rip` |

La idea importante de von Neumann es que **instrucciones y datos viven en la misma
memoria direccionable**. Un byte puede ser parte de tu string, parte de tu entero o
parte del stream de instrucciones; el contexto decide. Tu programa compilado no es
texto mágico. Son bytes cargados en memoria. La CPU busca algunos de esos bytes como
instrucciones, y esas instrucciones pueden leer o escribir otros bytes como datos.

Por eso la programación de bajo nivel aplasta abstracciones todo el tiempo. Una
expresión de C se vuelve instrucciones. Una instrucción se vuelve bytes. Esos bytes
viven en direcciones. La CPU solo ve las últimas dos capas: direcciones y bytes.

## El instruction pointer es el señalador

Toda arquitectura tiene un registro cuyo trabajo es "dónde está la próxima
instrucción?". El nombre genérico es **program counter**; en x86-64 el registro
arquitectural es `rip`, el **instruction pointer** (puntero de instrucción).

Para código lineal, `rip` avanza por la longitud de la instrucción que se acaba de
decodificar. En x86-64 esa longitud es variable: una instrucción puede medir 1 byte,
otra 3, otra 10 o más. La CPU no suma una cantidad fija. Decodifica bytes suficientes
para conocer la longitud de la instrucción, y después avanza `rip` a la siguiente.

El control flow es solamente código que escribe el señalador en otro lado:

- Un salto condicional cambia `rip` solo si su condición es verdadera.
- Un salto incondicional escribe un nuevo target en `rip`.
- Un `call` guarda una dirección de retorno en el stack y después redirige `rip` al callee.
- Un `ret` saca esa dirección guardada y la vuelve a poner en `rip`.

Ese es el significado mecánico de "el programa bifurca". No hay un diagrama de flujo
pasando dentro de la CPU. Solo hay una próxima dirección.

## El clock es un metrónomo, no un cronómetro

Una CPU de 3 GHz tiene un clock que tickea unas tres mil millones de veces por
segundo. Un tick es un **ciclo**. Eso suena a "tres mil millones de instrucciones por
segundo", pero ese es el modelo equivocado.

Distintas instrucciones tienen costos distintos. Una suma simple de enteros puede ser
barata. Un load que falla en cache puede esperar cientos de ciclos. Una división es
mucho más lenta que una suma. Un branch cuya dirección se predijo bien puede ser casi
invisible; un branch mal predicho puede tirar trabajo que la CPU ya había empezado.
Latencia es "cuánto falta para que este resultado esté listo"; throughput es "cada
cuánto puede la máquina empezar o terminar este tipo de trabajo". No son el mismo
número.

Las CPUs modernas también rompen instrucciones en unidades internas de trabajo,
superponen muchas instrucciones y emiten más de una operación por ciclo cuando las
dependencias lo permiten. Entonces el clock no promete que una línea de fuente, una
instrucción o una operación termine por tick. Es el ritmo contra el que se mide todo
ese progreso de hardware.

## Trazalo a mano

Acá hay un programita C que podés compilar y correr:

```c
#include <stdio.h>

__attribute__((noinline))
int add_then_double(int a, int b) {
    int sum = a + b;
    return sum * 2;
}

int main(void) {
    printf("%d\n", add_then_double(7, 5));
    return 0;
}
```

Buildealo localmente:

```bash
cc -Wall -Wextra -O2 -fomit-frame-pointer demo.c -o demo && ./demo
```

```text
24
```

Ahora pegá solo la función en [Compiler Explorer](https://godbolt.org), elegí un
compilador x86-64 GCC o Clang, usá `-O2 -fomit-frame-pointer -masm=intel`, y ocultá
directives/comments. Una salida x86-64 típica es:

```asm
add_then_double:
    lea     eax, [rdi + rsi]
    add     eax, eax
    ret
```

Bajo la System V AMD64 calling convention (convención de llamada), el primer
argumento entero llega en `edi`, el segundo en `esi`, y el valor entero de retorno
sale en `eax`. Las direcciones exactas dependen del loader, pero asumí que la función
empieza en `0x401120`:

| Paso | `rip` antes del fetch | Instrucción | Qué hace execute/write-back | `rip` después |
|---|---|---|---|---|
| 1 | `0x401120` | `lea eax, [rdi + rsi]` | Calcula `7 + 5`, escribe `12` en `eax` | `0x401123` |
| 2 | `0x401123` | `add eax, eax` | Suma `eax` consigo mismo, escribe `24` en `eax`, actualiza flags | `0x401125` |
| 3 | `0x401125` | `ret` | Saca la dirección de retorno del caller desde `[rsp]` y la pone en `rip` | dirección del caller |

Dos detalles importan acá. Primero, `lea` no está cargando memoria en esta forma; usa
el hardware de generación de direcciones como aritmética. Segundo, escribir `eax` en
x86-64 también pone en cero la mitad alta de `rax`, así que el registro de retorno
completo de 64 bits queda limpio aunque el tipo de retorno de C sea `int`.

Ese cuadro es el movimiento mental: una llamada a función no es una visita abstracta
a un bloque con nombre. Es `rip` aterrizando sobre bytes, esos bytes mutando
registros, y `ret` restaurando `rip` a la continuación guardada.

## Las CPUs modernas preservan el modelo, no el mecanismo

Las CPUs reales de alta performance no terminan literalmente una instrucción antes
de buscar la siguiente. Pipelinean las etapas fetch/decode/execute para que muchas
instrucciones estén en vuelo a la vez. Son superscalar, lo que significa que pueden
emitir varias operaciones en un ciclo. Ejecutan fuera de orden cuando trabajo
independiente puede correr antes que trabajo más viejo bloqueado. Predicen branches
para que el fetch pueda seguir antes de que el branch se resuelva.

Eso no vuelve inútil a fetch-decode-execute. Significa que el modelo es
**arquitectural**, no microscópico. La CPU trabaja fuerte para preservar la ilusión
de que tu thread único ejecutó en orden de programa, con registros y memoria quedando
como si cada instrucción hubiera terminado una después de la otra. Cuando leés
assembly, debuggeás un crash, razonás sobre un branch o entendés qué hace `ret`, ese
modelo arquitectural es el que necesitás primero.

La maquinaria más profunda importa después para performance. Pipelining, cache
misses, branch prediction y ejecución fuera de orden explican por qué dos streams de
instrucciones con el mismo resultado pueden tener costos muy distintos. Pero no podés
razonar sobre los trucos hasta que el modelo simple esté firme.

## Apéndice ARM64

En AArch64, la misma función tiene la misma forma pero registros distintos e
instrucciones de ancho fijo. Primer argumento entero: `w0`. Segundo: `w1`. Retorno
entero: `w0`.

```asm
add_then_double:
    add     w8, w1, w0
    lsl     w0, w8, #1
    ret
```

Cada instrucción AArch64 acá mide 4 bytes, así que el program counter avanza en pasos
limpios de 4 bytes para código lineal. La historia arquitectural sigue siendo la
misma: fetch en el program counter, decode, execute, write-back, avanzar o saltar.

## En la práctica

- **No leas GHz como velocidad.** La frecuencia de clock es una entrada. La mezcla de
  instrucciones, las cadenas de dependencias, la latencia de memoria, branch
  prediction y la salida del compilador deciden cuánto trabajo útil pasa por ciclo.
- **No asumas que una línea de fuente es una instrucción.** El optimizador puede
  borrarla, combinarla, duplicarla o reemplazarla por una instrucción como `lea` cuyo
  nombre no se parece a la operación C.
- **Cuando el control flow se vuelve misterioso, buscá `rip`.** En un debugger, el
  instruction pointer actual te dice dónde está realmente la ejecución. El call stack
  es una historia reconstruida desde direcciones de retorno guardadas y metadata de
  frames/unwind.
- **Usá primero el modelo simple, después refiná.** Fetch-decode-execute es la primera
  abstracción correcta. Pipelining y ejecución fuera de orden explican performance,
  no el significado básico de tu programa.

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/you-are-the-runtime-now|Ahora el runtime sos vos]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] · [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]]

## Fuentes

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3–4 — C a nivel máquina, secuenciación de instrucciones y el modelo de arquitectura del procesador. https://csapp.cs.cmu.edu/
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1–2 — la autoridad x86-64 para `rip`, encoding de instrucciones y semántica de instrucciones. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Compiler Explorer** — la forma más rápida de verificar cómo un fragmento concreto de C se vuelve assembly x86-64 o AArch64 bajo compiladores reales. https://godbolt.org/
- **Charles Petzold — *Code: The Hidden Language of Computer Hardware and Software*** — construye la intuición de que las instrucciones son bytes interpretados por hardware.
- **Ben Eater — *Build an 8-bit computer from scratch*** — una CPU fetch/decode/execute visible, construida con partes simples; ideal para volver concreto el loop de control. https://eater.net/8bit
- **Agner Fog — optimization manuals** — referencia avanzada para latencia de instrucciones, throughput, pipelining y costo microarquitectural real. https://www.agner.org/optimize/
