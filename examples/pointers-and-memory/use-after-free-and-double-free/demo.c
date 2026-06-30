// demo.c — muestra ownership de heap con un destructor que llama a `free`, limpia
// el puntero owner y vuelve segura una segunda destruccion.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>
#include <stdlib.h>

struct Buffer {
    int *data;
    size_t count;
};

static int buffer_init(struct Buffer *buffer, size_t count) {
    buffer->data = malloc(count * sizeof *buffer->data);
    if (buffer->data == NULL) {
        buffer->count = 0;
        return -1;
    }

    buffer->count = count;
    for (size_t i = 0; i < count; i++) {
        buffer->data[i] = (int)(i + 1);
    }

    return 0;
}

static void buffer_destroy(struct Buffer *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->count = 0;
}

static int buffer_sum(const struct Buffer *buffer) {
    int sum = 0;
    for (size_t i = 0; i < buffer->count; i++) {
        sum += buffer->data[i];
    }
    return sum;
}

int main(void) {
    struct Buffer buffer = {0};

    if (buffer_init(&buffer, 4) != 0) {
        perror("buffer_init");
        return 1;
    }

    printf("sum while alive       = %d\n", buffer_sum(&buffer));
    printf("owner before destroy  = %s\n",
           buffer.data != NULL ? "live" : "NULL");

    buffer_destroy(&buffer);
    printf("owner after destroy   = %s count=%zu\n",
           buffer.data == NULL ? "NULL" : "live", buffer.count);

    buffer_destroy(&buffer);
    printf("second destroy safe   = %s count=%zu\n",
           buffer.data == NULL ? "NULL" : "live", buffer.count);

    return 0;
}
