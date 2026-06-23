// demo.c — IEEE 754 en acción: la desigualdad 0.1 + 0.2 != 0.3, el layout de bits
// (sign/exponent/mantissa) de un float, y los valores especiales (inf, NaN).
// Necesita -lm por las funciones de <math.h>:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo -lm && ./demo
//
// memcpy a un uint32_t es la forma portable de ver los bits (un cast float*->int*
// violaría strict aliasing).
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>

static void show_float(const char *name, float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof bits);
    uint32_t sign = bits >> 31;
    uint32_t exp  = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF;
    printf("%-6s = %-10g 0x%08X  sign=%u exp=%u(2^%d) mant=0x%06X\n",
           name, (double)f, bits, sign, exp, (int)exp - 127, mant);
}

int main(void) {
    printf("0.1 + 0.2 = %.17f\n", 0.1 + 0.2);
    printf("0.3       = %.17f\n", 0.3);
    printf("equal? %s\n\n", (0.1 + 0.2 == 0.3) ? "yes" : "NO");

    show_float("1.0", 1.0f);
    show_float("0.5", 0.5f);
    show_float("-2.0", -2.0f);
    show_float("0.1", 0.1f);                     // la mantissa se repite: inexacto

    float inf = 1.0f / 0.0f, nan = 0.0f / 0.0f;
    printf("\n1.0/0.0 = %g (isnan %d)\n", (double)inf, isnan(inf));
    printf("0.0/0.0 = %g (isnan %d)\n", (double)nan, isnan(nan));
    printf("nan == nan ? %s\n", (nan == nan) ? "true" : "false");

    printf("\ntolerant compare: fabs((0.1+0.2)-0.3) < 1e-9 ? %s\n",
           fabs((0.1 + 0.2) - 0.3) < 1e-9 ? "true" : "false");
    return 0;
}
