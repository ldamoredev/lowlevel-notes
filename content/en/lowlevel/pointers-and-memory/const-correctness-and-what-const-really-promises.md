---
title: "Const-correctness and what `const` really promises"
description: `const` on a pointer type says which access path may not write. It is a compile-time promise about mutation through that expression, not ownership, lifetime, immutability, or thread-safety.
tags: [const, pointers, qualifiers, api-design, c]
order: 7
updated: 2026-06-30
---
# Const-correctness and what `const` really promises

`const` is a promise about writes through a particular access path. `const int *p` means
"do not modify the `int` through `p`"; it does not mean the object is globally immutable,
owned by the callee, alive forever, or thread-safe. `int * const p` means the pointer
object itself cannot be reseated. Const-correctness is the discipline of putting those
promises on API boundaries so read-only code cannot accidentally become write-capable.

> The reset: `const` attaches to a level of indirection. Ask "which object is protected:
> the pointed-to thing, the pointer variable, or both?"

## Read the declaration by levels

The common cases:

| Declaration | Can reseat pointer? | Can write through it? |
|---|---:|---:|
| `int *p` | yes | yes |
| `const int *p` | yes | no |
| `int const *p` | yes | no |
| `int * const p` | no | yes |
| `const int * const p` | no | no |

`const int *` and `int const *` are the same type. They make the pointed-to `int`
read-only through that pointer. `int * const` makes the pointer object fixed but still
lets you modify the `int` it points at.

For multi-level pointers, each level matters:

```c
const char **a;        // may change *a; may not write **a
char * const *b;       // may not change *b; may write **b
const char * const *c; // may not change *c or write **c
```

This is where vague "const pointer" language fails. Name the level.

## How it really works

`const` is a type qualifier checked by the compiler. It can prevent accidental writes:

```c
int sum(const int *items, size_t count);
```

That signature says `sum` will not mutate the elements through `items`. It can accept
both mutable arrays and const arrays because a mutable object may be viewed through a
read-only pointer. The reverse is not safe: a pointer to const data must not silently
become write-capable.

The promise is local to the access path. In this program, the object changes even though a
const view exists:

```c
int x = 1;
const int *view = &x;
x = 2;        // legal: direct mutable access
```

That is not a contradiction. `view` promised not to write; it did not freeze `x`.

Objects declared `const` are stronger:

```c
const int limit = 10;
```

Writing to `limit` through a casted pointer is undefined behavior. Compilers may place
const objects in read-only memory or assume they do not change. Casting away `const` is
only valid if the original object was not actually declared const and you are restoring a
mutable access path you legitimately own.

`const` also does not manage lifetime. A `const char *` can dangle. It also does not make
shared data thread-safe; two threads can race on a mutable object even if one API sees it
through a `const` pointer.

## Executable artifact: const at different levels

The demo lives in
`examples/pointers-and-memory/const-correctness-and-what-const-really-promises/demo.c`.

```c
// demo.c - shows the difference between a pointer to `const` data, a `const`
// pointer, and APIs that promise to read without mutating.
// Compiles cleanly and runs with:
//
//   gcc -O0 -Wall -Wextra demo.c -o demo && ./demo
#include <stddef.h>
#include <stdio.h>

static int sum_read_only(const int *items, size_t count) {
    int sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += items[i];
    }
    return sum;
}

static void reseat_pointer(int **slot, int *target) {
    *slot = target;
}

static void print_name(const char *name) {
    printf("name                  = %s\n", name);
}

int main(void) {
    int a = 10;
    int b = 20;
    int values[] = {1, 2, 3, 4};

    const int *read_only_view = &a;
    int *const fixed_pointer = &a;
    int *moving_pointer = &a;

    printf("read through const    = %d\n", *read_only_view);
    read_only_view = &b;
    printf("reseated const view   = %d\n", *read_only_view);

    *fixed_pointer = 11;
    printf("wrote via fixed ptr   = %d\n", a);

    reseat_pointer(&moving_pointer, &b);
    *moving_pointer = 21;
    printf("b after reseat write  = %d\n", b);

    printf("sum read-only array   = %d\n",
           sum_read_only(values, sizeof values / sizeof values[0]));

    print_name("low-level atlas");
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
read through const    = 10
reseated const view   = 20
wrote via fixed ptr   = 11
b after reseat write  = 21
sum read-only array   = 10
name                  = low-level atlas
```

The `const int *` view can be reseated from `a` to `b`, but cannot write through that
view. The `int * const` pointer cannot be reseated, but can still write to `a`. The
read-only sum API accepts an ordinary mutable array because read-only access is a safe
restriction.

## Failure modes & trade-offs

- **Thinking `const` means immutable.** It usually means "not through this expression."
  Other aliases may still mutate a non-const object.
- **Casting away real const.** If the original object was declared `const`, writing through
  a casted pointer is undefined behavior.
- **Forgetting lifetime.** `const char *p` can point to a string literal, a stack buffer, a
  heap allocation, or dead storage. Const says nothing about lifetime.
- **Shallow const surprises.** `const struct Table *t` prevents writes to fields through
  `t`, but any pointer fields inside may still point at mutable objects unless their types
  are also const-qualified.
- **Qualifier mismatch in APIs.** A function that only reads but accepts `T *` rejects
  const callers and advertises write capability it does not need.
- **Const as cleanup theater.** Sprinkling `const` inside tiny local scopes is less
  valuable than making API boundaries accurately read-only.

## In practice

- **Put `const` on input buffers.** Prefer `void parse(const char *text)` and
  `int sum(const int *items, size_t count)` for read-only access.
- **Return `const T *` for borrowed read-only views.** It says callers may inspect but not
  mutate through that pointer.
- **Use `T * const` sparingly.** It can clarify local invariants, but API constness is
  usually more important.
- **Do not cast away `const` to satisfy a bad API.** Fix the API when you can; otherwise
  isolate and document the unsafe boundary.
- **Propagate const through nested data deliberately.** If a graph, table, or string array
  should be read-only through an API, qualify the pointed-to layers too.

**Connects to:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|What a pointer really is]] · [[lowlevel/pointers-and-memory/pointers-to-pointers-double-indirection|Pointers to pointers]] · [[lowlevel/pointers-and-memory/void-star-and-type-erasure|void* & type erasure]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]] · [[lowlevel/c-from-the-metal/index|C from the Metal]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — type qualifiers, constraint rules, lvalue access, and undefined behavior after modifying const objects. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — `const` type qualifier** — compact reference for qualifier placement, pointer levels, and constraints. https://en.cppreference.com/w/c/language/const
- **cppreference — Pointer declaration** — pointer declarator syntax and multi-level const examples. https://en.cppreference.com/w/c/language/pointer
- **Jens Gustedt — *Modern C*** — practical API-level const-correctness and pointer qualification discipline. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Robert Seacord — *Effective C*** — secure C guidance on const-correct interfaces, casts, and avoiding accidental mutation. https://nostarch.com/Effective_C
