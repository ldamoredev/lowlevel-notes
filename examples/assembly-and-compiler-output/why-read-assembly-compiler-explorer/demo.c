// demo.c — ejemplo mínimo para comparar C contra la salida del compilador.
// Usá run.sh para emitir assembly x86-64 Intel y ARM64, y para correr el programa.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//   ./run.sh
#include <stdio.h>

long weighted_score(long packets, long cost) {
    return packets * 3 + cost * 5 + 7;
}

int main(void) {
    volatile long packets = 6;
    volatile long cost = 9;
    long result = weighted_score(packets, cost);

    printf("score = %ld\n", result);
    return 0;
}
