// demo.c — preprocessor: includes, macros y compilacion condicional.
// Compila limpio (sin warnings) con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//
// Proba tambien:
//
//   gcc -O0 -Wall -Wextra -DENABLE_TRACE=1 -DBUFFER_CAP=6 demo.c -o demo && ./demo
//
// El preprocessor corre antes del compilador: reemplaza tokens, incluye archivos
// y decide que bloques llegan al compilador.
#include <stdio.h>

#ifndef BUFFER_CAP
#define BUFFER_CAP 4
#endif

#ifndef ENABLE_TRACE
#define ENABLE_TRACE 0
#endif

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#if ENABLE_TRACE
#define TRACE(message) printf("trace: %s\n", (message))
#else
#define TRACE(message) ((void)0)
#endif

static int clamp_to_cap(int value) {
    return value > BUFFER_CAP ? BUFFER_CAP : value;
}

int main(void) {
    int samples[BUFFER_CAP] = {0};

    for (size_t i = 0; i < ARRAY_LEN(samples); i++) {
        samples[i] = (int)(i + 1) * 10;
    }

    printf("BUFFER_CAP value       = %d\n", BUFFER_CAP);
    printf("BUFFER_CAP as text     = %s\n", STRINGIFY(BUFFER_CAP));
    printf("ARRAY_LEN(samples)     = %zu\n", ARRAY_LEN(samples));
    printf("MAX(3, 7)              = %d\n", MAX(3, 7));
    printf("clamp_to_cap(9)        = %d\n", clamp_to_cap(9));

    TRACE("compiled only when ENABLE_TRACE is nonzero");

#if ENABLE_TRACE
    printf("trace mode             = compiled in\n");
#else
    printf("trace mode             = compiled out\n");
#endif

    return 0;
}
