#!/bin/sh
# run.sh -- muestra definiciones, referencias undefined y resolucion de link.
set -e
cd "$(dirname "$0")"
CC="${CC:-gcc}"

echo "== compile objects =="
"$CC" -O0 -Wall -Wextra -c main.c -o main.o
"$CC" -O0 -Wall -Wextra -c mathlib.c -o mathlib.o

echo "== nm main.o: references waiting for definitions =="
nm main.o

echo "== nm mathlib.o: exported and internal definitions =="
nm mathlib.o

echo "== link resolves the U symbols =="
"$CC" main.o mathlib.o -o demo
./demo
