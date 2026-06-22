// demo.c — trabajo idéntico, órdenes de recorrido opuestos: la misma matriz sumada
// por filas (row-major, stride 1) vs por columnas (column-major, stride N).
// Mismas operaciones, ~25x de diferencia, porque la cache mueve LÍNEAS de 64 bytes:
// row-major usa los 16 int de cada línea; col-major usa 1 y desperdicia 15.
//
//   gcc -O2 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(void) {
    const int N = 8192;                       // 8192 x 8192 ints = 256 MB
    int *m = malloc((size_t)N * N * sizeof(int));
    if (!m) { perror("malloc"); return 1; }
    for (long i = 0; i < (long)N * N; i++) m[i] = 1;

    volatile long sink = 0;
    double t0 = now_ms();
    long s = 0;
    for (int i = 0; i < N; i++)               // row-major: stride 1, recorre cada línea
        for (int j = 0; j < N; j++) s += m[(long)i * N + j];
    double t1 = now_ms(); sink = s;

    s = 0;
    for (int j = 0; j < N; j++)               // column-major: stride N, línea nueva por paso
        for (int i = 0; i < N; i++) s += m[(long)i * N + j];
    double t2 = now_ms(); sink = s;

    printf("row-major  (stride 1) : %8.1f ms\n", t1 - t0);
    printf("col-major  (stride N) : %8.1f ms\n", t2 - t1);
    printf("slowdown              : %8.1fx\n", (t2 - t1) / (t1 - t0));
    return sink == -1;
}
