---
title: "`make` y el grafo de dependencias de build"
description: make es un ejecutor de grafos de dependencias guiado por timestamps: targets dependen de prerequisites, rules describen cómo reconstruirlos y los builds incrementales salen del grafo.
tags: [toolchain, make, build-systems, dependencies, incremental-builds]
order: 9
updated: 2026-06-22
---
# `make` y el grafo de dependencias de build

`make` no es "un shell script con tabs." Es un motor chico de grafos. Targets dependen de
prerequisites, las rules describen cómo reconstruir targets y los timestamps deciden si un
nodo está stale. Ese modelo existe porque los proyectos C están partidos en translation
units: tocar un `.c` no debería reconstruir el mundo, pero tocar un header compartido sí
debería reconstruir cada object que depende de él.

> El reset: un `Makefile` es un grafo de dependencias más recipes. El grafo decide *si*
> reconstruir; la recipe dice *cómo*.

## Cómo funciona realmente

Una rule tiene target, prerequisites y recipe:

```make
target: prerequisite ...
	command
```

Cuando pedís `make demo`, make chequea si `demo` existe y si algún prerequisite es más
nuevo. Si `main.o` o `math.o` es más nuevo que `demo`, corre la recipe de link. Para decidir
si esos objects están al día, make chequea recursivamente sus propios prerequisites. Esto
convierte una pila de archivos en un dependency graph dirigido.

Las variables mantienen líneas de comando centralizadas. Las pattern rules evitan copiar y
pegar recipes de compile. Variables automáticas como `$@` y `$<` significan "el target" y
"el primer prerequisite." Entonces un proyecto C mínimo puede expresar "cada `.o` sale de
su `.c` y del header compartido" en una rule.

| Idea de make | Significado |
|---|---|
| Target | archivo o nombre phony que le pedís a make construir |
| Prerequisite | archivo o target que debe estar actualizado primero |
| Recipe | comandos shell que corren cuando el target está stale |
| Pattern rule | rule reusable como `%.o: %.c` |
| Phony target | target tipo comando que no es un archivo real de salida |
| Timestamp | señal default de frescura |

Make no parsea C semánticamente por sí solo. Si tu object depende de un header, el
`Makefile` tiene que decirlo, o el compilador tiene que emitir dependency files con opciones
como `-MMD -MP`. Sin edges correctos, make puede decidir honestamente que el grafo está al
día mientras el programa está stale.

## Artefacto ejecutable: mirá reconstruir el grafo

El ejemplo vive en
`examples/toolchain-and-linking/make-and-the-build-dependency-graph/`. Su `Makefile`
contiene una rule de link, una pattern rule de compile y un target `clean`:

```make
CC ?= gcc
CFLAGS ?= -O0 -Wall -Wextra
OBJS = main.o math.o

demo: $(OBJS)
	@echo "LINK $@"
	$(CC) $(OBJS) -o $@

%.o: %.c math.h
	@echo "CC $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) demo
```

El fuente C es deliberadamente chico:

```c
#include <stdio.h>

#include "math.h"

int main(void) {
    printf("make graph result: %d\n", add_bias(31));
    return 0;
}
```

Corrélo:

```bash
cd examples/toolchain-and-linking/make-and-the-build-dependency-graph
./run.sh
```

Salida real de esta máquina:

```text
== clean build ==
CC main.c -> main.o
cc -O0 -Wall -Wextra -c main.c -o main.o
CC math.c -> math.o
cc -O0 -Wall -Wextra -c math.c -o math.o
LINK demo
cc main.o math.o -o demo
== run ==
make graph result: 42
== no-op build query ==
make -q: graph is up to date
== header timestamp change rebuilds dependents ==
CC main.c -> main.o
cc -O0 -Wall -Wextra -c main.c -o main.o
CC math.c -> math.o
cc -O0 -Wall -Wextra -c math.c -o math.o
LINK demo
cc main.o math.o -o demo
```

El edge del header se ve. Tocar `math.h` vuelve stale a `main.o` y `math.o` porque la
pattern rule dice que cada object depende de ese header. Si la rule listara solo `%.c`,
make se perdería el cambio de header y el build podría usar object files stale en silencio.

## Modos de falla y trade-offs

- **Edges de dependencia faltantes.** El bug clásico es cambiar un header y no reconstruir
  todos los objects que lo incluyen.
- **Granularidad de timestamps.** Cambios de archivo muy rápidos o clock skew pueden
  confundir chequeos por timestamp. Archivos generados empeoran esto.
- **Tabs son sintaxis.** Las recipes deben empezar con tab en la sintaxis tradicional de
  make; espacios pueden producir errores confusos.
- **Targets phony pueden chocar con archivos.** Si existe un archivo real llamado `clean` y
  el target no es `.PHONY`, `make clean` puede no hacer nada.
- **Recursive make esconde edges globales.** Partir en muchos sub-makes puede perder
  información de dependencias entre directorios.
- **Make es bajo nivel.** Es excelente para control directo, pero no descubre features de
  plataforma o dependencias de paquetes como generadores más altos.

## En la práctica

- **Modelá primero el grafo.** Preguntá "¿qué archivo produce qué archivo, y qué inputs lo
  vuelven stale?"
- **Usá pattern rules para edges de compile.** Reducen duplicación y mantienen flags
  consistentes entre objects.
- **Generá dependencias de headers.** `-MMD -MP` más `.d` files incluidos es el patrón
  común de producción en C/C++.
- **Mantené recipes aburridas.** Make debería decidir el grafo; las recipes shell deberían
  hacer la acción de build nombrada por el target.
- **Usá make como capa de ejecución.** Incluso cuando CMake u otro generador escribe los
  archivos de build, la idea de abajo sigue siendo un dependency graph.

**Conecta con:** [[lowlevel/toolchain-and-linking/index|Toolchain y Linking]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declaraciones vs definiciones y linkage]] · [[lowlevel/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model|gcc vs clang y el modelo de compiler driver]] · [[lowlevel/toolchain-and-linking/static-linking-and-archives|Static linking y archives]]

## Fuentes

- **GNU Make Manual — Rules** — targets, prerequisites, recipes y lógica de update. https://www.gnu.org/software/make/manual/make.html#Rule-Introduction
- **GNU Make Manual — Pattern Rules** — rules reusables estilo `%.o: %.c` y variables automáticas. https://www.gnu.org/software/make/manual/make.html#Pattern-Rules
- **GNU Make Manual — Phony Targets** — por qué targets tipo comando deberían marcarse `.PHONY`. https://www.gnu.org/software/make/manual/make.html#Phony-Targets
- **GCC Manual — opciones de dependency generation** — `-M`, `-MM`, `-MMD` y flags relacionados para archivos de dependencias de headers. https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html
- **David Drysdale — *Beginner's Guide to Linkers*** — contexto de por qué objects separados y pasos de link crean dependencias reales de build. https://www.lurklurk.org/linkers/linkers.html
