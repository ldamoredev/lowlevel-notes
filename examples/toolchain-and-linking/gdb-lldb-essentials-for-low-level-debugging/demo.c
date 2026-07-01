#include <stdio.h>

static int accumulate(int value) {
    int doubled = value * 2;
    int biased = doubled + 5;
    return biased;
}

int main(void) {
    int result = accumulate(7);
    printf("debug result: %d\n", result);
    return 0;
}
