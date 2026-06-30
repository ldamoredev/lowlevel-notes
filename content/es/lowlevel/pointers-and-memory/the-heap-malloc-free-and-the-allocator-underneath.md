---
title: "El heap: malloc/free y el allocator por debajo"
description: La allocation en heap le da a los objetos de C un lifetime independiente de un stack frame. malloc le pide bytes crudos al allocator, free los devuelve y la corrección depende de ownership explícito.
tags: [heap, malloc, free, allocator, ownership]
order: 4
updated: 2026-06-30
---
# El heap: malloc/free y el allocator por debajo

El heap es memoria cuyo lifetime no está atado a una llamada de función. `malloc` le pide
al allocator de la biblioteca C un bloque de bytes crudos; `free` devuelve exactamente ese
bloque. Entre esas dos llamadas, sos dueño del bloque y podés tratarlo como storage para
objetos si respetás tamaño, alineación y reglas de tipo. El allocator por debajo no es
magia: mantiene metadata, reutiliza bloques liberados, pide chunks más grandes al sistema
operativo y puede volverse lento o corromperse por mala disciplina de ownership.

> El reset: `malloc` te da storage, no un objeto administrado. C no recuerda la longitud,
> no inicializa los bytes comunes de `malloc`, no llama destructores ni libera el bloque
> por vos. Ownership es la feature de runtime ausente que tenés que escribir en el
> programa.

## El contrato del heap

La API central es chica:

| Función | Contrato |
|---|---|
| `malloc(n)` | asigna `n` bytes; el contenido queda indeterminado; devuelve `NULL` si falla |
| `calloc(count, size)` | asigna `count * size` bytes y pone todos los bytes en cero; devuelve `NULL` si falla |
| `realloc(p, n)` | redimensiona un bloque asignado; puede moverlo; el puntero viejo queda inválido si tiene éxito |
| `free(p)` | libera un puntero devuelto por allocation; `free(NULL)` está permitido y no hace nada |

El puntero a objeto devuelto está alineado correctamente para tipos de objeto comunes, así
que esto es válido:

```c
int *items = malloc(count * sizeof *items);
```

Dos detalles en esa línea son intencionales. `sizeof *items` evita repetir el tipo, así la
allocation sigue correcta si `items` cambia de tipo. Y `count * sizeof *items` es una
cuenta de bytes, porque `malloc` trabaja en bytes aunque el puntero que recibís sea tipado.

Chequeá siempre el resultado antes de dereferenciar. En una workstation, allocations
chicas casi nunca fallan en programas de juguete. En código de sistemas, procesos de larga
vida, entornos restringidos, overcommit e input hostil vuelven al fallo parte del
contrato. Un chequeo contra `NULL` no es ceremonia; es la rama donde nunca adquiriste
ownership.

## Cómo funciona realmente

La mayoría de las llamadas a `malloc` no entran al kernel una por una. El allocator,
normalmente en libc, maneja una arena de heap dentro del address space del proceso.
Obtiene memoria del OS en chunks más grandes, comúnmente con mecanismos como
`brk`/`sbrk` históricamente y `mmap` para mappings grandes o separados en sistemas
Unix-like. Después sub-asigna pedazos a tu programa.

Para hacer eso, el allocator guarda bookkeeping metadata: tamaños de bloques, estado
libre/usado, bins o size classes y links entre bloques libres. Los allocators reales
optimizan tamaños comunes, comportamiento por thread, localidad y fragmentación. Un
bloque liberado puede reutilizarse inmediatamente en un `malloc` posterior, quedar en una
cache por thread, coalescerse con vecinos o devolverse al OS más tarde. No podés inferir
"la memoria desapareció" de `free`; solo podés inferir "este programa ya no posee ese
bloque."

El puntero devuelto por `malloc` apunta al payload usable, no a la metadata del allocator.
Escribir antes del inicio o después del final del bloque corrompe bytes en los que el
allocator puede depender. Por eso los heap overflows muchas veces fallan más tarde: la
escritura mala daña estado del allocator ahora, y un `malloc` o `free` posterior descubre
el desastre.

`realloc` es la parte más filosa de la API básica. Puede crecer in-place, mover el bloque a
otra dirección, achicarlo o fallar. Si tiene éxito, el puntero viejo ya no debe usarse. Si
falla, el bloque viejo sigue siendo tuyo y todavía tenés que liberarlo. Por eso el patrón
seguro usa un temporal:

```c
int *grown = realloc(items, new_count * sizeof *items);
if (grown == NULL) {
    free(items);
    return false;
}
items = grown;
```

Para objetos over-aligned, C provee `aligned_alloc` en estándares modernos, con reglas
extra sobre tamaño y alineación. `malloc` común alcanza para tipos de objeto normales;
casos especializados de SIMD, páginas, DMA o cache lines necesitan APIs explícitas de
alineación y su propia nota.

## Artefacto ejecutable: poseer, crecer, liberar

El demo vive en
`examples/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath/demo.c`.

```c
#include <stdio.h>
#include <stdlib.h>

struct Node {
    int value;
    struct Node *next;
};

static void print_ints(const char *label, const int *items, size_t count) {
    printf("%s", label);
    for (size_t i = 0; i < count; i++) {
        printf(" %d", items[i]);
    }
    printf("\n");
}

static void free_chain(struct Node *head) {
    while (head != NULL) {
        struct Node *next = head->next;
        free(head);
        head = next;
    }
}

int main(void) {
    size_t count = 4;
    int *numbers = malloc(count * sizeof *numbers);

    if (numbers == NULL) {
        perror("malloc numbers");
        return 1;
    }

    for (size_t i = 0; i < count; i++) {
        numbers[i] = (int)((i + 1) * 10);
    }

    printf("malloc numbers        = %p\n", (void *)numbers);
    print_ints("numbers initial       =", numbers, count);

    size_t bigger_count = 6;
    int *grown = realloc(numbers, bigger_count * sizeof *numbers);
    if (grown == NULL) {
        free(numbers);
        perror("realloc numbers");
        return 1;
    }

    numbers = grown;
    numbers[4] = 50;
    numbers[5] = 60;
    count = bigger_count;

    printf("realloc numbers       = %p\n", (void *)numbers);
    print_ints("numbers grown         =", numbers, count);

    size_t zero_count = 3;
    int *zeros = calloc(zero_count, sizeof *zeros);
    if (zeros == NULL) {
        free(numbers);
        perror("calloc zeros");
        return 1;
    }

    print_ints("calloc zeros          =", zeros, zero_count);

    struct Node *first = malloc(sizeof *first);
    struct Node *second = malloc(sizeof *second);
    if (first == NULL || second == NULL) {
        free(first);
        free(second);
        free(zeros);
        free(numbers);
        perror("malloc node");
        return 1;
    }

    first->value = 1;
    first->next = second;
    second->value = 2;
    second->next = NULL;

    printf("node chain            = %d -> %d\n",
           first->value, first->next->value);

    free_chain(first);
    free(zeros);
    free(numbers);

    first = NULL;
    zeros = NULL;
    numbers = NULL;

    printf("after free owners     = %s, %s, %s\n",
           first == NULL ? "NULL" : "live",
           zeros == NULL ? "NULL" : "live",
           numbers == NULL ? "NULL" : "live");

    return 0;
}
```

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Una salida real:

```
malloc numbers        = 0x7fc147804080
numbers initial       = 10 20 30 40
realloc numbers       = 0x7fc147804080
numbers grown         = 10 20 30 40 50 60
calloc zeros          = 0 0 0
node chain            = 1 -> 2
after free owners     = NULL, NULL, NULL
```

`realloc` justo mantuvo la misma dirección en esta corrida; el código no debe depender de
eso. El patrón seguro sigue siendo `grown = realloc(numbers, ...)`, chequear `grown` y
recién entonces reemplazar el owner. `calloc` produjo bytes en cero, que acá aparecen como
enteros cero. La última línea pone las variables dueñas en `NULL` después de `free`; eso
no invalida aliases en otros lados, pero previene reutilización accidental por estos
nombres.

## Modos de falla y trade-offs

- **Olvidar liberar.** Un leak no es "algunos bytes perdidos"; es ownership sin camino de
  release. En un proceso de larga vida, leaks repetidos se vuelven un bug de confiabilidad.
- **Double-free.** Llamar `free` dos veces sobre la misma allocation corrompe estado del
  allocator. Poner una variable dueña en `NULL` ayuda solo si no hay otros aliases.
- **Use-after-free.** Después de `free(p)`, todo valor de puntero hacia ese bloque es
  inválido para acceso. El allocator puede reutilizar los bytes para otro objeto en
  cualquier momento.
- **Overflow en matemática de tamaños.** `count * sizeof *p` puede wrappear `size_t` antes
  de que `malloc` lo vea, asignando un bloque más chico que el loop escribirá después. Las
  implementaciones de `calloc` deben detectar overflow de multiplicación; la matemática
  manual de tamaños para `malloc` necesita chequeos cuando los counts vienen de input.
- **Asumir que `malloc` pone en cero.** No lo hace. Usá `calloc` para bytes en cero, o
  inicializá vos cada campo.
- **Fragmentación y costo del allocator.** La allocation en heap es flexible, pero cada
  llamada corre bookkeeping, puede tomar locks, puede perder cache y puede fragmentar
  memoria. Los hot paths suelen batchear, reutilizar o pasar a allocators custom más
  adelante en esta rama.

## En la práctica

- **Escribí quién es el owner.** Cada puntero de heap debería tener un owner claro
  responsable de exactamente un `free`.
- **Emparejá puntero con longitud.** El allocator recuerda internamente el tamaño del
  bloque, pero C no lo expone portablemente a tu programa. Mantené `ptr + count` juntos.
- **Usá el patrón de temporal con `realloc`.** Nunca hagas `items = realloc(items, n)`
  salvo que estés dispuesto a filtrar el bloque viejo si falla.
- **Inicializá antes de publicar.** Asigná, llená cada campo y recién después entregá el
  puntero al resto del programa.
- **Liberá con la forma inversa del ownership.** Para estructuras enlazadas, árboles y
  arrays de punteros owned, escribí una función de cleanup que espeje la construcción.
- **Usá sanitizers y leak checkers.** AddressSanitizer, LeakSanitizer y Valgrind convierten
  muchos bugs de lifetime de heap de "crash random más tarde" a falla local de test.

**Conecta con:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Aritmética de punteros y stride]] · [[lowlevel/pointers-and-memory/the-stack-frames-locals-and-automatic-storage|El stack: frames, locales y automatic storage]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/machine-model/stack-vs-heap|Stack vs heap]] · [[lowlevel/machine-model/process-address-space-and-virtual-memory|Address space de proceso y memoria virtual]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — la autoridad para allocated storage duration, `malloc`, `calloc`, `realloc`, `free`, null pointers y reglas de lifetime de objetos. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Dynamic memory management** — referencia compacta para las APIs de allocation de C, comportamiento de falla, `free(NULL)` y reglas de `realloc`. https://en.cppreference.com/w/c/memory
- **Bryant & O'Hallaron — *Computer Systems: A Programmer's Perspective* (CS:APP)**, capítulo de dynamic memory allocation — internals de allocator: free lists implícitas/explícitas, coalescing, splitting y fragmentación. https://csapp.cs.cmu.edu/
- **Doug Lea — *A Memory Allocator*** — notas clásicas de diseño de allocator desde dlmalloc: bins, coalescing, localidad, tunability y trade-offs. https://gee.cs.oswego.edu/dl/html/malloc.html
- **Dan Luu — *Malloc tutorial*** — walkthrough de un allocator chico que vuelve concreta la metadata y el crecimiento de heap estilo `sbrk`. https://danluu.com/malloc-tutorial/
- **Clang AddressSanitizer / LeakSanitizer documentation** — herramientas prácticas para detectar heap buffer overflows, use-after-free, double-free y leaks en tests. https://clang.llvm.org/docs/AddressSanitizer.html
