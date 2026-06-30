#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_ints(const void *left, const void *right) {
    int a = *(const int *)left;
    int b = *(const int *)right;

    return (a > b) - (a < b);
}

static void print_ints(const int *values, size_t count) {
    for (size_t i = 0; i < count; i++) {
        printf("%s%d", i == 0 ? "" : " ", values[i]);
    }

    printf("\n");
}

int main(void) {
    char *end = NULL;
    errno = 0;
    long parsed = strtol("42kb", &end, 10);

    printf("strtol value          = %ld\n", parsed);
    printf("strtol rest           = %s\n", end);

    errno = 0;
    (void)strtol("999999999999999999999999999999", NULL, 10);
    printf("strtol ERANGE         = %s\n", errno == ERANGE ? "yes" : "no");

    int *values = malloc(4 * sizeof *values);
    if (values == NULL) {
        perror("malloc");
        return 1;
    }

    values[0] = 4;
    values[1] = 1;
    values[2] = 3;
    values[3] = 2;

    qsort(values, 4, sizeof values[0], compare_ints);
    printf("sorted ints           = ");
    print_ints(values, 4);

    /* memmove acepta rangos solapados; memcpy no promete eso. */
    char text[8] = "abcde";
    memmove(text + 2, text, 3);
    printf("memmove text          = %s\n", text);

    free(values);
    return 0;
}
