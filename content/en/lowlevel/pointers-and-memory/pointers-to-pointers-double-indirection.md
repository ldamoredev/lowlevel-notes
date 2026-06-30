---
title: "Pointers to pointers (double indirection)"
description: A pointer-to-pointer is a pointer to a pointer object. It lets a function replace the caller's pointer, model arrays of pointers, and edit links in pointer-based data structures.
tags: [pointers, double-indirection, out-parameters, linked-lists, ownership]
order: 5
updated: 2026-06-30
---
# Pointers to pointers (double indirection)

A pointer-to-pointer is exactly what the type says: a pointer whose target object is
itself a pointer. `T *` lets you reach a `T`; `T **` lets you reach a `T *` so you can
read or replace the pointer value stored there. That extra level is what lets a function
change the caller's pointer, return allocated objects through out-parameters, and edit
linked data structures without special cases. The cost is cognitive: every `*` changes
which object you are talking about.

> The reset: `int **pp` does not point at an `int`. It points at an `int *`. `*pp` is the
> pointer object, and `**pp` is the `int` reached after following both levels.

## The extra star changes what you can mutate

Start with one object and one pointer:

```c
int value = 7;
int *p = &value;
int **pp = &p;
```

The levels are:

| Expression | Type | Object reached |
|---|---|---|
| `value` | `int` | the integer object |
| `p` | `int *` | a pointer object that stores `&value` |
| `*p` | `int` | the integer reached through `p` |
| `pp` | `int **` | a pointer object that stores `&p` |
| `*pp` | `int *` | the caller's pointer object `p` |
| `**pp` | `int` | the integer reached through `p` |

Passing `p` to a function lets the function modify `*p`, the pointed-to `int`. Passing
`&p` lets the function modify `p` itself. That is the whole reason `T **` shows up in C
APIs: the callee needs to replace a pointer variable owned by the caller.

## How it really works

C passes arguments by value. If you call:

```c
void set_to_null(int *p) {
    p = NULL;
}
```

the assignment changes only the function's local copy of the pointer. The caller's
pointer is untouched. To modify the caller's pointer object, pass its address:

```c
void set_to_null(int **slot) {
    *slot = NULL;
}
```

`slot` is a pointer to the caller's pointer variable. `*slot = NULL` writes into that
variable. The same pattern is used for allocation out-parameters:

```c
bool make_widget(struct Widget **out);
```

The API says, "give me the address of your `struct Widget *`; if I succeed, I will store a
new owning pointer there." This is common when the return value is needed for status or
error reporting.

Double indirection is also a clean way to edit linked structures. In a singly linked list,
each "link" is a `struct Node *`: either the head pointer or a `next` field. A cursor of
type `struct Node **` can point at whichever link currently leads to the place you want to
modify. Inserting at the head and inserting after a node become the same operation:
replace `*link`.

Arrays of pointers use the same syntax but a different idea. `char **argv` in `main`
points at the first element of an array of `char *` values. It is not a two-dimensional
character array; it is an array of pointers, and each pointer may point at a string stored
elsewhere. This distinction mirrors the earlier array lesson: `T **` is not the same
layout as `T rows[][cols]`.

Const gets noisy at this level because there are multiple things that can be const:

```c
const char **a;        // pointer to pointer to const char: can change *a
char * const *b;       // pointer to const pointer to char: cannot change *b
const char * const *c; // pointer to const pointer to const char
```

Read from the inside out: what is the pointed-to object at each level, and which level is
protected from writes?

## Executable artifact: mutate the caller's pointer

The demo lives in `examples/pointers-and-memory/pointers-to-pointers-double-indirection/demo.c`.

```c
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
```

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Real output:

```
value through p       = 7
value after **pp      = 42
owned from out-param  = 99
list after append     = 10 20 30
list after free       = NULL
```

`**pp = 42` changes the original `value` through two levels. `make_owned_int(&owned, 99)`
stores a heap pointer into the caller's `owned` variable. The `append` function walks a
`struct Node **` cursor until it points at the link that should be replaced, so appending
to an empty list and appending after the tail use the same write: `*head = node`.

## Failure modes & trade-offs

- **Losing track of the level.** `p`, `*p`, and `**p` name different objects. A wrong
  dereference can overwrite the pointer variable when you meant to overwrite the object,
  or the reverse.
- **Out-parameter ownership ambiguity.** `T **out` must say whether the callee allocates,
  borrows, transfers ownership, or merely points into existing storage. The type alone
  does not say.
- **Partial initialization.** If a function takes multiple out-parameters, define what
  happens on failure. Leaving some outputs written and others indeterminate is a cleanup
  trap.
- **`T **` vs multidimensional arrays.** `int **` describes a pointer to pointer. It does
  not describe contiguous `int rows[3][4]` storage, whose decayed parameter type is
  `int (*)[4]`.
- **Const holes.** Converting between `T **` and `const T **` is not safe in the way many
  people expect; it can let code smuggle a non-const pointer to const storage.
- **Readable code cost.** Double indirection can remove special cases in data structures,
  but beyond two levels the code usually needs a small helper type, clearer naming, or a
  different API.

## In practice

- **Use names that expose the level.** `out`, `slot`, `link`, `headp`, and `cursor` are
  clearer than another generic `p`.
- **Document out-parameter states.** Say what the callee writes on success, what it writes
  on failure, and who owns the resulting pointer.
- **Initialize outputs before risky work.** Setting `*out = NULL` early makes cleanup paths
  simpler and safer.
- **Use `Node **link` for singly linked list edits.** It is one of the rare places where
  double indirection makes code smaller and more correct.
- **Keep pointer arrays and real 2D arrays separate.** `char **argv` style layouts are
  arrays of pointers; matrix-like contiguous storage should use pointer-to-array types.
- **Stop at two levels unless there is a strong reason.** `T ***` is sometimes real, but
  it should trigger an API design review.

**Connects to:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Pointer arithmetic & stride]] · [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|The heap: malloc/free & the allocator underneath]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — pointer declarators, indirection, assignment, function-call argument passing, and qualification rules. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Pointer declaration** — compact reference for pointer-to-pointer syntax, null pointers, function parameters, and multi-level indirection. https://en.cppreference.com/w/c/language/pointer
- **cppreference — Type qualifiers** — how `const` applies at different pointer levels and why qualifier conversions get tricky with `T **`. https://en.cppreference.com/w/c/language/const
- **Jens Gustedt — *Modern C*** — modern treatment of pointer parameters, out-parameters, ownership discipline, and linked data structures. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Richard Reese — *Understanding and Using C Pointers*** — practical pointer-to-pointer examples: out-parameters, arrays of pointers, and linked list manipulation. https://www.oreilly.com/library/view/understanding-and-using/9781449344535/
- **Beej's Guide to C — Pointers** — approachable explanation of pointer levels, `argv`, and passing pointers to functions. https://beej.us/guide/bgc/html/split/pointers.html
