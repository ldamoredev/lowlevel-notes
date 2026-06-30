// demo.c — stack a nivel assembly: frame, slots locales y una llamada.
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

static NOINLINE long combine_slots(long first, long second, long sum, long diff) {
    return first + second + sum + diff;
}

long stack_example(long a, long b) {
    volatile long first = a;
    volatile long second = b;
    volatile long sum = a + b;
    volatile long diff = a - b;

    return combine_slots(first, second, sum, diff);
}

int main(void) {
    volatile long a = 21;
    volatile long b = 8;
    long result = stack_example(a, b);

    printf("stack result = %ld\n", result);
    return 0;
}
