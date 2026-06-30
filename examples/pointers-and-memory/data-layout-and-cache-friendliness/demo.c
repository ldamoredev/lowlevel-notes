// demo.c — compara AoS y SoA para el mismo calculo, mostrando el stride real que
// ve un loop cuando solo consume un campo hot.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>

struct ParticleAoS {
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float vz;
    int alive;
};

struct ParticlesSoA {
    float *x;
    float *y;
    float *z;
    float *vx;
    float *vy;
    float *vz;
    int *alive;
};

static float sum_alive_x_aos(const struct ParticleAoS *particles, size_t count) {
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (particles[i].alive) {
            sum += particles[i].x;
        }
    }
    return sum;
}

static float sum_alive_x_soa(struct ParticlesSoA particles, size_t count) {
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (particles.alive[i]) {
            sum += particles.x[i];
        }
    }
    return sum;
}

int main(void) {
    struct ParticleAoS aos[] = {
        {.x = 1.0f, .y = 0.0f, .z = 0.0f, .vx = 0.1f, .vy = 0.0f, .vz = 0.0f, .alive = 1},
        {.x = 2.0f, .y = 0.0f, .z = 0.0f, .vx = 0.1f, .vy = 0.0f, .vz = 0.0f, .alive = 0},
        {.x = 3.0f, .y = 0.0f, .z = 0.0f, .vx = 0.1f, .vy = 0.0f, .vz = 0.0f, .alive = 1},
        {.x = 4.0f, .y = 0.0f, .z = 0.0f, .vx = 0.1f, .vy = 0.0f, .vz = 0.0f, .alive = 1},
    };

    float x[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float y[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float z[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float vx[] = {0.1f, 0.1f, 0.1f, 0.1f};
    float vy[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float vz[] = {0.0f, 0.0f, 0.0f, 0.0f};
    int alive[] = {1, 0, 1, 1};

    struct ParticlesSoA soa = {
        .x = x, .y = y, .z = z,
        .vx = vx, .vy = vy, .vz = vz,
        .alive = alive,
    };

    size_t count = sizeof aos / sizeof aos[0];

    printf("AoS particle size     = %zu bytes\n", sizeof aos[0]);
    printf("AoS x stride          = %td bytes\n",
           (const char *)(const void *)&aos[1].x -
           (const char *)(const void *)&aos[0].x);
    printf("SoA x stride          = %td bytes\n",
           (const char *)(const void *)&x[1] -
           (const char *)(const void *)&x[0]);
    printf("sum alive x AoS       = %.1f\n", sum_alive_x_aos(aos, count));
    printf("sum alive x SoA       = %.1f\n", sum_alive_x_soa(soa, count));

    return 0;
}
