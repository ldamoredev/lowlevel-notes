// demo.c — addressing modes sobre un array de long.
// Usá run.sh para emitir assembly x86-64 Intel y ARM64, y para correr el programa.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//   ./run.sh
#include <stdio.h>

long touch_neighbors(long *xs, long i, long bias) {
    long current = xs[i];
    long ahead = xs[i + 2];
    long updated = current + bias;

    xs[i + 1] = updated;
    return updated + ahead;
}

int main(void) {
    long xs[6] = {2, 4, 6, 8, 10, 12};
    long result = touch_neighbors(xs, 2, 20);

    printf("xs[3] = %ld, result = %ld\n", xs[3], result);
    return 0;
}
