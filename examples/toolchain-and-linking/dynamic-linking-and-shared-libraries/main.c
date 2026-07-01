#include <stdio.h>

#include "greeting.h"

int main(void) {
    printf("%s value=%d\n", greeting_label(), greeting_value());
    return 0;
}
