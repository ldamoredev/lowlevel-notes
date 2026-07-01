#include <limits.h>
#include <stdio.h>

int main(void) {
    int value = INT_MAX;
    int overflowed = value + 1;
    printf("ubsan result: %d\n", overflowed);
    return 0;
}
