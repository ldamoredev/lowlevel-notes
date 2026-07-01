// main.c -- produce un object file con codigo, datos y simbolos externos.
#include <stdio.h>

extern int scale(int value);

int global_counter = 7;
int zero_counter;
const char banner[] = "object";

static int local_bias(void) {
    return 3;
}

int compute(void) {
    zero_counter = scale(global_counter + local_bias());
    return zero_counter;
}

int main(void) {
    printf("%s result: %d\n", banner, compute());
    return 0;
}
