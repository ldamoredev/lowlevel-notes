#!/bin/sh
# run.sh -- muestra que gcc/clang son drivers que orquestan herramientas internas.
set -e
cd "$(dirname "$0")"
GCC="${GCC:-gcc}"
CLANG="${CLANG:-clang}"

rm -f demo_gcc demo_clang demo.o trace.txt

echo "== build with gcc driver =="
"$GCC" -O0 -Wall -Wextra demo.c -o demo_gcc
./demo_gcc

echo "== build with clang driver =="
"$CLANG" -O0 -Wall -Wextra demo.c -o demo_clang
./demo_clang

echo "== gcc driver identity on this host =="
"$GCC" --version | sed -n '1,2p'

echo "== clang driver phase trace (-###, compile only) =="
"$CLANG" -### -c demo.c -o demo.o 2>trace.txt
grep -o '"-cc1"' trace.txt | head -n 1
grep -o '"-triple" "[^"]*"' trace.txt | head -n 1
grep -o '"-emit-obj"' trace.txt | head -n 1
