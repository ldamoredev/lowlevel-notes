// main.c -- usa declaraciones; las definiciones viven en mathlib.o.
#include <stdio.h>

#include "api.h"

int main(void) {
    int sum = public_add(20, 1);
    printf("sum=%d twice=%d\n", sum, public_twice(sum));
    return 0;
}
