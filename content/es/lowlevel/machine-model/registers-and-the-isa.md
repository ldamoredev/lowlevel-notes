---
title: "Registros y la ISA"
description: Los registros son el almacenamiento más rápido y chico de la CPU; la ISA es el contrato que los define y las instrucciones que los tocan. El mapa de registros de x86-64, el aliasing de sub-registros y por qué el compilador pelea por tan pocos nombres.
tags: [machine-model, registers, isa, x86-64, arm64]
order: 3
updated: 2026-06-21
---
# Registros y la ISA

Ya vimos a la CPU buscar bytes en `rip` y mutar estado en
[[lowlevel/machine-model/the-cpu-fetch-decode-execute|el loop fetch–decode–execute]].
Esta nota hace zoom en *dónde vive ese estado* y *quién define las reglas*. Los
registros son el puñado de slots nombrados y de ancho fijo dentro de la CPU — el
almacenamiento más rápido de la máquina por lejos. La **instruction set architecture
(ISA)** es el contrato que dice qué registros existen, qué instrucciones pueden
tocarlos y cómo se codifican esas instrucciones. Las variables de C son una ficción;
los registros y la ISA son la superficie real y finita sobre la que corre tu código.

> El reset: no hay un pozo infinito de variables. Hay ~16 registros de propósito
> general, y casi todo lo que hace la CPU es "cargar en un registro, operar sobre
> registros, guardar de vuelta". El compilador invierte esfuerzo real en decidir qué
> llega a vivir en uno.

## Por qué los registros son especiales

Un registro es almacenamiento on-die que la ALU puede leer y escribir en
esencialmente cero tiempo extra — *es* donde pasa el cómputo. Todo lo demás está más
lejos y es más lento. Latencias aproximadas en una máquina moderna:

| Almacenamiento | Latencia aprox. de acceso | Capacidad |
|---|---|---|
| Registro | ~0 ciclos (es el operando) | ~16 GPRs × 8 bytes |
| Cache L1 | ~4 ciclos | decenas de KB |
| Cache L2 | ~12 ciclos | cientos de KB |
| RAM | ~100–300 ciclos | GBs |

Esa brecha es toda la razón por la que importan los registros. Un `add rax, rbx` es
gratis comparado con un valor que tiene que venir de la RAM. La CPU no puede operar
directamente sobre la mayoría de la memoria como pretende un lenguaje de alto nivel —
trae los valores *a* registros, trabaja ahí, y escribe resultados *de vuelta*. "Hacelo
rápido" muy seguido significa "mantené los valores calientes en registros y fuera de
la memoria".

## El mapa de registros de x86-64

x86-64 expone **16 registros de propósito general (GPRs)**, cada uno de 64 bits:

| Registro | Rol convencional (según la ABI / algunas instrucciones) |
|---|---|
| `rax` | Valor de retorno; operando implícito de `mul`/`div` |
| `rbx` | Scratch callee-saved |
| `rcx` | 4º argumento; count implícito de shift/`rep` |
| `rdx` | 3er argumento; mitad alta de `mul`/`div` |
| `rsi`, `rdi` | 2º y 1er argumento; source/dest de string-ops |
| `rbp` | Frame pointer (por convención) |
| `rsp` | **Stack pointer** (no es un registro libre) |
| `r8`–`r15` | Agregados por x86-64; `r8`/`r9` son el 5º/6º argumento |

Se llaman "de propósito general", pero la ISA y la ABI les clavan trabajos
convencionales a casi todos — `rsp` es siempre el stack pointer, `rax` lleva valores de
retorno, los shifts leen su count de `cl`. Más allá de los GPRs están `rip` (el
instruction pointer, no escribible directamente como dato), `rflags` (bits de estado
como zero, carry, sign, overflow) y los **registros vectoriales** `xmm0–15` / `ymm` /
`zmm` (128/256/512 bits) que usa el código SIMD.

## Aliasing de sub-registros (y un borde filoso)

Cada GPR es un registro físico que podés direccionar en cuatro anchos. **No son
registros separados** — se solapan:

| 64-bit | 32-bit | 16-bit | 8-bit (low) |
|---|---|---|---|
| `rax` | `eax` | `ax` | `al` |
| `rdi` | `edi` | `di` | `dil` |
| `r8` | `r8d` | `r8w` | `r8b` |

Escribir `eax` y escribir `al` no es simétrico, y esta es una sorpresa clásica:

```asm
mov eax, 1      ; escribe los 32 bits bajos Y zero-extiende: rax = 0x0000000000000001
mov al, 1       ; escribe solo los 8 bits bajos: los 56 bits altos de rax quedan SIN CAMBIAR
```

Escribir un sub-registro de **32 bits** zero-extiende al registro completo de 64 bits
(lo vimos en la nota #2: un retorno `int` en `eax` deja `rax` limpio). Escribir un
sub-registro de **8 o 16 bits** deja intactos los bits altos — así que un byte alto
viejo puede filtrarse si asumiste otra cosa. El hardware hace esto por razones de
compatibilidad, y es una fuente real de bugs sutiles en assembly escrito a mano.

## La ISA es un contrato, no un chip

La **instruction set architecture** es la especificación que una CPU promete cumplir.
Define:

- el **set de registros** (los nombres de arriba) y sus anchos,
- las **instrucciones**, su **semántica** y su **codificación en bytes**,
- los **addressing modes** (cómo los operandos nombran a la memoria),
- el **memory model** (qué ordenamiento pueden observar otros cores), y
- niveles de privilegio, interrupciones y excepciones.

La ISA es la frontera entre dos mundos. **Los compiladores apuntan a ella; las CPUs la
implementan.** Y algo clave: muchas *microarquitecturas* distintas implementan la
misma ISA: Intel y AMD implementan **x86-64**; la serie M de Apple, Qualcomm y Ampere
implementan **ARM64 (AArch64)**. El código compilado para una ISA corre en cualquier
chip que la cumpla, aunque el silicio adentro sea radicalmente distinto. Esa
separación es justamente por qué el OS de este atlas puede apuntar a x86-64 y correr
sin cambios en una Mac Intel y (vía QEMU) en Apple Silicon.

Dos grandes familias de ISA, dos filosofías:

| | x86-64 | ARM64 (AArch64) |
|---|---|---|
| Estilo | CISC | RISC |
| Largo de instrucción | Variable (1–15 bytes) | Fijo (4 bytes) |
| GPRs | 16 | 31 (`x0`–`x30`) + `sp` |
| Operandos en memoria | Muchas instrucciones tocan memoria | Solo load/store |

La línea CISC/RISC es más borrosa que antes — los chips x86 modernos decodifican esas
instrucciones complejas en *micro-ops* tipo RISC internamente — pero el encuadre
todavía explica por qué el disassembly de ARM64 se ve tan regular y el de x86-64 tan
denso.

## Mirá cómo se usan los registros

Una función con seis argumentos muestra de un vistazo los registros de argumento de
System V y el registro de retorno. Pegá esto en
[Compiler Explorer](https://godbolt.org) con un compilador x86-64 en `-O2 -masm=intel`:

```c
long sum6(long a, long b, long c, long d, long e, long f) {
    return a + b + c + d + e + f;
}
```

```asm
sum6:
    lea     rax, [rdi + rsi]   ; rax = a + b
    add     rax, rdx           ; + c
    add     rax, rcx           ; + d
    add     rax, r8            ; + e
    add     rax, r9            ; + f
    ret                        ; el valor de retorno ya está en rax
```

Cada argumento llegó en un registro que la ABI fijó de antemano — `rdi, rsi, rdx, rcx,
r8, r9` — y el resultado se acumula en `rax` porque ahí es donde vive un valor de
retorno. (El 7º argumento entero, si existiera, entraría por el stack, no por un
registro — simplemente no alcanzan.) El *porqué* de este orden es la
[[lowlevel/assembly-and-compiler-output/index|calling convention]]; el punto acá es
que el compilador está eligiendo entre un set chico y fijo de nombres.

## Apéndice ARM64

AArch64 tiene 31 registros de propósito general `x0`–`x30` (64 bits), cada uno con una
vista de 32 bits `w0`–`w30`, más un stack pointer separado `sp` y un peculiar
**registro cero** (`xzr`/`wzr`) que se lee como 0 y descarta las escrituras. `x30` es
el link register (dirección de retorno); el program counter no es un GPR. La misma
`sum6`:

```asm
sum6:
    add     x8, x0, x1
    add     x8, x8, x2
    add     x8, x8, x3
    add     x8, x8, x4
    add     x0, x8, x5
    ret
```

Los argumentos vienen en `x0`–`x7` (hasta ocho en registros, contra seis en x86-64), y
el valor de retorno sale en `x0`. Escribir `w0` zero-extiende a `x0`, replicando la
regla `eax`→`rax`.

## En la práctica

- **Hay muchos menos registros que variables.** Cuando una función necesita más valores
  vivos que registros, el compilador **derrama** (spill) algunos al stack y los
  recarga — "register pressure". Los loops ajustados son más rápidos en parte porque
  sus valores calientes entran en registros.
- **Las escrituras a sub-registros no son uniformes.** Las de 32 bits zero-extienden;
  las de 8/16 bits preservan los bits altos. En assembly escrito a mano o inline, esto
  filtra datos viejos si te lo olvidás.
- **No memorices el mapa — reconocelo.** No necesitás cada rol convencional de memoria,
  pero ver `rdi`/`rsi`/`rax` en un disassembly y saber "argumentos y retorno" es lo que
  hace rápido leer la salida del compilador.
- **Una ISA, muchos chips.** Apuntar a una ISA (x86-64) en vez de a una CPU específica
  es lo que hace portable a un binario entre fabricantes y generaciones — y lo que deja
  al proyecto del OS host-agnostic.

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/machine-model/the-cpu-fetch-decode-execute|La CPU: fetch–decode–execute]] · [[lowlevel/assembly-and-compiler-output/index|Assembly y Salida del Compilador]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3 — representación a nivel máquina de los programas; registros, operandos y cómo C mapea sobre ellos. https://csapp.cs.cmu.edu/
- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 — el modelo autoritativo de registros x86-64, comportamiento de sub-registros y codificación. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **Arm Architecture Reference Manual for A-profile (AArch64)** — el set de registros ARM64 autoritativo, `wzr`/`xzr` y la semántica de instrucciones. https://developer.arm.com/documentation/ddi0487/latest
- **System V AMD64 ABI** — qué registros llevan argumentos, retornos y cuáles son callee-saved. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Compiler Explorer** — verificá en segundos qué registros asigna un compilador real a una función C dada. https://godbolt.org/
- **Agner Fog — manuales de optimización** — uso de registros, el costo de los spills y detalle microarquitectural. https://www.agner.org/optimize/
