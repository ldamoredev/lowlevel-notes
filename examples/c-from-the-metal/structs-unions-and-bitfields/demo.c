#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct PacketHeader {
    uint8_t tag;
    uint32_t length;
    uint16_t flags;
};

struct CompactHeader {
    uint32_t length;
    uint16_t flags;
    uint8_t tag;
};

union Payload {
    uint32_t as_u32;
    double as_double;
};

enum ValueKind {
    VALUE_U32,
    VALUE_DOUBLE,
};

struct Value {
    enum ValueKind kind;
    union Payload payload;
};

struct StatusBits {
    unsigned ready : 1;
    unsigned error : 1;
    unsigned mode : 3;
    unsigned reserved : 3;
};

static void print_packet_layout(void) {
    printf("PacketHeader size     = %zu bytes\n", sizeof(struct PacketHeader));
    printf("PacketHeader align    = %zu bytes\n", _Alignof(struct PacketHeader));
    printf("  tag offset          = %zu\n", offsetof(struct PacketHeader, tag));
    printf("  length offset       = %zu\n", offsetof(struct PacketHeader, length));
    printf("  flags offset        = %zu\n", offsetof(struct PacketHeader, flags));
    printf("CompactHeader size    = %zu bytes\n", sizeof(struct CompactHeader));
}

static void print_value(struct Value value) {
    if (value.kind == VALUE_U32) {
        printf("tagged union value    = u32:%u\n", value.payload.as_u32);
        return;
    }

    printf("tagged union value    = double:%.2f\n", value.payload.as_double);
}

int main(void) {
    struct PacketHeader header = {
        .tag = 7,
        .length = 1024,
        .flags = 3,
    };
    struct Value id = {
        .kind = VALUE_U32,
        .payload.as_u32 = 42,
    };
    struct Value ratio = {
        .kind = VALUE_DOUBLE,
        .payload.as_double = 0.75,
    };
    struct StatusBits status = {
        .ready = 1,
        .error = 0,
        .mode = 5,
        .reserved = 0,
    };

    /* El orden de los campos cambia el padding que agrega el compilador. */
    print_packet_layout();

    printf("header values         = tag:%u length:%u flags:%u\n",
           (unsigned)header.tag, header.length, (unsigned)header.flags);
    printf("Payload union size    = %zu bytes\n", sizeof(union Payload));
    printf("Value struct size     = %zu bytes\n", sizeof(struct Value));

    /* El tag externo dice que miembro de la union esta activo. */
    print_value(id);
    print_value(ratio);

    /* Los bitfields sirven para flags en memoria, no para formatos portables. */
    printf("StatusBits size       = %zu bytes\n", sizeof(struct StatusBits));
    printf("status bits           = ready:%u error:%u mode:%u\n",
           status.ready, status.error, status.mode);

    return 0;
}
