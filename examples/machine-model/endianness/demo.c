// demo.c — detectá el orden de bytes del host, dumpeá los bytes de un valor y
// convertí a network order (big-endian) con htonl/ntohl.
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//
// En x86-64 (little-endian) el valor 0x0A0B0C0D queda en memoria como 0d 0c 0b 0a;
// htonl lo reordena al big-endian 0a 0b 0c 0d para el cable.
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>   // htonl, ntohl

static void dump(const char *label, const void *p, size_t n) {
    const unsigned char *b = p;
    printf("%-24s", label);
    for (size_t i = 0; i < n; i++) printf("%02x ", b[i]);
    putchar('\n');
}

int main(void) {
    uint32_t x = 0x0A0B0C0D;

    unsigned char first = *(unsigned char*)&x;          // primer byte en memoria
    printf("value: 0x%08X\n", x);
    printf("this machine is: %s\n\n",
           first == 0x0D ? "little-endian (LSB first)" : "big-endian (MSB first)");

    dump("bytes in memory:", &x, sizeof x);

    uint32_t net = htonl(x);                            // host -> network (big-endian)
    dump("after htonl (network):", &net, sizeof net);
    printf("ntohl(htonl(x)) == x ? %s\n", ntohl(net) == x ? "yes" : "no");

    uint32_t s = ((x & 0xFF000000u) >> 24) | ((x & 0x00FF0000u) >> 8) |   // bswap a mano
                 ((x & 0x0000FF00u) <<  8) | ((x & 0x000000FFu) << 24);
    printf("manual bswap: 0x%08X -> 0x%08X\n", x, s);
    return 0;
}
