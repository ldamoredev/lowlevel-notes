// demo.c — compara salida -O0 vs -O2 para una función chica.
// Usá run.sh para emitir assembly x86-64 Intel y ARM64, y para correr el programa.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//   ./run.sh
#include <stdio.h>

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

NOINLINE long optimize_me(long x, long y) {
    long twice = x * 2;
    long dead = y * 0;
    long folded = 40 + 2;

    if (twice > y) {
        return twice + folded + dead;
    }

    return y - twice + folded + dead;
}

int main(void) {
    volatile long x = 9;
    volatile long y = 11;

    printf("optimized result = %ld\n", optimize_me(x, y));
    return 0;
}
