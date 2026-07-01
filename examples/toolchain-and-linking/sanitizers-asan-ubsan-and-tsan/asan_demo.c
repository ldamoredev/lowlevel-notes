#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *values = malloc(3 * sizeof(int));
    if (values == NULL) {
        return 1;
    }

    values[0] = 10;
    values[1] = 20;
    values[2] = 30;
    printf("asan before bug: %d\n", values[1]);
    fflush(stdout);

    values[3] = 40;

    free(values);
    return 0;
}
