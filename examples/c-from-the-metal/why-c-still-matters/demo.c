// demo.c — C como capa fina: un struct es memoria con offsets, bytes y alineacion.
// Compila limpio (sin warnings) con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
//
// Leer un objeto como unsigned char* esta definido por C: es la forma portable de
// inspeccionar su representacion en bytes.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint8_t flags;
    uint8_t checksum;
};

static unsigned checksum_before_checksum(const struct PacketHeader *header) {
    const unsigned char *bytes = (const unsigned char *)header;
    unsigned sum = 0;

    for (size_t i = 0; i < offsetof(struct PacketHeader, checksum); i++) {
        sum += bytes[i];
    }

    return sum & 0xFFu;
}

static void dump_bytes(const void *object, size_t size) {
    const unsigned char *bytes = (const unsigned char *)object;

    for (size_t i = 0; i < size; i++) {
        printf("%02X%s", (unsigned)bytes[i], i + 1 == size ? "\n" : " ");
    }
}

int main(void) {
    struct PacketHeader header = {
        .magic = 0x4C4C4154u,  // En little-endian se ve como bytes: 54 41 4C 4C.
        .version = 0x0102u,
        .flags = 0xA5u,
        .checksum = 0u,
    };

    header.checksum = (uint8_t)checksum_before_checksum(&header);

    printf("sizeof(PacketHeader) = %zu\n", sizeof header);
    printf("offsets: magic=%zu version=%zu flags=%zu checksum=%zu\n",
           offsetof(struct PacketHeader, magic),
           offsetof(struct PacketHeader, version),
           offsetof(struct PacketHeader, flags),
           offsetof(struct PacketHeader, checksum));
    printf("fields: magic=0x%08X version=0x%04X flags=0x%02X checksum=0x%02X\n",
           (unsigned)header.magic,
           (unsigned)header.version,
           (unsigned)header.flags,
           (unsigned)header.checksum);

    printf("bytes : ");
    dump_bytes(&header, sizeof header);

    unsigned char *raw = (unsigned char *)&header;
    raw[0] = 0xEF;
    raw[1] = 0xBE;
    raw[2] = 0xAD;
    raw[3] = 0xDE;

    printf("after raw byte writes, magic = 0x%08X\n", (unsigned)header.magic);
    return 0;
}
