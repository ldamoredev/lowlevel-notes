#!/bin/sh
# run.sh -- compila dos object files, inspecciona sus partes y despues linkea.
set -e
cd "$(dirname "$0")"
CC="${CC:-gcc}"

echo "== compile to relocatable object files =="
"$CC" -O0 -Wall -Wextra -c main.c -o main.o
"$CC" -O0 -Wall -Wextra -c support.c -o support.o

echo "== file main.o =="
file main.o

echo "== sections in main.o =="
objdump -h main.o | sed -n '1,18p'

echo "== symbols in main.o =="
nm main.o

echo "== relocations in main.o =="
objdump -r main.o | sed -n '1,20p'

echo "== link and run =="
"$CC" main.o support.o -o demo
./demo
