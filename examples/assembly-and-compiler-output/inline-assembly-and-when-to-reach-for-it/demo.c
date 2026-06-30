// demo.c — inline asm como contrato con el compilador, no como magia.
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

static NOINLINE long plain_twice(const long *value) {
    long first = *value;
    long second = *value;
    return first + second;
}

static NOINLINE long barrier_twice(const long *value) {
    long first = *value;
    __asm__ volatile("" ::: "memory");
    long second = *value;
    return first + second;
}

int main(void) {
    volatile long value = 21;
    const long *ptr = (const long *)&value;

    printf("plain   = %ld\n", plain_twice(ptr));
    printf("barrier = %ld\n", barrier_twice(ptr));
    return 0;
}
