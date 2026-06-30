// demo.c — muestra function pointers con una tabla de comandos, una funcion
// `apply` que recibe callbacks y comparacion de punteros a funcion.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>

typedef int (*binary_op)(int left, int right);

struct Command {
    const char *name;
    binary_op run;
};

static int add(int left, int right) {
    return left + right;
}

static int multiply(int left, int right) {
    return left * right;
}

static int apply(binary_op op, int left, int right) {
    return op(left, right);
}

int main(void) {
    struct Command commands[] = {
        {"add", add},
        {"mul", multiply},
    };

    for (size_t i = 0; i < sizeof commands / sizeof commands[0]; i++) {
        printf("%s(6, 7)              = %d\n",
               commands[i].name, commands[i].run(6, 7));
    }

    binary_op op = add;
    printf("apply add             = %d\n", apply(op, 2, 3));

    op = multiply;
    printf("apply multiply        = %d\n", apply(op, 2, 3));
    printf("same function pointer = %s\n",
           op == multiply ? "yes" : "no");

    return 0;
}
