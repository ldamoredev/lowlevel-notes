// demo.c — AArch64: registros de argumento, stack args y return value.
// Usá run.sh para emitir assembly ARM64 y x86-64, y para correr el programa local.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//   ./run.sh
#include <stdio.h>

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

NOINLINE long arm64_mix(long a, long b, long c, long d, long e,
                        long f, long g, long h, long i, long j) {
    return a + 2 * b - c + 3 * d + e - 4 * f + g + h - i + j;
}

int main(void) {
    volatile long a = 1;
    volatile long b = 2;
    volatile long c = 3;
    volatile long d = 4;
    volatile long e = 5;
    volatile long f = 6;
    volatile long g = 7;
    volatile long h = 8;
    volatile long i = 9;
    volatile long j = 10;

    long result = arm64_mix(a, b, c, d, e, f, g, h, i, j);
    printf("arm64 mix = %ld\n", result);
    return 0;
}
