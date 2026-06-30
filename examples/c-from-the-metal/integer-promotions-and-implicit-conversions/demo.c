#include <stdint.h>
#include <stdio.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
static void compare_signed_and_unsigned(int left, unsigned int right) {
    printf("-1 < 1u              = %s\n", left < right ? "true" : "false");
}
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

int main(void) {
    uint8_t a = 250;
    uint8_t b = 10;
    int promoted_sum = a + b;
    uint8_t narrowed_sum = (uint8_t)(a + b);
    int negative = -1;
    unsigned int one = 1;
    unsigned int converted_negative = (unsigned int)negative;
    char plain = (char)0xFF;

    /* Los uint8_t se promueven a int antes de sumar. */
    printf("sizeof(a + b)        = %zu bytes\n", sizeof(a + b));
    printf("promoted sum          = %d\n", promoted_sum);
    printf("narrowed uint8_t sum  = %u\n", (unsigned)narrowed_sum);

    /* En una comparacion mixta, el int negativo se convierte a unsigned. */
    compare_signed_and_unsigned(negative, one);
    printf("(unsigned)-1         = %u\n", converted_negative);

    /* La signedness de char plano es implementation-defined. */
    printf("(char)0xFF as int     = %d\n", (int)plain);

    return 0;
}
