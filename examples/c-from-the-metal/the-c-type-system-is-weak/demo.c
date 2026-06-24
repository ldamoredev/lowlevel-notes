// demo.c — el sistema de tipos de C chequea la forma, no la intencion.
// Compila limpio (sin warnings) con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//
// Nada aca es undefined behavior: el punto es mostrar cosas peligrosas que el
// compilador acepta porque son conversiones o aliases legales en C.
#include <stdint.h>
#include <stdio.h>

typedef uint32_t UserId;
typedef uint32_t FileDescriptor;

static void load_user(UserId id) {
    printf("load_user(%u)\n", (unsigned)id);
}

static const char *flag_state(int enabled) {
    return enabled ? "enabled" : "disabled";
}

int main(void) {
    UserId user = 42;
    FileDescriptor fd = 42;

    load_user(user);
    load_user(fd);  // C acepta esto: UserId y FileDescriptor son el mismo tipo real.

    printf("flag_state(1)    = %s\n", flag_state(1));
    printf("flag_state(1234) = %s\n", flag_state(1234));
    printf("flag_state(-7)   = %s\n", flag_state(-7));

    unsigned int big = 300;
    uint8_t small = (uint8_t)big;  // Cast explicito: "confia en mi", aunque trunque.
    printf("(uint8_t)300     = %u\n", (unsigned)small);

    return 0;
}
