#include "counter.h"

int counter_value = 0;

static int step = 1;

static int add_step(int value) {
    return value + step;
}

void counter_reset(int value) {
    counter_value = value;
}

int counter_next(void) {
    counter_value = add_step(counter_value);
    return counter_value;
}

int counter_read(void) {
    return counter_value;
}
