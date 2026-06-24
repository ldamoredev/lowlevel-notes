// demo.c — tres bordes de UB y sus versiones definidas.
// Compila limpio (sin warnings) con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//
// No ejecutes overflow signed, type punning con punteros incompatibles ni
// modificaciones no secuenciadas. Chequea, copia bytes, y separa los efectos.
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void checked_mul_int(int a, int b) {
    int out = 0;

    if (__builtin_mul_overflow(a, b, &out)) {
        printf("signed overflow avoided: %d * %d does not fit in int\n", a, b);
        return;
    }

    printf("checked multiply: %d * %d = %d\n", a, b, out);
}

static void show_float_bits(float value) {
    uint32_t bits = 0;

    memcpy(&bits, &value, sizeof bits);
    printf("alias-safe punning: %.2f -> 0x%08X\n", value, (unsigned)bits);
}

static void sequenced_update(int start) {
    int i = start;
    int left = i;
    i++;
    int right = i;
    i++;

    printf("sequenced effects: start=%d left=%d right=%d final=%d sum=%d\n",
           start, left, right, i, left + right);
}

int main(void) {
    checked_mul_int(1000, 20);
    checked_mul_int(INT_MAX, 2);

    show_float_bits(1.50f);

    sequenced_update(3);
    return 0;
}
