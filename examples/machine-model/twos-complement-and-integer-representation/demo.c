// demo.c — complemento a dos en acción: patrones de bits, negación (~x + 1),
// wraparound unsigned y la trampa de comparación signed/unsigned.
// Compilá limpio (sin warnings) con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//
// Los casts (unsigned)a son a propósito: escribir la comparación como `a > b`
// directo es justo lo que marca -Wsign-compare, que es la lección.
#include <stdio.h>
#include <limits.h>

static void bits32(const char *name, unsigned int v) {
    printf("%-8s = %11d  0x%08X  ", name, (int)v, v);
    for (int i = 31; i >= 0; i--) {
        putchar((v >> i) & 1 ? '1' : '0');
        if (i % 8 == 0) putchar(' ');
    }
    putchar('\n');
}

int main(void) {
    printf("INT_MAX=%d  INT_MIN=%d  UINT_MAX=%u\n\n", INT_MAX, INT_MIN, UINT_MAX);

    bits32("0", 0);
    bits32("1", 1);
    bits32("-1", (unsigned)-1);                 // todos unos
    bits32("INT_MAX", (unsigned)INT_MAX);       // 0x7FFFFFFF
    bits32("INT_MIN", (unsigned)INT_MIN);       // 0x80000000

    int x = 5;
    printf("\nnegate 5 via ~x + 1 = %d\n", ~x + 1);

    unsigned u = 0;
    printf("unsigned 0u - 1     = %u   (wraps to UINT_MAX)\n", u - 1);

    int a = -1; unsigned b = 0;                 // la trampa de conversión
    printf("in (a > b): (unsigned)(-1) = %u, so a > b is %s\n",
           (unsigned)a, ((unsigned)a > b) ? "true (!)" : "false");
    return 0;
}
