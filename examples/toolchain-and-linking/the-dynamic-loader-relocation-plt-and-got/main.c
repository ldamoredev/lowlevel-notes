#include <stdio.h>

#include "plugin.h"

int main(void) {
    printf("loader result=%d\n", plugin_scale(5));
    return 0;
}
