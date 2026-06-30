// demo.c — stack frames: prologue, epilogue, locals, and a real call.
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

static NOINLINE long add_bias(long value) {
    return value + 7;
}

long framed_total(long a, long b) {
    volatile long local = a + b;
    long adjusted = add_bias(local);

    return adjusted + local;
}

int main(void) {
    volatile long a = 14;
    volatile long b = 5;

    printf("framed total = %ld\n", framed_total(a, b));
    return 0;
}
