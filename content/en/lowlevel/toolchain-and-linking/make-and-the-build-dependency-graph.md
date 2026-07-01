---
title: "`make` and the build dependency graph"
description: make is a timestamp-driven dependency graph executor: targets depend on prerequisites, rules describe how to rebuild them, and incremental builds fall out of the graph.
tags: [toolchain, make, build-systems, dependencies, incremental-builds]
order: 9
updated: 2026-06-22
---
# `make` and the build dependency graph

`make` is not "a shell script with tabs." It is a small graph engine. Targets depend on
prerequisites, rules describe how to rebuild targets, and timestamps decide whether a node
is stale. That model exists because C projects are split across translation units: touching
one `.c` should not rebuild the world, but touching a shared header should rebuild every
object that depends on it.

> The reset: a `Makefile` is a dependency graph plus recipes. The graph decides *whether*
> to rebuild; the recipe says *how*.

## How it really works

A rule has a target, prerequisites, and a recipe:

```make
target: prerequisite ...
	command
```

When you ask for `make demo`, make checks whether `demo` exists and whether any prerequisite
is newer. If `main.o` or `math.o` is newer than `demo`, the link recipe runs. To decide
whether those objects are current, make recursively checks their own prerequisites. This
turns a pile of files into a directed dependency graph.

Variables keep command lines centralized. Pattern rules keep repeated compile recipes from
being copy-pasted. Automatic variables such as `$@` and `$<` mean "the target" and "the
first prerequisite." A tiny C project can therefore express "every `.o` comes from its `.c`
and shared header" in one rule.

| Make idea | Meaning |
|---|---|
| Target | file or phony name you ask make to build |
| Prerequisite | file or target that must be current first |
| Recipe | shell commands run when target is stale |
| Pattern rule | reusable rule such as `%.o: %.c` |
| Phony target | command-like target that is not a real output file |
| Timestamp | default freshness signal |

Make does not parse C semantically by itself. If your object depends on a header, the
`Makefile` must say so, or the compiler must emit dependency files with options such as
`-MMD -MP`. Without correct edges, make can honestly decide the graph is current while the
program is stale.

## Executable artifact: watch the graph rebuild

The example lives in
`examples/toolchain-and-linking/make-and-the-build-dependency-graph/`. Its `Makefile`
contains one link rule, one pattern compile rule, and a `clean` target:

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

The C source is deliberately small:

```c
#include <stdio.h>

#include "math.h"

int main(void) {
    printf("make graph result: %d\n", add_bias(31));
    return 0;
}
```

Run it:

```bash
cd examples/toolchain-and-linking/make-and-the-build-dependency-graph
./run.sh
```

Real output from this machine:

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

The header edge is visible. Touching `math.h` makes both `main.o` and `math.o` stale because
the pattern rule says every object depends on that header. If the rule only listed `%.c`,
make would miss the header change and the build could silently use stale object files.

## Failure modes & trade-offs

- **Missing dependency edges.** The classic bug is changing a header and not rebuilding all
  objects that include it.
- **Timestamp granularity.** Very fast file changes or clock skew can confuse timestamp
  checks. Generated files make this worse.
- **Tabs are syntax.** Recipes must begin with a tab in traditional make syntax; spaces can
  produce confusing errors.
- **Phony targets can collide with files.** If a real file named `clean` exists and the
  target is not `.PHONY`, `make clean` may do nothing.
- **Recursive make hides global graph edges.** Splitting into many sub-makes can lose
  cross-directory dependency information.
- **Make is low-level.** It is excellent for direct control, but it does not discover
  platform features or package dependencies the way higher-level generators do.

## In practice

- **Model the graph first.** Ask "what file produces what file, and what inputs make it
  stale?"
- **Use pattern rules for compile edges.** They reduce duplication and make flags
  consistent across objects.
- **Generate header dependencies.** `-MMD -MP` plus included `.d` files is the common C/C++
  production pattern.
- **Keep recipes boring.** Make should decide the graph; shell recipes should do the one
  build action named by the target.
- **Use make as the execution layer.** Even when CMake or another generator writes the
  build files, the underlying idea is still a dependency graph.

**Connects to:** [[lowlevel/toolchain-and-linking/index|Toolchain & Linking]] · [[lowlevel/c-from-the-metal/translation-units-declarations-and-linkage|Translation units, declarations vs definitions, and linkage]] · [[lowlevel/toolchain-and-linking/gcc-vs-clang-and-the-compiler-driver-model|gcc vs clang and the compiler driver model]] · [[lowlevel/toolchain-and-linking/static-linking-and-archives|Static linking and archives]]

## Sources

- **GNU Make Manual — Rules** — targets, prerequisites, recipes, and update logic. https://www.gnu.org/software/make/manual/make.html#Rule-Introduction
- **GNU Make Manual — Pattern Rules** — reusable `%.o: %.c`-style rules and automatic variables. https://www.gnu.org/software/make/manual/make.html#Pattern-Rules
- **GNU Make Manual — Phony Targets** — why command-like targets should be marked `.PHONY`. https://www.gnu.org/software/make/manual/make.html#Phony-Targets
- **GCC Manual — dependency generation options** — `-M`, `-MM`, `-MMD`, and related flags for header dependency files. https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html
- **David Drysdale — *Beginner's Guide to Linkers*** — context for why separate objects and link steps create real build dependencies. https://www.lurklurk.org/linkers/linkers.html
