---
title: "Cómo compilan las construcciones de C: loops, structs, `switch`"
description: Los loops se vuelven branches sobre labels, los structs se vuelven offsets en bytes y los switch se vuelven comparaciones, jump tables o conditional selects. Aprendé a leer los tres en salida real del compilador.
tags: [assembly, x86-64, c, loops, structs, switch]
order: 9
updated: 2026-06-30
---
# Cómo compilan las construcciones de C: loops, structs, `switch`

La sintaxis de C es high-level comparada con la CPU, pero la mayoría de sus
construcciones baja a pocos patrones de máquina. Un loop es un label, una condición, un
cuerpo y un branch de vuelta. Un campo de struct es un offset en bytes desde un puntero
base. Un `switch` es una estrategia de dispatch elegida por el compilador: cadena de
comparaciones, jump table, árbol de branches, conditional selects o una mezcla. Cuando
ves esas formas, el control flow y el data layout de C se vuelven legibles en assembly.

> El reset: assembly no conoce "for loop", "campo de struct" ni "case de `switch`".
> Conoce direcciones, offsets, comparaciones, saltos y aritmética.

## Cómo funciona realmente

Las construcciones fuente mapean aproximadamente así:

| Construcción de C | Forma de máquina |
|---|---|
| `for` / `while` | label de header, condición, cuerpo, incremento, branch |
| `items[i]` | puntero base más `i * sizeof item` |
| `p->field` | load/store en offset fijo desde `p` |
| `switch` | cadena de compares, jump table, branch tree o conditional select |

La optimización cambia la forma visible. El compilador puede invertir una condición,
combinar incremento con exit test, hoistear aritmética de punteros o reemplazar un branch
por un conditional move. Tu trabajo no es reconstruir la sintaxis exacta de C. Tu trabajo
es recuperar dataflow y bordes de control flow.

## Un loop real sobre structs

El artefacto ejecutable de esta nota vive en
`examples/assembly-and-compiler-output/how-c-constructs-compile-loops-structs-switch/`.

```c
struct Packet {
    int tag;
    long value;
    int scale;
};

NOINLINE long classify_and_sum(const struct Packet *items, size_t count) {
    long total = 0;

    for (size_t i = 0; i < count; i++) {
        long scaled = items[i].value * items[i].scale;

        switch (items[i].tag) {
        case 0:
            total += scaled;
            break;
        case 1:
            total -= scaled;
            break;
        case 2:
            total += items[i].value;
            break;
        default:
            total += 7;
            break;
        }
    }

    return total;
}
```

En este target, `struct Packet` tiene este layout:

| Campo | Offset | Razón |
|---|---:|---|
| `tag` | 0 | `int` arranca el objeto |
| padding | 4 | alinear el siguiente `long` |
| `value` | 8 | `long` necesita alineación de 8 bytes |
| `scale` | 16 | `int` después del `long` |
| tail padding | 20 | hacer que el stride de array sea 24 |

El stride es 24 bytes, así que `items[i + 1]` está 24 bytes después de `items[i]`.

En esta máquina, `gcc` es Apple clang 15 apuntando a Mach-O x86-64. Este es el cuerpo
relevante de la función copiado de la salida real de:

```sh
gcc -S -O2 -fno-omit-frame-pointer -masm=intel demo.c -o demo.s
```

```asm
_classify_and_sum:                      ## @classify_and_sum
	.cfi_startproc
## %bb.0:
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset rbp, -16
	mov	rbp, rsp
	.cfi_def_cfa_register rbp
	test	rsi, rsi
	je	LBB0_10
## %bb.1:
	add	rdi, 16
	xor	eax, eax
	jmp	LBB0_4
	.p2align	4, 0x90
LBB0_8:                                 ##   in Loop: Header=BB0_4 Depth=1
	add	rax, rcx
LBB0_3:                                 ##   in Loop: Header=BB0_4 Depth=1
	add	rdi, 24
	dec	rsi
	je	LBB0_11
LBB0_4:                                 ## =>This Inner Loop Header: Depth=1
	mov	rcx, qword ptr [rdi - 8]
	mov	edx, dword ptr [rdi - 16]
	cmp	edx, 2
	je	LBB0_8
## %bb.5:                               ##   in Loop: Header=BB0_4 Depth=1
	movsxd	r8, dword ptr [rdi]
	imul	rcx, r8
	cmp	edx, 1
	je	LBB0_9
## %bb.6:                               ##   in Loop: Header=BB0_4 Depth=1
	test	edx, edx
	je	LBB0_8
## %bb.2:                               ##   in Loop: Header=BB0_4 Depth=1
	add	rax, 7
	jmp	LBB0_3
	.p2align	4, 0x90
LBB0_9:                                 ##   in Loop: Header=BB0_4 Depth=1
	sub	rax, rcx
	jmp	LBB0_3
LBB0_10:
	xor	eax, eax
LBB0_11:
	pop	rbp
	ret
	.cfi_endproc
```

Mapeá primero los argumentos: `items` llega en `rdi`, `count` llega en `rsi`, y el valor
de retorno sale en `rax`.

Ahora decodificá las construcciones fuente:

- `test rsi, rsi; je LBB0_10` es el chequeo de loop vacío. Si `count == 0`, devuelve cero.
- `xor eax, eax` inicializa `total = 0`.
- `add rdi, 16` hace que `rdi` apunte al campo `scale` del item actual. Así el compilador
  direcciona campos cercanos con offsets negativos.
- `mov rcx, qword ptr [rdi - 8]` carga `value`, porque `scale_offset - 8 == value_offset`.
- `mov edx, dword ptr [rdi - 16]` carga `tag`, porque `scale_offset - 16 == tag_offset`.
- `movsxd r8, dword ptr [rdi]` carga `scale` signed y lo ensancha a 64 bits.
- `imul rcx, r8` computa `scaled = value * scale`.
- `cmp edx, 2`, `cmp edx, 1` y `test edx, edx` implementan el `switch`.
- `add rdi, 24` avanza al siguiente `struct Packet`.
- `dec rsi; je LBB0_11` decrementa el conteo restante y sale cuando llega a cero.

El compilador no preservó literalmente el orden fuente. Chequea `case 2` antes de
computar `scaled`, porque ese case solo necesita `value`. Es legal porque no cambia el
comportamiento observable de C.

## Leer el switch

Para este `switch` chico, el compilador eligió una cadena de comparaciones:

```asm
	cmp	edx, 2
	je	LBB0_8
	movsxd	r8, dword ptr [rdi]
	imul	rcx, r8
	cmp	edx, 1
	je	LBB0_9
	test	edx, edx
	je	LBB0_8
	add	rax, 7
```

`edx` contiene `tag`. Si `tag == 2`, el código salta al camino que suma `value`. Si no,
calcula `scaled`. `tag == 1` resta `scaled`; `tag == 0` suma `scaled`; cualquier otro
valor suma 7.

Para switches densos con muchos casos, los compiladores suelen usar jump tables. Para
switches pequeños, cadenas de branches o conditional moves suelen ser más baratas y más
chicas.

## Apéndice ARM64

El mismo `demo.c` se cross-compiló en esta máquina con:

```sh
clang -S -O2 -fno-omit-frame-pointer -arch arm64 demo.c -o demo.arm64.s
```

Cuerpo relevante de la función:

```asm
_classify_and_sum:                      ; @classify_and_sum
	.cfi_startproc
; %bb.0:
	cbz	x1, LBB0_4
; %bb.1:
	mov	x8, x0
	mov	x0, #0
	add	x8, x8, #16
LBB0_2:                                 ; =>This Inner Loop Header: Depth=1
	ldur	x9, [x8, #-8]
	ldrsw	x10, [x8]
	mul	x10, x9, x10
	ldur	w11, [x8, #-16]
	add	x9, x9, x0
	sub	x12, x0, x10
	add	x13, x0, #7
	add	x10, x10, x0
	cmp	w11, #0
	csel	x10, x13, x10, ne
	cmp	w11, #1
	csel	x10, x12, x10, eq
	cmp	w11, #2
	csel	x0, x9, x10, eq
	add	x8, x8, #24
	subs	x1, x1, #1
	b.ne	LBB0_2
; %bb.3:
	ret
LBB0_4:
	mov	x0, #0
	ret
	.cfi_endproc
```

ARM64 muestra los mismos hechos de loop/struct: `x0` empieza como `items`, `x1` es
`count`, `x8` se vuelve un puntero al campo `scale` actual, y el puntero avanza 24 bytes
por iteración. `ldur x9, [x8, #-8]` carga `value`, `ldur w11, [x8, #-16]` carga `tag`, y
`ldrsw x10, [x8]` carga y sign-extiende `scale`.

El lowering del switch cambia. En vez de una cadena de branches, ARM64 usa `csel`
conditional selects para elegir el próximo valor de `total` después de las comparaciones.
Mismo C, distinta estrategia de máquina.

## Modos de falla y trade-offs

- **Esperar orden fuente.** El compilador puede reordenar loads, comparaciones y
  aritmética mientras preserve comportamiento observable.
- **Olvidar padding de struct.** `items[i]` avanza por `sizeof(struct Packet)`, no por la
  suma de campos que imaginaste.
- **Asumir que todo switch es jump table.** Switches chicos suelen compilar a branches o
  conditional selects. Switches densos y grandes suelen volverse jump tables.
- **Perder la signed extension.** `movsxd` y `ldrsw` importan: `scale` es un `int` signed
  ensanchado a aritmética `long` de 64 bits.
- **Confundir conteo de loop con índice.** Loops optimizados pueden usar un conteo
  restante decremental en vez de un `i` creciente.
- **Leer labels como bloques fuente.** Los labels son targets de control flow generados
  por el compilador, no nombres de bloques originales de C.

## En la práctica

- **Empezá por el layout.** Conocé `sizeof` y offsets antes de leer accesos a struct.
- **Encontrá la variable de inducción.** Puede ser un puntero que avanza por stride, no un
  índice entero explícito.
- **Trazá una iteración.** Seguí loads, dispatch de case, actualización del acumulador,
  avance de puntero y exit test.
- **Tratá el lowering de switch como una elección.** Compare chain, jump table y
  conditional select son salidas válidas para la misma idea fuente.
- **Conectá campos con addressing modes.** Esta nota es donde
  [[lowlevel/assembly-and-compiler-output/addressing-modes-and-memory-operands|addressing modes y operandos de memoria]]
  vuelven a ser C fuente.

**Conecta con:** [[lowlevel/assembly-and-compiler-output/addressing-modes-and-memory-operands|Addressing modes y operandos de memoria]] · [[lowlevel/assembly-and-compiler-output/control-flow-jumps-conditions-and-flags|Control flow: jumps, condiciones y el registro de flags]] · [[lowlevel/assembly-and-compiler-output/system-v-amd64-calling-convention|La calling convention System V AMD64]] · [[lowlevel/c-from-the-metal/structs-unions-and-bitfields|Structs, unions y bitfields]] · [[lowlevel/pointers-and-memory/struct-layout-alignment-and-padding|Layout de structs: alineación y padding]]

## Fuentes

- **Intel 64 and IA-32 Architectures Software Developer's Manual**, Vol. 1 y Vol. 2 — semántica de instrucciones para branches, compares, loads, sign extension y multiplicación entera. https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- **System V AMD64 ABI** — registros de argumentos y expectativas de data layout usadas para leer la salida x86-64 generada. https://gitlab.com/x86-psABIs/x86-64-ABI
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 3 — loops, conditionals, switch statements y layout de structs a nivel máquina. https://csapp.cs.cmu.edu/
- **Felix Cloutier — x86 and amd64 instruction reference** — lookup rápido de `test`, `cmp`, `imul`, `movsxd` y jumps; verificá edge cases contra Intel SDM. https://www.felixcloutier.com/x86/
- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — reglas fuente para structs, indexing de arrays, conversiones enteras y semántica de switch. https://www.open-std.org/jtc1/sc22/wg14/
- **Arm Architecture Reference Manual + Apple ARM64 docs** — addressing load/store ARM64, `csel`, branches y detalles de ABI de plataforma para la salida del apéndice. https://developer.arm.com/documentation/ddi0487/latest · https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
