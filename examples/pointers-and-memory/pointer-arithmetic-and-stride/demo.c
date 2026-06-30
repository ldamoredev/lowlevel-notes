// demo.c — muestra que la aritmetica de punteros avanza por el stride del tipo:
// `int`, `double`, structs, puntero one-past y bytes crudos.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct Packet {
    uint16_t length;
    uint8_t tag;
    uint8_t payload[5];
};

static ptrdiff_t byte_distance(const void *a, const void *b) {
    const unsigned char *left = a;
    const unsigned char *right = b;
    return right - left;
}

static void print_stride(const char *label,
                         size_t element_size,
                         const void *first,
                         const void *second) {
    printf("%-10s sizeof=%2zu  byte stride=%2td\n",
           label, element_size, byte_distance(first, second));
}

int main(void) {
    int numbers[4] = {10, 20, 30, 40};
    double weights[3] = {1.5, 2.5, 3.5};
    struct Packet packets[2] = {
        {.length = 5, .tag = 7, .payload = {1, 2, 3, 4, 5}},
        {.length = 3, .tag = 9, .payload = {6, 7, 8, 0, 0}},
    };

    print_stride("int", sizeof numbers[0], &numbers[0], &numbers[1]);
    print_stride("double", sizeof weights[0], &weights[0], &weights[1]);
    print_stride("Packet", sizeof packets[0], &packets[0], &packets[1]);

    printf("numbers[2] via *(numbers + 2) = %d\n", *(numbers + 2));
    printf("packets[1].tag via pointer    = %u\n",
           (unsigned int)(packets + 1)->tag);

    int *begin = numbers;
    int *end = numbers + (sizeof numbers / sizeof numbers[0]);
    int sum = 0;

    for (int *it = begin; it != end; it++) {
        sum += *it;
    }

    printf("one-past delta               = %td elements\n", end - begin);
    printf("sum walked by pointer        = %d\n", sum);

    const unsigned char *bytes = (const unsigned char *)(const void *)numbers;
    printf("first int raw bytes          =");
    for (size_t i = 0; i < sizeof numbers[0]; i++) {
        printf(" %02x", bytes[i]);
    }
    printf("\n");

    return 0;
}
