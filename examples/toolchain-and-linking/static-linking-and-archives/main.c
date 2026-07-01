#include <stdio.h>

#include "calc.h"

int main(void) {
    int sum = calc_add(2, 5);
    int product = calc_mul(sum, 3);
    printf("static archive result: %d\n", product);
    return 0;
}
