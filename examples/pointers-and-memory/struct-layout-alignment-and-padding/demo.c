// demo.c — muestra `sizeof`, `_Alignof`, `offsetof`, padding por orden de campos
// y stride de arrays de structs.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>

struct BadOrder {
    char tag;
    double score;
    int count;
};

struct BetterOrder {
    double score;
    int count;
    char tag;
};

struct Header {
    unsigned char kind;
    unsigned char flags;
    unsigned short length;
};

int main(void) {
    printf("BadOrder sizeof       = %zu align=%zu\n",
           sizeof(struct BadOrder), _Alignof(struct BadOrder));
    printf("  tag offset          = %zu\n", offsetof(struct BadOrder, tag));
    printf("  score offset        = %zu\n", offsetof(struct BadOrder, score));
    printf("  count offset        = %zu\n", offsetof(struct BadOrder, count));

    printf("BetterOrder sizeof    = %zu align=%zu\n",
           sizeof(struct BetterOrder), _Alignof(struct BetterOrder));
    printf("  score offset        = %zu\n", offsetof(struct BetterOrder, score));
    printf("  count offset        = %zu\n", offsetof(struct BetterOrder, count));
    printf("  tag offset          = %zu\n", offsetof(struct BetterOrder, tag));

    struct BetterOrder items[2] = {
        {.score = 1.5, .count = 2, .tag = 'a'},
        {.score = 2.5, .count = 3, .tag = 'b'},
    };

    ptrdiff_t stride = (const char *)(const void *)&items[1] -
                       (const char *)(const void *)&items[0];
    printf("array stride          = %td\n", stride);

    printf("Header sizeof         = %zu align=%zu\n",
           sizeof(struct Header), _Alignof(struct Header));

    return 0;
}
