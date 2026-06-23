// demo.c — un programa mínimo para recorrer el pipeline de build con run.sh.
// answer() queda como símbolo propio (T) y printf como símbolo sin resolver (U)
// en el object file, hasta que el linker lo completa.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//   ./run.sh   # muestra las cuatro etapas: preprocess -> compile -> assemble -> link
#include <stdio.h>

int answer(void) { return 42; }

int main(void) {
    printf("the answer is %d\n", answer());
    return 0;
}
