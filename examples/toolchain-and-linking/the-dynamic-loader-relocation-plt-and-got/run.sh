#!/bin/sh
# run.sh -- muestra carga dinamica nativa y relocations ELF PIC para GOT/PLT.
set -e
cd "$(dirname "$0")"
CC="${CC:-gcc}"
ELF_CC="${ELF_CC:-clang}"

rm -f plugin.o libplugin.dylib demo elf_pic.o

echo "== native dynamic loader path =="
"$CC" -O0 -Wall -Wextra -fPIC -c plugin.c -o plugin.o
"$CC" -dynamiclib -install_name @rpath/libplugin.dylib plugin.o -o libplugin.dylib
"$CC" -O0 -Wall -Wextra main.c -L. -lplugin -Wl,-rpath,@loader_path -o demo
otool -L demo | sed -n '1,3p'
./demo

echo "== Linux/ELF PIC relocations =="
"$ELF_CC" -target x86_64-unknown-linux-gnu -ffreestanding -fPIC -O0 -Wall -Wextra -c elf_pic.c -o elf_pic.o
file elf_pic.o
objdump -r elf_pic.o

echo "== disassembly around unresolved accesses =="
objdump -d elf_pic.o | sed -n '1,28p'
