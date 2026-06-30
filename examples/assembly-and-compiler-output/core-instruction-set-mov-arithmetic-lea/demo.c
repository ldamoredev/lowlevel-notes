// demo.c — instrucciones base: movimiento, aritmética y lea.
// Usá run.sh para emitir assembly x86-64 Intel y ARM64, y para correr el programa.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//   ./run.sh
#include <stdio.h>

long core_ops(long x, long y, long *out) {
    long product = x * y;
    long adjusted = product + x * 8;

    *out = product;
    return adjusted - y + 12;
}

int main(void) {
    volatile long x = 7;
    volatile long y = 5;
    long saved = 0;
    long result = core_ops(x, y, &saved);

    printf("product = %ld, result = %ld\n", saved, result);
    return 0;
}
