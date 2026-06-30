// demo.c — muestra type punning portable con `memcpy` y byte inspection mediante
// `unsigned char`, sin violar strict aliasing con casts de puntero.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct Pair {
    uint16_t low;
    uint16_t high;
};

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static float float_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static void dump_bytes(const void *object, size_t size) {
    const unsigned char *bytes = object;

    printf("object bytes          =");
    for (size_t i = 0; i < size; i++) {
        printf(" %02x", bytes[i]);
    }
    printf("\n");
}

int main(void) {
    float value = 1.0f;
    uint32_t bits = float_bits(value);
    float restored = float_from_bits(bits);

    printf("float 1.0 bits        = 0x%08x\n", bits);
    printf("restored float        = %.1f\n", restored);

    struct Pair pair = {.low = 0x1122u, .high = 0x3344u};
    dump_bytes(&pair, sizeof pair);

    uint32_t combined;
    memcpy(&combined, &pair, sizeof combined);
    printf("pair copied as u32    = 0x%08x\n", combined);

    return 0;
}
