// demo.c — cómo compilan un loop, un struct y un switch.
// Usá run.sh para emitir assembly x86-64 Intel y ARM64, y para correr el programa.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//   ./run.sh
#include <stddef.h>
#include <stdio.h>

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

struct Packet {
    int tag;
    long value;
    int scale;
};

NOINLINE long classify_and_sum(const struct Packet *items, size_t count) {
    long total = 0;

    for (size_t i = 0; i < count; i++) {
        long scaled = items[i].value * items[i].scale;

        switch (items[i].tag) {
        case 0:
            total += scaled;
            break;
        case 1:
            total -= scaled;
            break;
        case 2:
            total += items[i].value;
            break;
        default:
            total += 7;
            break;
        }
    }

    return total;
}

int main(void) {
    struct Packet packets[] = {
        {0, 10, 2},
        {1, 3, 5},
        {2, 8, 4},
        {9, 99, 1},
    };

    size_t count = sizeof packets / sizeof packets[0];
    printf("classified sum = %ld\n", classify_and_sum(packets, count));
    return 0;
}
