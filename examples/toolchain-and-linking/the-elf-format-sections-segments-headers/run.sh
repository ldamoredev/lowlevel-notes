#!/bin/sh
# run.sh -- emite un object file ELF x86-64 relocatable y muestra sus tablas.
set -e
cd "$(dirname "$0")"
CC="${CC:-clang}"

echo "== compile a Linux/ELF relocatable object =="
"$CC" -target x86_64-unknown-linux-gnu -ffreestanding -O0 -Wall -Wextra -c elf_demo.c -o elf_demo.o

echo "== identify format =="
file elf_demo.o

echo "== section headers =="
objdump -h elf_demo.o | sed -n '1,24p'

echo "== symbol table =="
objdump -t elf_demo.o | sed -n '/SYMBOL TABLE/,$p'

echo "== relocations =="
objdump -r elf_demo.o

echo "== note =="
echo "This is a relocatable ELF object. Executables and shared libraries add program headers/segments."
