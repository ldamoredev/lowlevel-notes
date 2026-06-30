#include <stdio.h>
#include <string.h>

static void inspect_parameter(const char *text) {
    printf("sizeof parameter      = %zu bytes\n", sizeof text);
    printf("strlen parameter      = %zu chars\n", strlen(text));
}

static void print_bytes(const char *label, const char *bytes, size_t count) {
    printf("%s", label);

    for (size_t i = 0; i < count; i++) {
        printf(" %02X", (unsigned char)bytes[i]);
    }

    printf("\n");
}

int main(void) {
    char word[] = "metal";
    const char *literal = "metal";
    char copied[8] = {0};
    char not_a_string[4] = {'b', 'u', 'g', '!'};
    char small[6] = {0};

    memcpy(copied, "low", 4);

    printf("sizeof word array     = %zu bytes\n", sizeof word);
    printf("strlen word           = %zu chars\n", strlen(word));
    printf("sizeof \"metal\"        = %zu bytes\n", sizeof "metal");
    printf("sizeof literal ptr    = %zu bytes\n", sizeof literal);
    printf("strlen literal        = %zu chars\n", strlen(literal));

    inspect_parameter(word);

    /* El array es modificable; el string literal debe tratarse como read-only. */
    word[0] = 'M';
    printf("modified array        = %s\n", word);

    /* strlen camina hasta encontrar NUL; esto no es un C string. */
    print_bytes("not_a_string bytes   =", not_a_string, sizeof not_a_string);

    int needed = snprintf(small, sizeof small, "%s", "systems");
    printf("snprintf needed       = %d chars\n", needed);
    printf("small buffer          = %s\n", small);

    print_bytes("copied bytes         =", copied, sizeof copied);

    return 0;
}
