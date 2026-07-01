#!/bin/sh
# run.sh -- muestra como un static archive se extrae por simbolos y orden de link.
set -e
cd "$(dirname "$0")"
CC="${CC:-gcc}"

rm -f main.o add.o mul.o libcalc.a demo demo_wrong link-error.txt

echo "== native archive and run =="
"$CC" -O0 -Wall -Wextra -c main.c add.c mul.c
ar rcs libcalc.a add.o mul.o
ar -t libcalc.a

echo "== symbols inside archive members =="
nm -g libcalc.a

echo "== library before object on this Apple ld =="
"$CC" -L. -lcalc main.o -o demo_wrong
./demo_wrong

echo "== portable order: object before library =="
"$CC" main.o -L. -lcalc -o demo
./demo

echo "== portability note =="
echo "GNU/ELF linkers commonly require the portable order above for static archives."
