#!/bin/sh
# run.sh — watch demo.c pass through every stage of the build pipeline.
# Works with gcc or clang. On Linux you get ELF (inspect with readelf/objdump);
# on macOS you get Mach-O (inspect with otool). The STAGES are identical.
set -e
cd "$(dirname "$0")"
CC="${CC:-gcc}"

echo "== 1) preprocess (-E): headers/macros expanded =="
$CC -E demo.c | wc -l                     # ~560 lines from 6

echo "== 2) compile (-S): C -> assembly =="
$CC -S -O2 -masm=intel demo.c -o demo.s 2>/dev/null || $CC -S -O2 demo.c -o demo.s
echo "   wrote demo.s"

echo "== 3) assemble (-c): assembly -> object, with an unresolved symbol =="
$CC -c demo.c -o demo.o
nm demo.o                                 # _printf shows as 'U' (undefined)

echo "== 4) link: resolve symbols -> executable =="
$CC demo.o -o demo
file demo

echo "== run =="
./demo
