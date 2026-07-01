#include <stdio.h>

#include "message.h"

int main(void) {
    printf("%s result: %d\n", message_label(), message_value());
    return 0;
}
