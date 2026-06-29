---
title: "Arrays and array-to-pointer decay"
description: C arrays are contiguous objects, not pointers, but in most expressions an array name converts to a pointer to its first element. The exceptions, the sizeof trap, arr vs &arr, and why multidimensional arrays are not T**.
tags: [c, arrays, pointers, sizeof, decay]
order: 7
updated: 2026-06-29
---
# Arrays and array-to-pointer decay

An array in C is a real object: `N` contiguous elements of type `T`. But in most expression
contexts, the array expression **decays** to a `T *` pointing at the first element,
`&arr[0]`. That is why array indexing and pointer arithmetic feel interchangeable, and why
arrays become sharp the moment you pass them to a function. The array did not become a
pointer everywhere; one expression context converted it.

> The reset: array-to-pointer decay is not "arrays are pointers." It is "an array
> expression usually converts to a pointer to element zero, except in a few important
> contexts."

## The decay rule

For an array object:

```c
int a[5] = {10, 20, 30, 40, 50};
```

The expression `a` usually converts to `int *`, with the value `&a[0]`. Pointer arithmetic
then scales by `sizeof *a`, so `a + 2` means "two `int` elements after `a[0]`." Indexing is
defined in terms of that arithmetic:

```c
a[i]      == *(a + i)
i[a]      == *(i + a)
```

The second form is legal because addition is commutative at the expression level: one
operand is the pointer and the other is the integer. It is also a good reminder that
`[]` is not bounds checking. It is pointer arithmetic plus dereference.

Decay does **not** happen in these key contexts:

| Context | Result |
|---|---|
| `sizeof arr` | size of the whole array object |
| `&arr` | pointer to the whole array, type `T (*)[N]` |
| string literal initializing an array | `char s[] = "hi";` creates an array with copied bytes |

That exception list is small and essential. `sizeof arr` is useful only while `arr` is
still an array object in the current scope. `&arr` has the same address value as `arr` when
printed as `void *`, but a different type: pointer to array, not pointer to first element.

## How it really works

The compiler knows the array bound when the expression still has array type. In `main`,
`sizeof arr` is `5 * sizeof(int)`, so on the machine used for this demo it is 20 bytes.
But when you pass `arr` to a function, the argument expression decays before the call. A
parameter written as `int values[]` is adjusted by the language to `int *values`; the
function receives a pointer, not an array object.

That is the classic length-loss bug:

```c
void inspect(int values[]) {
    sizeof values;  // sizeof(int *), not the caller's array
}
```

Compilers often warn about this because it is almost always a mistake. The rule is simple:
pass the length with the pointer. `sizeof(arr) / sizeof(arr[0])` is valid only in the scope
where `arr` is still a real array, not after it has crossed a function boundary.

`arr` and `&arr` also explain why type matters more than printed address. Converted to
`void *`, both point at the same byte. But `arr + 1` advances by one element, while
`&arr + 1` advances by the whole array:

```c
arr + 1   // int *: one int later
&arr + 1  // int (*)[5]: one array-of-5-int later
```

Multidimensional arrays follow the same rule, without becoming `T **`. For `int m[2][3]`,
the expression `m` decays to `int (*)[3]`: a pointer to a row of three `int`. The column
count is part of the pointed-to type, so pointer arithmetic can jump exactly one row. A
separately allocated "array of pointers to rows" is a different layout and a different
type.

VLAs, variable length arrays, carry a runtime bound in their array type while in scope, but
they still decay in expression and function-call contexts. Array is not pointer, even when
array expressions often behave pointer-like.

## Executable artifact: watch the type vanish

The demo lives in `examples/c-from-the-metal/arrays-and-array-to-pointer-decay/demo.c`.
It intentionally computes `sizeof` on an adjusted function parameter, so the source
locally disables that one teaching warning while keeping the `-Wall -Wextra` build clean.

```c
#include <stddef.h>
#include <stdio.h>

/* Silence only the warning this demo intentionally triggers. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsizeof-array-argument"
#endif
static void inspect_parameter(int values[], size_t count) {
    /* In function parameters, int values[] is adjusted to int *. */
    printf("sizeof parameter      = %zu bytes\n", sizeof values);
    printf("count passed in       = %zu elements\n", count);
}
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static void inspect_subscript(const int *values, size_t index) {
    int via_index = values[index];
    int via_pointer = *(values + index);
    int via_reversed = index[values];

    printf("a[2], *(a + 2), 2[a] = %d, %d, %d\n",
           via_index, via_pointer, via_reversed);
}
```

Compile and run:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Real output:

```
sizeof arr in main    = 20 bytes
elements in main      = 5
sizeof parameter      = 8 bytes
count passed in       = 5 elements
a[2], *(a + 2), 2[a] = 30, 30, 30
(void*)arr == (void*)&arr = true
arr + 1 byte delta    = 4 bytes
&arr + 1 byte delta   = 20 bytes
matrix row stride     = 12 bytes
```

The first `sizeof` sees the real array. The second sees a pointer parameter. The subscript
line shows the definition of `[]`. The address comparison is true after both expressions
are converted to `void *`, but the byte deltas reveal the type difference: `arr + 1` moves
one `int`; `&arr + 1` moves the whole `int[5]`. The matrix stride is 12 bytes because
`int matrix[2][3]` decays to a pointer to a three-`int` row.

## Failure modes & trade-offs

- **Lost length.** A function parameter `T a[]` does not carry the caller's element count.
  If the function needs bounds, pass `size_t count` next to the pointer.
- **False `ARRAY_LEN` confidence.** `sizeof(a) / sizeof(a[0])` is good for real arrays and
  wrong for decayed pointers. Macro-based `ARRAY_LEN` helpers cannot rescue a pointer.
- **`T **` confusion.** A two-dimensional array is contiguous row-major storage. It decays
  to a pointer to row, not to a pointer to pointer. APIs must spell the column count:
  `void f(size_t rows, size_t cols, int m[rows][cols])` or `int (*m)[COLS]`.
- **One-past is not readable.** `arr + count` is a valid one-past pointer for comparison or
  loop termination. Dereferencing it is undefined behavior.
- **API ergonomics vs safety.** C's pointer-plus-length convention is explicit and cheap,
  but it is easy to split the pair. Stronger abstractions wrap pointer and length in a
  struct.

## In practice

- **Pass pointer and length together.** Prefer `void process(int *items, size_t count)` or
  a small slice struct over a naked pointer.
- **Compute array length before decay.** In the same scope as `int items[N]`, use
  `sizeof items / sizeof items[0]`; after a call boundary, trust the explicit `count`.
- **Read function parameters honestly.** `int a[]`, `int a[10]`, and `int *a` are the same
  parameter type for a one-dimensional array parameter.
- **Use `&arr` when you mean the whole array type.** It is rare, but useful for APIs that
  require an exact bound, such as `int (*p)[5]`.
- **Spell multidimensional array APIs with the trailing dimension.** `int m[][3]` or
  `int (*m)[3]` preserves row stride; `int **` describes a different structure.

**Connects to:** [[lowlevel/c-from-the-metal/index|C from the Metal]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declarations, and linkage]] · [[lowlevel/c-from-the-metal/the-c-type-system-is-weak|The C type system is weak]] · [[lowlevel/c-from-the-metal/undefined-behavior-the-contract|Undefined behavior: the contract]] · [[lowlevel/machine-model/bits-bytes-words-and-addresses|Bits, bytes, words & addresses]] · [[lowlevel/pointers-and-memory/index|Pointers & Memory]]

## Sources

- **ISO/IEC 9899 (WG14 C standard working drafts)** — the authority for array declarators, array-to-pointer conversion, `sizeof`, subscripting, and function parameter adjustment. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Array declaration** — compact reference for array types, array-to-pointer conversion, multidimensional arrays, and function parameter adjustment. https://en.cppreference.com/c/language/array
- **cppreference — `sizeof` operator** — exact cases where `sizeof` evaluates object size and where array-to-pointer conversion does not happen. https://en.cppreference.com/c/language/sizeof
- **cppreference — Operator precedence and member access** — reference for subscripting as an operator expression and how it relates to pointer arithmetic. https://en.cppreference.com/c/language/operator_precedence
- **Jens Gustedt — *Modern C*** — modern treatment of arrays, pointer parameters, VLAs, and array API design. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Beej's Guide to C — Arrays** — approachable examples of array storage, indexing, function parameters, and why passing length matters. https://beej.us/guide/bgc/html/split/arrays.html
