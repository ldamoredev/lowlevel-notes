// demo.c — observá crecer la latencia por acceso a medida que el working set
// deja cada nivel de cache (L1 → L2 → L3 → RAM).
//
//   gcc -O2 -Wall -Wextra demo.c -o demo && ./demo
//
// Dos detalles son los que lo hacen funcionar, y los dos son fáciles de errar:
//   1. El ciclo ALEATORIO de Sattolo derrota al prefetcher del hardware. Un anillo
//      secuencial (i+1)%n se queda en ~2 ns aun a 64 MB porque la CPU lo predice.
//   2. La cadena dependiente idx = a[idx] + el sink `volatile` impiden que -O2
//      solape las cargas o borre el loop entero por dead-code elimination.
// Sacá cualquiera de los dos y la curva se aplana y oculta el efecto.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    const long ACCESSES = 256L * 1024 * 1024;   // trabajo fijo por tamaño
    volatile int sink = 0;
    printf("%10s  %12s\n", "size(KB)", "ns/access");

    for (long kb = 4; kb <= 64L * 1024; kb *= 2) {
        long n = kb * 1024 / (long)sizeof(int);
        int *a = malloc((size_t)n * sizeof(int));
        if (!a) { perror("malloc"); return 1; }

        // Armá UN solo ciclo aleatorio grande (algoritmo de Sattolo).
        for (long i = 0; i < n; i++) a[i] = (int)i;
        for (long i = n - 1; i > 0; i--) {
            long j = rand() % i;
            int t = a[i]; a[i] = a[j]; a[j] = t;
        }

        int idx = 0;
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (long i = 0; i < ACCESSES; i++) idx = a[idx];   // chase dependiente
        clock_gettime(CLOCK_MONOTONIC, &t1);

        double ns = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
        printf("%10ld  %12.2f\n", kb, ns / (double)ACCESSES);
        sink = idx;          // fuerza a observar el chase (evita dead-code elimination)
        free(a);
    }
    return sink == -1;
}
