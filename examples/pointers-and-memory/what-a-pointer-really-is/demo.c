#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static void add_ten(int *slot) {
    if (slot != NULL) {
        *slot += 10;
    }
}

static void print_first_int_bytes(const int *value) {
    const unsigned char *bytes = (const unsigned char *)(const void *)value;

    printf("first int bytes       =");
    for (size_t i = 0; i < sizeof *value; i++) {
        printf(" %02x", bytes[i]);
    }
    printf("\n");
}

int main(void) {
    int cells[3] = {11, 22, 33};
    int *p = &cells[0];
    int *alias = p;
    void *generic = p;
    int *from_void = generic;
    int *none = NULL;

    ptrdiff_t element_delta = &cells[1] - &cells[0];
    ptrdiff_t byte_delta = (const unsigned char *)(const void *)&cells[1] -
                           (const unsigned char *)(const void *)&cells[0];

    printf("sizeof p              = %zu bytes\n", sizeof p);
    printf("sizeof *p             = %zu bytes\n", sizeof *p);
    printf("&cells[0]             = %p\n", (void *)&cells[0]);
    printf("&cells[1]             = %p\n", (void *)&cells[1]);
    printf("element delta         = %td element\n", element_delta);
    printf("byte delta            = %td bytes\n", byte_delta);
    printf("*p before             = %d\n", *p);

    *alias = 41;
    add_ten(p);

    printf("cells[0] after alias  = %d\n", cells[0]);
    printf("*(p + 1)              = %d\n", *(p + 1));
    printf("void* round trip      = %s\n",
           from_void == p ? "same pointer" : "different pointer");

#if defined(UINTPTR_MAX)
    uintptr_t bits = (uintptr_t)(void *)p;
    printf("uintptr_t round trip  = %s\n",
           (void *)bits == (void *)p ? "same bits" : "changed bits");
#endif

    print_first_int_bytes(p);
    printf("null comparison       = %s\n", none == NULL ? "NULL" : "not NULL");

    return 0;
}
