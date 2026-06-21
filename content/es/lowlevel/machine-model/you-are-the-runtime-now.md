---
title: "Ahora el runtime sos vos"
description: El runtime administrado hacía bounds checks, liberaba tu memoria, normalizaba tus enteros y atrapaba tus errores de tipo. El bare metal saca ese piso — la CPU ejecuta tus bytes exactamente, incluido lo que está mal. Este es el reset de todo el atlas.
tags: [machine-model, undefined-behavior, mental-model, foundations]
order: 1
updated: 2026-06-21
featured: true
---
# Ahora el runtime sos vos

Durante toda tu carrera hubo una capa de software entre tu código y la máquina que
limpiaba en silencio lo que vos dejabas sucio. Chequeaba cada índice de array,
liberaba la memoria que te olvidabas, evitaba que los enteros hicieran algo
sorprendente y convertía tus errores en excepciones prolijas con stack trace. Esa
capa — el motor de JavaScript, la JVM, el CLR de .NET, el intérprete de Python — es el
**runtime administrado**, y bajar a bajo nivel es sacarlo. De acá para abajo no hay
piso: la CPU ejecuta los bytes que emitió tu compilador, exactamente, a unos pocos
nanosegundos cada uno, incluidos los bytes que codifican un error.

> El reset: ya nadie te cubre la espalda. Un bug en código administrado es un test en
> rojo o una excepción atrapada. Un bug acá puede corromper memoria en silencio,
> devolver basura del stack de la semana pasada, o desaparecer del todo bajo el
> optimizador — y todavía "andar en tu máquina".

## Qué hacía el runtime por vos

Todo lo de esta columna era un *servicio*, pagado en CPU y memoria, en el que nunca
tuviste que pensar. Programar a bajo nivel te devuelve cada uno.

| El runtime hacía esto por vos | En bare metal, es tu trabajo |
|---|---|
| Chequeaba los límites de cada acceso a array | Nada chequea un índice — fuera de rango lee/escribe lo que haya ahí |
| Liberaba memoria sin usar con el garbage collector | Vos liberás (`free`) lo que pediste (`malloc`), una vez, en el momento justo |
| Normalizaba la matemática de enteros (o usaba bignums) | Enteros de ancho fijo; el overflow con signo es **undefined**, no "da la vuelta" |
| Inicializaba en cero variables y campos | Una variable local tiene los bytes que quedaron en el stack |
| Imponía los tipos en runtime | Un tipo es una *instrucción de layout*; reinterpretá los bytes y la máquina obedece |
| Convertía fallas en excepciones | Un puntero malo es un segfault — o peor, corrupción silenciosa sin falla alguna |
| Hacía de `null` un valor chequeado que tira excepción | Desreferenciar null es undefined behavior que el compilador puede *asumir que nunca pasa* |

Nada de esto es la CPU siendo hostil. La CPU no tiene noción de "array", "objeto" ni
"tipo". Tiene registros, direcciones e instrucciones. Esas abstracciones vivían
enteramente en el runtime que acabás de sacar.

## El contrato nuevo, hecho concreto

Acá está el mismo programita dos veces. Primero en TypeScript, donde el runtime
cumple cada promesa:

```ts
const a = [10, 20, 30];
console.log(a[5]);              // undefined — comportamiento definido, sin crash ni basura

let x = Number.MAX_SAFE_INTEGER;
console.log(x + 1);            // un número mal-pero-definido; nunca undefined behavior

let u: number;
console.log(u);               // el compilador se niega: "used before being assigned" (TS2454)
```

Fuera de rango te da `undefined`. La matemática que pierde precisión te da un
resultado *definido, aunque impreciso*. Leer una variable sin asignar se atrapa antes
de que el código corra siquiera. Ahora la misma forma en C, donde ninguna de esas
garantías existe:

```c
#include <stdio.h>
#include <limits.h>

int main(void) {
    int a[3] = {10, 20, 30};

    printf("a[5]   = %d\n", a[5]);     // (1) lectura fuera de rango: undefined behavior
    int x = INT_MAX;
    printf("x + 1  = %d\n", x + 1);    // (2) overflow con signo: undefined behavior
    int u;
    printf("u      = %d\n", u);        // (3) lectura sin inicializar: valor indeterminado
    return 0;
}
```

Compilalo y corrélo de la forma ingenua y *parece* andar — imprime tres números y
sale con 0. Esa es la trampa. "Corrió" no es "está bien". Cada una de esas tres líneas
es un bug que la máquina no tenía obligación de atrapar:

1. `a[5]` lee 8 bytes más allá de un array de 3 elementos — lo que sea que haya en el
   stack ahí. Hoy un valor viejo; mañana, tras un refactor, un crash.
2. `INT_MAX + 1` es **undefined behavior**, no "da la vuelta a `INT_MIN`". El estándar
   dice que el overflow con signo no puede pasar, así que el optimizador *tiene
   derecho a asumir que nunca pasa* — y va a borrar los chequeos que escribiste para
   protegerte de eso.
3. `u` se lee antes de tener un valor. Imprime los bytes que dejó en el stack una
   llamada anterior.

El punto del atlas es que no *adivinás* sobre esto. Construís las herramientas para
*verlo*. Compilá la versión en C con los sanitizers y lo invisible se vuelve ruidoso:

```bash
gcc -Wall -Wextra -fsanitize=address,undefined -g demo.c -o demo && ./demo
```

```text
demo.c:8:29: runtime error: signed integer overflow: 2147483647 + 1 cannot be
             represented in type 'int'
==12345==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x...
    READ of size 4 at 0x... thread T0
        #0 ... in main demo.c:6
```

El mismo programa, la misma máquina — lo único que cambió es que le pediste a la
herramienta correcta que mire. Ese paso, de "corrió" a "puedo ver exactamente qué
hizo", es toda la disciplina de este atlas.

## Por qué "undefined" es la palabra que importa

Viniendo de un lenguaje administrado, el instinto es leer "undefined behavior"
(comportamiento indefinido) como "implementation-defined" o "lo que haga el
hardware". No es ni una cosa ni la otra. **El undefined behavior es una promesa que
*vos* le hacés al compilador de que algo nunca va a pasar.** A cambio, el compilador
optimiza como si fuera cierto. El overflow con signo no va a pasar, así que se puede
probar que el límite de un loop es finito. Un puntero que desreferenciaste no puede
ser null, así que el chequeo de null tres líneas después es código muerto y se borra.
Cuando rompés la promesa, el resultado no es un número equivocado — es que el
razonamiento del compilador se construyó sobre una premisa falsa, y *cualquier cosa*
aguas abajo ya no tiene sentido. Por eso los bugs de UB son tan escurridizos: muchas
veces andan en `-O0`, se rompen en `-O2`, y cambian de comportamiento cuando agregás
un `printf` que no tiene nada que ver.

## En la práctica

- **"Anda en mi máquina" es la frase más peligrosa acá.** El undefined behavior no es
  aleatorio — es la *ausencia de toda garantía*. Puede quedarse invisible meses y
  después aparecer como corrupción de datos en producción tras actualizar el compilador.
- **Ahora sos responsable de cuatro cosas que tenía el runtime:** el ciclo de vida de
  la memoria (quién libera, cuándo), los límites (cada índice, cada puntero), los
  rangos de enteros (overflow, truncamiento, signo) y la inicialización (no leer bytes
  indeterminados).
- **El arreglo no es el miedo — es tooling y disciplina.** Los sanitizers, `-Wall
  -Wextra`, valgrind y los tests son cómo seguís yendo rápido en C sin volar a ciegas.
  Eso no es un apéndice del trabajo de bajo nivel; es [[lowlevel/craftsmanship-low-level/index|una parte de primera clase]].
- Esta rama es casi sin código justamente porque el resto recién tiene sentido una vez
  que aterrizó este reset: acá abajo no hay runtime. **Ahora el runtime sos vos.**

**Conecta con:** [[lowlevel/machine-model/index|Modelo de Máquina]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/craftsmanship-low-level/index|Craftsmanship de Bajo Nivel]]

## Fuentes

- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, cap. 1–3 — cómo los programas mapean a datos, registros y código máquina; la base de "no hay runtime". csapp.cs.cmu.edu
- **Chris Lattner — *What Every C Programmer Should Know About Undefined Behavior*** (blog de LLVM, 3 partes) — por qué el optimizador trata el UB como premisa, con borrados concretos. blog.llvm.org/2011/05/what-every-c-programmer-should-know.html
- **John Regehr — *A Guide to Undefined Behavior in C and C++*** — el recorrido canónico de qué es el UB y por qué muerde. blog.regehr.org/archives/213
- **cppreference — *Undefined behavior*** — la lista precisa, directo del modelo del estándar. en.cppreference.com/w/c/language/behavior
- **Charles Petzold — *Code*** — construye la máquina desde abajo para que "la CPU solo ejecuta bytes" deje de ser abstracto.
