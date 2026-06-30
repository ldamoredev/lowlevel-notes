#!/bin/sh
# run.sh — emití assembly real desde demo.c y corré el binario local.
set -eu
cd "$(dirname "$0")"

CC="${CC:-gcc}"
CLANG="${CLANG:-clang}"

echo "== compilar y correr con warnings =="
$CC -O0 -Wall -Wextra demo.c -o demo
./demo

echo
echo "== x86-64 Intel: gcc -S -O2 -masm=intel =="
$CC -S -O2 -masm=intel demo.c -o demo.s
sed -n '1,170p' demo.s

echo
echo "== ARM64: clang -S -O2 -arch arm64 =="
$CLANG -S -O2 -arch arm64 demo.c -o demo.arm64.s
sed -n '1,170p' demo.arm64.s
