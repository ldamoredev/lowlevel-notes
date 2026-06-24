// demo.c — UB como contrato: no ejecutes "el caso malo"; chequealo antes.
// Compila limpio (sin warnings) con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//
// Este programa evita UB a proposito. Muestra tres bordes clasicos y la version
// definida: overflow chequeado, bounds antes de indexar, y memcpy para type punning.
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void checked_add_int(int a, int b) {
    int out = 0;

    if (__builtin_add_overflow(a, b, &out)) {
        printf("checked add: %d + %d would overflow a signed int (UB avoided)\n", a, b);
        return;
    }

    printf("checked add: %d + %d = %d\n", a, b, out);
}

static void read_index(const int *items, size_t count, size_t index) {
    if (index >= count) {
        printf("bounds check: index %zu is outside 0..%zu (UB avoided)\n",
               index, count - 1);
        return;
    }

    printf("bounds check: items[%zu] = %d\n", index, items[index]);
}

static void show_float_bits(float value) {
    uint32_t bits = 0;

    memcpy(&bits, &value, sizeof bits);
    printf("memcpy punning: %.1f has bits 0x%08X\n", value, (unsigned)bits);
}

int main(void) {
    int items[] = {10, 20, 30};

    checked_add_int(40, 2);
    checked_add_int(INT_MAX, 1);

    read_index(items, sizeof items / sizeof items[0], 1);
    read_index(items, sizeof items / sizeof items[0], 3);

    show_float_bits(1.0f);
    return 0;
}
