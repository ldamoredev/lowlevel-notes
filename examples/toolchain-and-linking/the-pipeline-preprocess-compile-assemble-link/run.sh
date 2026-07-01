#!/bin/sh
# run.sh -- recorre preprocess -> compile -> assemble -> link para demo.c.
# Usa gcc por defecto, o CC=clang ./run.sh si queres probar otro driver.
set -e
cd "$(dirname "$0")"
CC="${CC:-gcc}"

echo "== 1) preprocess (-E): .c -> .i =="
"$CC" -E demo.c -o demo.i
wc -l demo.i

echo "== 2) compile (-S): .i -> .s =="
"$CC" -S -O2 -masm=intel demo.i -o demo.s 2>/dev/null || "$CC" -S -O2 demo.i -o demo.s
echo "wrote demo.s"

echo "== 3) assemble (-c): .s -> .o =="
"$CC" -c demo.s -o demo.o
nm demo.o

echo "== 4) link: .o + libc -> executable =="
"$CC" demo.o -o demo
file demo

echo "== run =="
./demo
