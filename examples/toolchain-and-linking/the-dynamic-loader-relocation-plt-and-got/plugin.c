#include "plugin.h"

static int plugin_bias(void) {
    return 37;
}

int plugin_scale(int value) {
    return value + plugin_bias();
}
