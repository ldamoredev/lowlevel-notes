#include <stdio.h>
#include <stdlib.h>

struct Node {
    int value;
    struct Node *next;
};

static void print_ints(const char *label, const int *items, size_t count) {
    printf("%s", label);
    for (size_t i = 0; i < count; i++) {
        printf(" %d", items[i]);
    }
    printf("\n");
}

static void free_chain(struct Node *head) {
    while (head != NULL) {
        struct Node *next = head->next;
        free(head);
        head = next;
    }
}

int main(void) {
    size_t count = 4;
    int *numbers = malloc(count * sizeof *numbers);

    if (numbers == NULL) {
        perror("malloc numbers");
        return 1;
    }

    for (size_t i = 0; i < count; i++) {
        numbers[i] = (int)((i + 1) * 10);
    }

    printf("malloc numbers        = %p\n", (void *)numbers);
    print_ints("numbers initial       =", numbers, count);

    size_t bigger_count = 6;
    int *grown = realloc(numbers, bigger_count * sizeof *numbers);
    if (grown == NULL) {
        free(numbers);
        perror("realloc numbers");
        return 1;
    }

    numbers = grown;
    numbers[4] = 50;
    numbers[5] = 60;
    count = bigger_count;

    printf("realloc numbers       = %p\n", (void *)numbers);
    print_ints("numbers grown         =", numbers, count);

    size_t zero_count = 3;
    int *zeros = calloc(zero_count, sizeof *zeros);
    if (zeros == NULL) {
        free(numbers);
        perror("calloc zeros");
        return 1;
    }

    print_ints("calloc zeros          =", zeros, zero_count);

    struct Node *first = malloc(sizeof *first);
    struct Node *second = malloc(sizeof *second);
    if (first == NULL || second == NULL) {
        free(first);
        free(second);
        free(zeros);
        free(numbers);
        perror("malloc node");
        return 1;
    }

    first->value = 1;
    first->next = second;
    second->value = 2;
    second->next = NULL;

    printf("node chain            = %d -> %d\n",
           first->value, first->next->value);

    free_chain(first);
    free(zeros);
    free(numbers);

    first = NULL;
    zeros = NULL;
    numbers = NULL;

    printf("after free owners     = %s, %s, %s\n",
           first == NULL ? "NULL" : "live",
           zeros == NULL ? "NULL" : "live",
           numbers == NULL ? "NULL" : "live");

    return 0;
}
