#include <stddef.h>
#include <stdio.h>

/* Se silencia solo el warning que este demo provoca a proposito. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsizeof-array-argument"
#endif
static void inspect_parameter(int values[], size_t count) {
    /* En parametros de funcion, int values[] se ajusta a int *. */
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

static void inspect_matrix(int matrix[2][3]) {
    int (*row)[3] = matrix;
    ptrdiff_t row_stride = (const char *)(const void *)(row + 1) -
                           (const char *)(const void *)row;

    printf("matrix row stride     = %td bytes\n", row_stride);
}

int main(void) {
    int arr[] = {10, 20, 30, 40, 50};
    size_t count = sizeof arr / sizeof arr[0];

    ptrdiff_t element_stride = (const char *)(const void *)(arr + 1) -
                               (const char *)(const void *)arr;
    ptrdiff_t array_stride = (const char *)(const void *)(&arr + 1) -
                             (const char *)(const void *)&arr;
    int matrix[2][3] = {
        {1, 2, 3},
        {4, 5, 6},
    };

    printf("sizeof arr in main    = %zu bytes\n", sizeof arr);
    printf("elements in main      = %zu\n", count);

    inspect_parameter(arr, count);
    inspect_subscript(arr, 2);

    printf("(void*)arr == (void*)&arr = %s\n",
           (void *)arr == (void *)&arr ? "true" : "false");
    printf("arr + 1 byte delta    = %td bytes\n", element_stride);
    printf("&arr + 1 byte delta   = %td bytes\n", array_stride);

    inspect_matrix(matrix);

    return 0;
}
