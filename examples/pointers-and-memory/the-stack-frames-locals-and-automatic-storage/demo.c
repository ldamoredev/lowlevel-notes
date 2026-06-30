// demo.c — muestra frames del stack, automatic storage y un `static` local que
// conserva su storage entre llamadas.
// Compila limpio y corre con:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stdio.h>

static void bump_static_local(void) {
    static int visits = 0;
    visits += 1;
    printf("static local: value=%d address=%p\n", visits, (void *)&visits);
}

static void show_frame(int depth, int *borrowed_from_caller) {
    int local = depth * 10 + 1;
    int scratch[2] = {local, local + 1};

    printf("depth %d: &local=%p  borrowed=%p  borrowed_value=%d\n",
           depth, (void *)&local, (void *)borrowed_from_caller,
           *borrowed_from_caller);

    if (depth < 3) {
        show_frame(depth + 1, &local);
    }

    printf("depth %d returning with local=%d scratch_sum=%d\n",
           depth, local, scratch[0] + scratch[1]);
}

int main(void) {
    int main_local = 100;

    printf("main frame: &main_local=%p\n", (void *)&main_local);
    show_frame(0, &main_local);

    bump_static_local();
    bump_static_local();

    printf("main local after calls=%d\n", main_local);
    return 0;
}
