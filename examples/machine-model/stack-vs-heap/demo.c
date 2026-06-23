// demo.c — imprime una dirección de cada región del address space (código, data,
// bss, heap, stack) y muestra que el stack crece HACIA ABAJO al recursar.
// Compilá en -O0 para que los locales no se optimicen a registros:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//
// Tus direcciones exactas cambian en cada corrida por ASLR, pero el orden de las
// regiones y la dirección de crecimiento del stack se mantienen.
#include <stdio.h>
#include <stdlib.h>

int global_initialized = 42;   // .data
int global_zero;               // .bss

void show_stack_growth(int depth) {
    int local;                 // un slot de stack fresco en cada llamada
    printf("  depth %d: &local = %p\n", depth, (void*)&local);
    if (depth < 3) show_stack_growth(depth + 1);
}

int main(void) {
    int stack_var = 7;
    int *heap_var = malloc(sizeof(int));

    printf("code  (&main)        = %p\n", (void*)main);
    printf("data  (&global_init) = %p\n", (void*)&global_initialized);
    printf("bss   (&global_zero) = %p\n", (void*)&global_zero);
    printf("heap  (malloc)       = %p\n", (void*)heap_var);
    printf("stack (&stack_var)   = %p\n", (void*)&stack_var);
    printf("\nstack grows DOWN as we recurse:\n");
    show_stack_growth(0);

    free(heap_var);
    return 0;
}
