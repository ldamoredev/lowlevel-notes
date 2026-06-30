// demo.c — implementa un tiny arena/bump allocator y un pool de slots fijos para
// mostrar alineacion, reset masivo, exhaustion y reutilizacion de slots.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct Arena {
    unsigned char *base;
    size_t capacity;
    size_t used;
};

struct PoolNode {
    struct PoolNode *next;
};

struct Pool {
    struct PoolNode *free_list;
    unsigned char *storage;
    size_t slot_size;
    size_t slot_count;
};

union PoolSlot {
    struct PoolNode node;
    int value;
};

static size_t align_up(size_t value, size_t alignment) {
    size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

static void *arena_alloc(struct Arena *arena, size_t size, size_t alignment) {
    size_t start = align_up(arena->used, alignment);
    size_t end = start + size;

    if (end > arena->capacity) {
        return NULL;
    }

    arena->used = end;
    return arena->base + start;
}

static void arena_reset(struct Arena *arena) {
    arena->used = 0;
}

static void pool_init(struct Pool *pool,
                      unsigned char *storage,
                      size_t slot_size,
                      size_t slot_count) {
    pool->storage = storage;
    pool->slot_size = slot_size;
    pool->slot_count = slot_count;
    pool->free_list = NULL;

    for (size_t i = 0; i < slot_count; i++) {
        struct PoolNode *node = (struct PoolNode *)(void *)(storage + i * slot_size);
        node->next = pool->free_list;
        pool->free_list = node;
    }
}

static void *pool_alloc(struct Pool *pool) {
    if (pool->free_list == NULL) {
        return NULL;
    }

    struct PoolNode *node = pool->free_list;
    pool->free_list = node->next;
    return node;
}

static void pool_free(struct Pool *pool, void *slot) {
    struct PoolNode *node = slot;
    node->next = pool->free_list;
    pool->free_list = node;
}

int main(void) {
    unsigned char arena_storage[64];
    struct Arena arena = {arena_storage, sizeof arena_storage, 0};

    int *ids = arena_alloc(&arena, 3 * sizeof *ids, _Alignof(int));
    double *score = arena_alloc(&arena, sizeof *score, _Alignof(double));
    if (ids == NULL || score == NULL) {
        printf("arena allocation failed\n");
        return 1;
    }

    ids[0] = 10;
    ids[1] = 20;
    ids[2] = 30;
    *score = 9.5;

    printf("arena used            = %zu bytes\n", arena.used);
    printf("arena values          = %d %d %d %.1f\n",
           ids[0], ids[1], ids[2], *score);

    arena_reset(&arena);
    printf("arena after reset     = %zu bytes\n", arena.used);

    union PoolSlot pool_storage[3];
    struct Pool pool;
    pool_init(&pool, (unsigned char *)(void *)pool_storage, sizeof pool_storage[0], 3);

    int *a = pool_alloc(&pool);
    int *b = pool_alloc(&pool);
    int *c = pool_alloc(&pool);
    int *d = pool_alloc(&pool);

    if (a == NULL || b == NULL || c == NULL) {
        printf("pool allocation failed\n");
        return 1;
    }

    *a = 1;
    *b = 2;
    *c = 3;

    printf("pool values           = %d %d %d\n", *a, *b, *c);
    printf("pool exhausted        = %s\n", d == NULL ? "yes" : "no");

    pool_free(&pool, b);
    int *again = pool_alloc(&pool);
    if (again == NULL) {
        printf("pool reuse failed\n");
        return 1;
    }

    *again = 22;
    printf("pool reused slot      = %d\n", *again);

    return 0;
}
