#include <stdio.h>

/* noinline para que la función exista como tal en el binario y puedas verla
   aislada en el disassembly / en Compiler Explorer. */
__attribute__((noinline))
int add_then_double(int a, int b) {
    int sum = a + b;
    return sum * 2;
}

int main(void) {
    printf("%d\n", add_then_double(7, 5));
    return 0;
}
