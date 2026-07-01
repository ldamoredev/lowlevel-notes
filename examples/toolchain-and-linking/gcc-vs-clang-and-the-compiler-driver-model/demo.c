#include <stdio.h>

static int twice(int value) {
    return value * 2;
}

int main(void) {
    printf("driver model: %d\n", twice(4));
    return 0;
}
