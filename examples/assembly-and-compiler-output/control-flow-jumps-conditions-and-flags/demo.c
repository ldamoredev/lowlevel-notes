// demo.c — control flow: comparaciones, flags y saltos condicionales.
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

static NOINLINE long above_limit(long x, long limit) {
    return (x - limit) * 3;
}

static NOINLINE long at_or_below_limit(long x, long limit) {
    return (limit - x) * 2;
}

long branch_score(long x, long limit) {
    if (x > limit) {
        return above_limit(x, limit);
    }

    return at_or_below_limit(x, limit);
}

int main(void) {
    volatile long first = 17;
    volatile long second = 11;
    volatile long limit = 12;

    long a = branch_score(first, limit);
    long b = branch_score(second, limit);

    printf("scores = %ld, %ld\n", a, b);
    return 0;
}
