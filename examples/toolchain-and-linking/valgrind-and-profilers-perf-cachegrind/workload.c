#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t mix(uint64_t value) {
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return value;
}

int main(void) {
    enum { count = 4096 };
    uint64_t *values = malloc(count * sizeof(*values));
    if (values == NULL) {
        return 1;
    }

    for (int i = 0; i < count; i++) {
        values[i] = mix((uint64_t)i + 1U);
    }

    uint64_t checksum = 0;
    for (int pass = 0; pass < 8; pass++) {
        for (int i = 0; i < count; i++) {
            checksum ^= mix(values[i] + (uint64_t)pass);
        }
    }

    printf("profile checksum: 0x%016llx\n", (unsigned long long)checksum);
    free(values);
    return 0;
}
