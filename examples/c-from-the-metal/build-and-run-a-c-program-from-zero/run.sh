#!/bin/sh
# run.sh — compila con flags explicitos, corre el programa y hace un chequeo sin link.
set -e
cd "$(dirname "$0")"
CC="${CC:-cc}"

echo "== compile =="
"$CC" -std=c17 -O0 -g -Wall -Wextra -pedantic demo.c -o demo

echo "== run =="
./demo 4

echo "== syntax-only check =="
"$CC" -std=c17 -Wall -Wextra -pedantic -fsyntax-only demo.c
echo "syntax ok"
