// demo.c — System V AMD64 calling convention: registers, stack args, return.
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

static NOINLINE long abi_mix(long a, long b, long c, long d,
                             long e, long f, long g, long h) {
    return a + 2 * b - c + 3 * d + e - 4 * f + g + h;
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

    long result = abi_mix(a, b, c, d, e, f, g, h);
    printf("abi mix = %ld\n", result);
    return 0;
}
