#include <stdio.h>

#include "counter.h"

int main(void) {
    counter_reset(40);

    printf("counter_read() = %d\n", counter_read());
    printf("counter_next() = %d\n", counter_next());
    printf("counter_value  = %d\n", counter_value);

    return 0;
}
