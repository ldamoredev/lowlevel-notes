#include <stdio.h>
#include <stdlib.h>

struct Node {
    int value;
    struct Node *next;
};

static int make_owned_int(int **out, int value) {
    int *slot = malloc(sizeof *slot);
    if (slot == NULL) {
        *out = NULL;
        return -1;
    }

    *slot = value;
    *out = slot;
    return 0;
}

static int push_front(struct Node **head, int value) {
    struct Node *node = malloc(sizeof *node);
    if (node == NULL) {
        return -1;
    }

    node->value = value;
    node->next = *head;
    *head = node;
    return 0;
}

static int append(struct Node **head, int value) {
    while (*head != NULL) {
        head = &(*head)->next;
    }

    return push_front(head, value);
}

static void print_list(const char *label, const struct Node *head) {
    printf("%s", label);
    for (const struct Node *node = head; node != NULL; node = node->next) {
        printf(" %d", node->value);
    }
    printf("\n");
}

static void free_list(struct Node **head) {
    while (*head != NULL) {
        struct Node *victim = *head;
        *head = victim->next;
        free(victim);
    }
}

int main(void) {
    int value = 7;
    int *p = &value;
    int **pp = &p;

    printf("value through p       = %d\n", *p);
    **pp = 42;
    printf("value after **pp      = %d\n", value);

    int *owned = NULL;
    if (make_owned_int(&owned, 99) != 0) {
        perror("make_owned_int");
        return 1;
    }

    printf("owned from out-param  = %d\n", *owned);
    free(owned);
    owned = NULL;

    struct Node *list = NULL;
    if (append(&list, 10) != 0 ||
        append(&list, 20) != 0 ||
        append(&list, 30) != 0) {
        free_list(&list);
        perror("append");
        return 1;
    }

    print_list("list after append     =", list);
    free_list(&list);
    printf("list after free       = %s\n", list == NULL ? "NULL" : "not NULL");

    return 0;
}
