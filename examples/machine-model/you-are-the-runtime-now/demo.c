#include <limits.h>
#include <stdio.h>

int main(void) {
    int a[3] = {10, 20, 30};

    printf("a[5]   = %d\n", a[5]);  // (1) out-of-bounds read: undefined behavior
    int x = INT_MAX;
    printf("x + 1  = %d\n", x + 1); // (2) signed overflow: undefined behavior
    int u;
    printf("u      = %d\n", u);     // (3) uninitialized read: indeterminate value
    return 0;
}
