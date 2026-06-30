// demo.c — mirá cómo seis argumentos enteros caen en registros.
// Usá run.sh para emitir assembly x86-64 Intel y ARM64, y para correr el programa.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//   ./run.sh
#include <stdio.h>

long mix_six(long a, long b, long c, long d, long e, long f) {
    long acc = a + b * 2;
    acc ^= c;
    acc += d * 4;
    acc -= e;
    return acc + f;
}

int main(void) {
    volatile long a = 10;
    volatile long b = 3;
    volatile long c = 5;
    volatile long d = 7;
    volatile long e = 11;
    volatile long f = 13;

    long result = mix_six(a, b, c, d, e, f);
    printf("mix = %ld\n", result);
    return 0;
}
