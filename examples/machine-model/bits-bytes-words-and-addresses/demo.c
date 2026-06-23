// demo.c — tamaños de tipos, byte addressing y los bytes adentro de una word.
// Compilá en -O0 para que el array local no se optimice:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//
// sizeof devuelve BYTES; multiplicá por CHAR_BIT para bits. El loop de direcciones
// muestra que cada int está a sizeof(int) bytes del siguiente, y el cast a char*
// deja ver los 4 bytes de un uint32_t (en x86-64, little-endian: 44 33 22 11).
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

int main(void) {
    printf("CHAR_BIT (bits per byte) = %d\n\n", CHAR_BIT);

    printf("type    bytes  bits\n");
    printf("char    %5zu  %4zu\n", sizeof(char),  sizeof(char)  * CHAR_BIT);
    printf("int     %5zu  %4zu\n", sizeof(int),   sizeof(int)   * CHAR_BIT);
    printf("long    %5zu  %4zu\n", sizeof(long),  sizeof(long)  * CHAR_BIT);
    printf("void*   %5zu  %4zu\n", sizeof(void*), sizeof(void*) * CHAR_BIT);

    int a[4];                                   // byte addressing
    printf("\neach int is %zu bytes apart:\n", sizeof(int));
    for (int i = 0; i < 4; i++)
        printf("  &a[%d] = %p\n", i, (void*)&a[i]);

    uint32_t w = 0x11223344u;                   // una word es solo bytes
    unsigned char *p = (unsigned char*)&w;
    printf("\nthe 4 bytes of 0x11223344 in memory order:\n  ");
    for (size_t i = 0; i < sizeof w; i++) printf("%02x ", p[i]);
    printf("\n");
    return 0;
}
