// demo.c — muestra disciplina de ownership: create/destroy, `realloc` mediante
// temporal y cleanup que devuelve el owner a estado vacio.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>
#include <stdlib.h>

struct OwnedInts {
    int *data;
    size_t count;
};

static int owned_ints_create(struct OwnedInts *owner, size_t count) {
    owner->data = calloc(count, sizeof *owner->data);
    if (owner->data == NULL) {
        owner->count = 0;
        return -1;
    }

    owner->count = count;
    for (size_t i = 0; i < count; i++) {
        owner->data[i] = (int)((i + 1) * 5);
    }

    return 0;
}

static void owned_ints_destroy(struct OwnedInts *owner) {
    free(owner->data);
    owner->data = NULL;
    owner->count = 0;
}

static int owned_ints_resize(struct OwnedInts *owner, size_t new_count) {
    int *new_data = realloc(owner->data, new_count * sizeof *owner->data);
    if (new_data == NULL) {
        return -1;
    }

    for (size_t i = owner->count; i < new_count; i++) {
        new_data[i] = (int)((i + 1) * 5);
    }

    owner->data = new_data;
    owner->count = new_count;
    return 0;
}

static int owned_ints_sum(const struct OwnedInts *owner) {
    int sum = 0;
    for (size_t i = 0; i < owner->count; i++) {
        sum += owner->data[i];
    }
    return sum;
}

int main(void) {
    struct OwnedInts owner = {0};

    if (owned_ints_create(&owner, 3) != 0) {
        perror("create");
        return 1;
    }

    printf("sum initial           = %d\n", owned_ints_sum(&owner));

    if (owned_ints_resize(&owner, 5) != 0) {
        owned_ints_destroy(&owner);
        perror("resize");
        return 1;
    }

    printf("sum after resize      = %d\n", owned_ints_sum(&owner));
    printf("owner before cleanup  = count %zu\n", owner.count);

    owned_ints_destroy(&owner);
    printf("owner after cleanup   = %s count=%zu\n",
           owner.data == NULL ? "NULL" : "live", owner.count);

    return 0;
}
