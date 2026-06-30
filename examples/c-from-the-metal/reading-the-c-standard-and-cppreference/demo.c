#include <limits.h>
#include <stddef.h>
#include <stdio.h>

int main(void) {
#ifdef __STDC_VERSION__
    printf("__STDC_VERSION__      = %ld\n", (long)__STDC_VERSION__);
#else
    printf("__STDC_VERSION__      = pre-C90\n");
#endif

    printf("CHAR_BIT              = %d\n", CHAR_BIT);
    printf("INT_MAX               = %d\n", INT_MAX);
    printf("sizeof(size_t)        = %zu bytes\n", sizeof(size_t));

    /* El estandar deja la signedness de char plano a la implementacion. */
    if ((char)0xFF < 0) {
        printf("plain char signed     = yes\n");
    } else {
        printf("plain char signed     = no\n");
    }

    return 0;
}
