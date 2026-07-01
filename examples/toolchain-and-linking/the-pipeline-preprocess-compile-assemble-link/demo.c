// demo.c -- ejemplo minimo para inspeccionar el pipeline de toolchain.
// build_label() queda definido en el object file; printf queda como simbolo
// externo sin resolver hasta el paso de link.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//   ./run.sh
#include <stdio.h>

int build_number(void) {
    return 4;
}

const char *build_label(void) {
    return "pipeline";
}

int main(void) {
    printf("%s stages: %d\n", build_label(), build_number());
    return 0;
}
