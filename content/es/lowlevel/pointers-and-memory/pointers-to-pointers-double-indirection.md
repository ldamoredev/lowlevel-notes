---
title: "Punteros a punteros (doble indirección)"
description: Un puntero-a-puntero es un puntero a un objeto puntero. Permite que una función reemplace el puntero del caller, modele arrays de punteros y edite links en estructuras basadas en punteros.
tags: [pointers, double-indirection, out-parameters, linked-lists, ownership]
order: 5
updated: 2026-06-30
---
# Punteros a punteros (doble indirección)

Un puntero-a-puntero es exactamente lo que dice el tipo: un puntero cuyo objeto destino es
otro puntero. `T *` te deja alcanzar un `T`; `T **` te deja alcanzar un `T *` para leer o
reemplazar el valor de puntero guardado ahí. Ese nivel extra es lo que permite que una
función cambie el puntero del caller, devuelva objetos asignados mediante out-parameters y
edite estructuras enlazadas sin casos especiales. El costo es cognitivo: cada `*` cambia
de qué objeto estás hablando.

> El reset: `int **pp` no apunta a un `int`. Apunta a un `int *`. `*pp` es el objeto
> puntero, y `**pp` es el `int` alcanzado después de seguir ambos niveles.

## La estrella extra cambia qué podés mutar

Arrancá con un objeto y un puntero:

```c
int value = 7;
int *p = &value;
int **pp = &p;
```

Los niveles son:

| Expresión | Tipo | Objeto alcanzado |
|---|---|---|
| `value` | `int` | el objeto entero |
| `p` | `int *` | un objeto puntero que guarda `&value` |
| `*p` | `int` | el entero alcanzado a través de `p` |
| `pp` | `int **` | un objeto puntero que guarda `&p` |
| `*pp` | `int *` | el objeto puntero del caller, `p` |
| `**pp` | `int` | el entero alcanzado a través de `p` |

Pasar `p` a una función deja que la función modifique `*p`, el `int` apuntado. Pasar `&p`
deja que la función modifique `p` en sí. Esa es toda la razón por la que `T **` aparece en
APIs de C: el callee necesita reemplazar una variable puntero poseída por el caller.

## Cómo funciona realmente

C pasa argumentos por valor. Si llamás a:

```c
void set_to_null(int *p) {
    p = NULL;
}
```

la asignación cambia solo la copia local del puntero dentro de la función. El puntero del
caller queda intacto. Para modificar el objeto puntero del caller, pasá su dirección:

```c
void set_to_null(int **slot) {
    *slot = NULL;
}
```

`slot` es un puntero a la variable puntero del caller. `*slot = NULL` escribe en esa
variable. El mismo patrón se usa para out-parameters de allocation:

```c
bool make_widget(struct Widget **out);
```

La API dice: "dame la dirección de tu `struct Widget *`; si tengo éxito, voy a guardar ahí
un nuevo puntero owning." Esto es común cuando el valor de retorno se necesita para estado
o reporte de error.

La doble indirección también es una forma limpia de editar estructuras enlazadas. En una
lista simplemente enlazada, cada "link" es un `struct Node *`: o el puntero head o un
campo `next`. Un cursor de tipo `struct Node **` puede apuntar al link que actualmente
lleva al lugar que querés modificar. Insertar al principio e insertar después de un nodo
se vuelven la misma operación: reemplazar `*link`.

Los arrays de punteros usan la misma sintaxis pero otra idea. `char **argv` en `main`
apunta al primer elemento de un array de valores `char *`. No es un array bidimensional de
caracteres; es un array de punteros, y cada puntero puede apuntar a un string guardado en
otro lado. Esta distinción espeja la lección anterior de arrays: `T **` no es el mismo
layout que `T rows[][cols]`.

`const` se vuelve ruidoso a este nivel porque hay varias cosas que pueden ser const:

```c
const char **a;        // puntero a puntero a const char: puede cambiar *a
char * const *b;       // puntero a puntero const a char: no puede cambiar *b
const char * const *c; // puntero a puntero const a const char
```

Leé de adentro hacia afuera: cuál es el objeto apuntado en cada nivel, y qué nivel está
protegido contra escrituras.

## Artefacto ejecutable: mutá el puntero del caller

El demo vive en `examples/pointers-and-memory/pointers-to-pointers-double-indirection/demo.c`.

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

Compilá y corré:

```bash
gcc -O0 -Wall -Wextra demo.c -o demo
./demo
```

Salida real:

```
value through p       = 7
value after **pp      = 42
owned from out-param  = 99
list after append     = 10 20 30
list after free       = NULL
```

`**pp = 42` cambia el `value` original a través de dos niveles. `make_owned_int(&owned,
99)` guarda un puntero de heap en la variable `owned` del caller. La función `append`
recorre un cursor `struct Node **` hasta que apunta al link que debería reemplazarse, así
que appendear a una lista vacía y appendear después del tail usan la misma escritura:
`*head = node`.

## Modos de falla y trade-offs

- **Perder el nivel.** `p`, `*p` y `**p` nombran objetos distintos. Una dereferencia
  equivocada puede sobrescribir la variable puntero cuando querías sobrescribir el objeto,
  o al revés.
- **Ownership ambiguo en out-parameters.** `T **out` tiene que decir si el callee asigna,
  presta, transfiere ownership o solo apunta a storage existente. El tipo solo no lo dice.
- **Inicialización parcial.** Si una función toma varios out-parameters, definí qué pasa
  ante falla. Dejar algunos outputs escritos y otros indeterminados es una trampa de
  cleanup.
- **`T **` vs arrays multidimensionales.** `int **` describe un puntero a puntero. No
  describe storage contiguo `int rows[3][4]`, cuyo tipo de parámetro decaído es
  `int (*)[4]`.
- **Agujeros de `const`.** Convertir entre `T **` y `const T **` no es seguro como mucha
  gente espera; puede dejar que el código cuele un puntero no-const hacia storage const.
- **Costo de legibilidad.** La doble indirección puede eliminar casos especiales en
  estructuras de datos, pero más allá de dos niveles el código suele necesitar un helper
  type chico, nombres más claros o una API distinta.

## En la práctica

- **Usá nombres que expongan el nivel.** `out`, `slot`, `link`, `headp` y `cursor` son más
  claros que otro `p` genérico.
- **Documentá estados de out-parameters.** Decí qué escribe el callee en success, qué
  escribe en failure y quién posee el puntero resultante.
- **Inicializá outputs antes del trabajo riesgoso.** Poner `*out = NULL` temprano vuelve
  más simples y seguros los caminos de cleanup.
- **Usá `Node **link` para ediciones de listas simplemente enlazadas.** Es uno de los
  lugares raros donde la doble indirección vuelve el código más chico y más correcto.
- **Separá arrays de punteros y arrays 2D reales.** Layouts estilo `char **argv` son arrays
  de punteros; storage contiguo tipo matriz debería usar tipos puntero-a-array.
- **Frená en dos niveles salvo que haya una razón fuerte.** `T ***` a veces es real, pero
  debería disparar una revisión de diseño de API.

**Conecta con:** [[lowlevel/pointers-and-memory/what-a-pointer-really-is|Qué es realmente un puntero]] · [[lowlevel/pointers-and-memory/pointer-arithmetic-and-stride|Aritmética de punteros y stride]] · [[lowlevel/pointers-and-memory/the-heap-malloc-free-and-the-allocator-underneath|El heap: malloc/free y el allocator por debajo]] · [[lowlevel/pointers-and-memory/index|Punteros y Memoria]] · [[lowlevel/c-from-the-metal/index|C desde el Metal]]

## Fuentes

- **ISO/IEC 9899 (working drafts del estándar C en WG14)** — declarators de punteros, indirección, asignación, pasaje de argumentos en llamadas a función y reglas de qualification. https://www.open-std.org/jtc1/sc22/wg14/
- **cppreference — Pointer declaration** — referencia compacta para sintaxis de puntero-a-puntero, null pointers, parámetros de función e indirección multinivel. https://en.cppreference.com/w/c/language/pointer
- **cppreference — Type qualifiers** — cómo aplica `const` en distintos niveles de puntero y por qué las conversiones de qualifiers se vuelven delicadas con `T **`. https://en.cppreference.com/w/c/language/const
- **Jens Gustedt — *Modern C*** — tratamiento moderno de parámetros puntero, out-parameters, disciplina de ownership y estructuras enlazadas. https://gustedt.gitlabpages.inria.fr/modern-c/
- **Richard Reese — *Understanding and Using C Pointers*** — ejemplos prácticos de punteros a punteros: out-parameters, arrays de punteros y manipulación de listas enlazadas. https://www.oreilly.com/library/view/understanding-and-using/9781449344535/
- **Beej's Guide to C — Pointers** — explicación accesible de niveles de puntero, `argv` y pasar punteros a funciones. https://beej.us/guide/bgc/html/split/pointers.html
