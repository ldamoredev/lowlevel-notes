#!/bin/sh
# run.sh -- construye una shared library nativa y muestra la dependencia runtime.
set -e
cd "$(dirname "$0")"
CC="${CC:-gcc}"

rm -f greeting.o libgreeting.dylib demo

echo "== compile position-independent object =="
"$CC" -O0 -Wall -Wextra -fPIC -c greeting.c -o greeting.o

echo "== build shared library =="
"$CC" -dynamiclib -install_name @rpath/libgreeting.dylib greeting.o -o libgreeting.dylib
file libgreeting.dylib

echo "== link executable with runtime search path =="
"$CC" -O0 -Wall -Wextra main.c -L. -lgreeting -Wl,-rpath,@loader_path -o demo

echo "== dynamic dependencies =="
otool -L demo | sed -n '1,4p'

echo "== run =="
./demo
