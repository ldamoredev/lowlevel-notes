// demo.c — muestra una slice con bounds explicitos y copia de C strings que
// chequea espacio para el terminador antes de llamar a `memcpy`.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>
#include <string.h>

struct Slice {
    int *data;
    size_t count;
};

static int slice_get(struct Slice slice, size_t index, int *out) {
    if (index >= slice.count) {
        return -1;
    }

    *out = slice.data[index];
    return 0;
}

static int copy_c_string(char *dst, size_t dst_size, const char *src) {
    size_t needed = strlen(src) + 1;
    if (needed > dst_size) {
        return -1;
    }

    memcpy(dst, src, needed);
    return 0;
}

int main(void) {
    int values[] = {10, 20, 30};
    struct Slice slice = {values, sizeof values / sizeof values[0]};

    int value = 0;
    if (slice_get(slice, 2, &value) == 0) {
        printf("slice[2]              = %d\n", value);
    }

    printf("slice[3] status       = %s\n",
           slice_get(slice, 3, &value) == 0 ? "ok" : "out of bounds");

    char name[8];
    printf("copy short            = %s\n",
           copy_c_string(name, sizeof name, "atlas") == 0 ? name : "too long");
    printf("copy long status      = %s\n",
           copy_c_string(name, sizeof name, "low-level") == 0 ? "ok" : "too long");

    return 0;
}
