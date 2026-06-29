#!/bin/sh
# run.sh — compila dos translation units por separado y deja que el linker las cosa.
set -e
cd "$(dirname "$0")"
CC="${CC:-gcc}"

echo "== compile each translation unit to an object file =="
"$CC" -O0 -Wall -Wextra -c counter.c -o counter.o
"$CC" -O0 -Wall -Wextra -c main.c -o main.o

echo "== link the objects into one executable =="
"$CC" counter.o main.o -o demo

echo "== run =="
./demo

echo "== nm counter.o =="
nm counter.o

echo "== nm main.o =="
nm main.o
