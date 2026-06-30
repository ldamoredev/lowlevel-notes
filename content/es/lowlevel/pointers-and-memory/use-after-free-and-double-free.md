---
title: "Bug clásico: use-after-free y double-free"
description: `free` termina el lifetime de una asignación, no todos los valores de puntero que todavía nombran su dirección vieja. Use-after-free y double-free son fallas de ownership que convierten reutilización del allocator en undefined behavior.
tags: [use-after-free, double-free, ownership, heap, undefined-behavior]
order: 12
updated: 2026-06-30
---
# Bug clásico: use-after-free y double-free

`free(p)` no borra el número guardado en `p`. Termina el lifetime de la asignación del heap
que `p` designaba. Cualquier copia de puntero que todavía contenga esa dirección queda
stale, y usarla es undefined behavior. Un double-free es la misma falla de ownership vista
desde el allocator: le pediste que recupere storage que ya no es una asignación viva.

> El reset: igualdad de punteros no es liveness de objeto. Una dirección puede parecer
> familiar y aun así ya no ser tuya.

## Cómo funciona realmente

La interfaz del heap de C es chica:

```c
void *malloc(size_t size);
void free(void *ptr);
```

`malloc` devuelve un puntero a storage asignado, o `NULL`. `free` acepta `NULL` o un valor
de puntero previamente devuelto por una función de asignación y todavía no liberado.
Después de un `free` exitoso, el lifetime de la asignación terminó. Leer mediante el
puntero viejo, escribir mediante él, pasarlo de vuelta a `free` o usarlo como si todavía
poseyera storage es undefined behavior.

El allocator normalmente guarda metadata alrededor de chunks: tamaños, bits de estado,
links de free-list o bins. Un double-free puede corromper esas estructuras. Un
use-after-free puede ser peor porque el allocator puede reciclar rápido la misma dirección
para otro objeto. Tu puntero stale entonces escribe en un owner nuevo. En seguridad, eso
puede convertirse en memory corruption, type confusion, control-flow hijacking o una fuga
de información. En sistemas comunes, muchas veces se vuelve corrupción intermitente de
estado que aparece lejos de la línea que liberó memoria.

Poner un puntero en `NULL` después de `free` sirve solo para la variable owner que
limpiás. Hace seguro destruir repetidamente mediante ese owner porque `free(NULL)` está
definido como no-op. No arregla otros alias:

```c
int *a = malloc(sizeof *a);
int *b = a;
free(a);
a = NULL;
*b = 7;      /* alias stale: sigue siendo undefined behavior */
```

La corrección más profunda es disciplina de ownership. En cada momento decidí qué
variable o estructura posee la asignación, qué punteros solo la toman prestada y cuándo
todos esos punteros prestados dejan de ser válidos.

AddressSanitizer vuelve esto visible rodeando asignaciones con red zones y muchas veces
poniendo chunks liberados en cuarentena antes de reutilizarlos. Esa es una estrategia de
debugging, no el modelo de ejecución de C. En un build normal, el allocator puede reusar
el bloque inmediatamente.

## Artefacto ejecutable: un destructor que limpia ownership

El demo vive en `examples/pointers-and-memory/use-after-free-and-double-free/demo.c`.

```c
// demo.c — muestra ownership de heap con un destructor que llama a `free`, limpia
// el puntero owner y vuelve segura una segunda destruccion.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>
#include <stdlib.h>

struct Buffer {
    int *data;
    size_t count;
};

static int buffer_init(struct Buffer *buffer, size_t count) {
    buffer->data = malloc(count * sizeof *buffer->data);
    if (buffer->data == NULL) {
        buffer->count = 0;
        return -1;
    }

    buffer->count = count;
    for (size_t i = 0; i < count; i++) {
        buffer->data[i] = (int)(i + 1);
    }

    return 0;
}

static void buffer_destroy(struct Buffer *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->count = 0;
}

static int buffer_sum(const struct Buffer *buffer) {
    int sum = 0;
    for (size_t i = 0; i < buffer->count; i++) {
        sum += buffer->data[i];
    }
    return sum;
}

int main(void) {
    struct Buffer buffer = {0};

    if (buffer_init(&buffer, 4) != 0) {
        perror("buffer_init");
        return 1;
    }

    printf("sum while alive       = %d\n", buffer_sum(&buffer));
    printf("owner before destroy  = %s\n",
           buffer.data != NULL ? "live" : "NULL");

    buffer_destroy(&buffer);
    printf("owner after destroy   = %s count=%zu\n",
           buffer.data == NULL ? "NULL" : "live", buffer.count);

    buffer_destroy(&buffer);
    printf("second destroy safe   = %s count=%zu\n",
           buffer.data == NULL ? "NULL" : "live", buffer.count);

    return 0;
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Salida real:

```
sum while alive       = 10
owner before destroy  = live
owner after destroy   = NULL count=0
second destroy safe   = NULL count=0
```

El demo no muestra el bug disparando undefined behavior. En cambio, muestra el patrón
local que previene una clase de double-free: poné el puntero owner y su tamaño en un
struct, liberá mediante un único destructor y después reseteá el owner a estado vacío.
Cualquier puntero prestado igual tiene que invalidarse por diseño de API.

## Modos de falla y trade-offs

- **Aliases que sobreviven al owner.** Limpiar el puntero owner no limpia copias guardadas
  en otro lado.
- **Liberar mediante el puntero equivocado.** `free` necesita el puntero devuelto por el
  allocator, no un puntero al medio del objeto.
- **Dos owners para una asignación.** Si dos structs creen que deben llamar a `free`, uno
  de los dos eventualmente se equivoca.
- **Reusar memoria liberada "porque todavía imprime bien."** Los bytes liberados pueden
  quedar iguales hasta que el allocator los reutilice. Eso vuelve temporal al bug.
- **Error paths.** Early returns suelen saltar cleanup o ejecutar cleanup dos veces salvo
  que ownership tenga una sola disciplina de salida.
- **Confianza solo en debug.** ASan, checks del allocator y mallocs endurecidos atrapan
  muchos bugs, pero no prueban que todos los bordes de lifetime sean correctos.

## En la práctica

- **Nombrá ownership en el tipo.** Un `struct Buffer` con `buffer_destroy` es más claro que
  un `int *` desnudo pasado por muchas funciones.
- **Hacé cortos los punteros prestados.** No guardes borrows a través de llamadas que
  pueden liberar, redimensionar o resetear el owner.
- **Destruí a estado vacío.** Poné punteros owner en `NULL` y tamaños en cero después del
  cleanup.
- **Usá un único camino de cleanup.** En C, un bloque `goto cleanup` suele ser más claro y
  seguro que frees duplicados en varios returns.
- **Corré sanitizers durante desarrollo.** Compilá código sospechoso con
  `-fsanitize=address,undefined -fno-omit-frame-pointer` cuando tu toolchain lo soporte.
- **Documentá transferencias.** Las APIs deberían decir si el caller conserva ownership,
  lo entrega o solo presta un puntero durante la llamada.

**Conecta con:** [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|El heap: `malloc`/`free` y el allocator por debajo]] · [[lowlevel/pointers-and-memory/memory-leaks-and-ownership-discipline|Memory leaks y disciplina de ownership]] · [[lowlevel/pointers-and-memory/buffer-overflow-and-out-of-bounds-access|Buffer overflow y acceso out-of-bounds]] · [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]]

## Fuentes

- **ISO WG14 N1570 — draft de C11** — reglas estándar para allocated storage duration, validez de punteros y undefined behavior alrededor de `free`. https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
- **cppreference — `free`** — contrato conciso de `free`, incluyendo `NULL` y punteros ya liberados. https://en.cppreference.com/w/c/memory/free
- **Documentación de Clang AddressSanitizer** — modelo práctico de detección para use-after-free, double-free y heap red zones. https://clang.llvm.org/docs/AddressSanitizer.html
- **MITRE CWE-416: Use After Free** — taxonomía de seguridad y consecuencias de punteros stale al heap. https://cwe.mitre.org/data/definitions/416.html
- **MITRE CWE-415: Double Free** — taxonomía de seguridad para liberar la misma memoria dos veces. https://cwe.mitre.org/data/definitions/415.html
- **Manual de Valgrind Memcheck** — comportamiento de análisis dinámico para frees inválidos y accesos heap inválidos. https://valgrind.org/docs/manual/mc-manual.html
