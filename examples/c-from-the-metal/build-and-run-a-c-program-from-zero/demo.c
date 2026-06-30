#include <stdio.h>
#include <stdlib.h>

static long sum_to(long count) {
    long total = 0;

    for (long i = 1; i <= count; i++) {
        total += i;
    }

    return total;
}

int main(int argc, char **argv) {
    long count = 3;

    if (argc > 1) {
        char *end = NULL;
        count = strtol(argv[1], &end, 10);
        if (*end != '\0' || count < 0) {
            fprintf(stderr, "usage: %s [non-negative-count]\n", argv[0]);
            return 2;
        }
    }

#ifdef __STDC_VERSION__
    printf("__STDC_VERSION__      = %ld\n", (long)__STDC_VERSION__);
#else
    printf("__STDC_VERSION__      = pre-C90\n");
#endif
    printf("requested count       = %ld\n", count);
    printf("sum 1..count          = %ld\n", sum_to(count));

    return 0;
}
