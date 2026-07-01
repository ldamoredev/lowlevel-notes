#include <pthread.h>
#include <stdio.h>

static int shared_counter;

static void *worker(void *unused) {
    (void)unused;
    for (int i = 0; i < 1000; i++) {
        shared_counter++;
    }
    return NULL;
}

int main(void) {
    pthread_t left;
    pthread_t right;

    if (pthread_create(&left, NULL, worker, NULL) != 0) {
        return 1;
    }
    if (pthread_create(&right, NULL, worker, NULL) != 0) {
        return 1;
    }

    pthread_join(left, NULL);
    pthread_join(right, NULL);
    printf("tsan counter: %d\n", shared_counter);
    return 0;
}
