// demo.c — muestra `void*` como transporte de direcciones: vistas borradas,
// callback de `qsort`, dump de bytes y copia con `memcpy`.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AnyView {
    const void *data;
    size_t size;
    void (*print)(const void *data);
};

static void print_int(const void *data) {
    const int *value = data;
    printf("int view              = %d\n", *value);
}

static void print_c_string(const void *data) {
    const char *const *text = data;
    printf("string view           = %s\n", *text);
}

static int compare_ints(const void *left, const void *right) {
    const int *a = left;
    const int *b = right;
    return (*a > *b) - (*a < *b);
}

static void dump_bytes(const void *data, size_t size) {
    const unsigned char *bytes = data;

    printf("raw bytes             =");
    for (size_t i = 0; i < size; i++) {
        printf(" %02x", bytes[i]);
    }
    printf("\n");
}

int main(void) {
    int number = 0x12345678;
    const char *word = "atlas";

    struct AnyView views[] = {
        {&number, sizeof number, print_int},
        {&word, sizeof word, print_c_string},
    };

    for (size_t i = 0; i < sizeof views / sizeof views[0]; i++) {
        views[i].print(views[i].data);
    }

    dump_bytes(&number, sizeof number);

    int values[] = {40, 10, 30, 20};
    qsort(values, sizeof values / sizeof values[0], sizeof values[0], compare_ints);

    printf("qsort result          =");
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        printf(" %d", values[i]);
    }
    printf("\n");

    void *erased = malloc(sizeof number);
    if (erased == NULL) {
        perror("malloc");
        return 1;
    }

    memcpy(erased, &number, sizeof number);
    int *restored = erased;
    printf("restored from void*   = 0x%x\n", *restored);

    free(erased);
    return 0;
}
