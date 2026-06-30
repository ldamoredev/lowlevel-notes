// demo.c — muestra la diferencia entre puntero a dato `const`, puntero `const`
// y APIs que prometen leer sin mutar.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>

static int sum_read_only(const int *items, size_t count) {
    int sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += items[i];
    }
    return sum;
}

static void reseat_pointer(int **slot, int *target) {
    *slot = target;
}

static void print_name(const char *name) {
    printf("name                  = %s\n", name);
}

int main(void) {
    int a = 10;
    int b = 20;
    int values[] = {1, 2, 3, 4};

    const int *read_only_view = &a;
    int *const fixed_pointer = &a;
    int *moving_pointer = &a;

    printf("read through const    = %d\n", *read_only_view);
    read_only_view = &b;
    printf("reseated const view   = %d\n", *read_only_view);

    *fixed_pointer = 11;
    printf("wrote via fixed ptr   = %d\n", a);

    reseat_pointer(&moving_pointer, &b);
    *moving_pointer = 21;
    printf("b after reseat write  = %d\n", b);

    printf("sum read-only array   = %d\n",
           sum_read_only(values, sizeof values / sizeof values[0]));

    print_name("low-level atlas");
    return 0;
}
