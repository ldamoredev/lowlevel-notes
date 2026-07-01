// mathlib.c -- exporta dos simbolos y mantiene helper privado con static.
#include "api.h"

static int hidden_bias(void) {
    return 1;
}

int public_add(int left, int right) {
    return left + right + hidden_bias();
}

int public_twice(int value) {
    return value * 2;
}
