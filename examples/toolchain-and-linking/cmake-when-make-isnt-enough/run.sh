#!/bin/sh
# run.sh -- verifica los mismos sources; usa CMake si esta instalado.
set -e
cd "$(dirname "$0")"
CC="${CC:-cc}"

rm -f demo

echo "== cmake configure/build if available =="
if command -v cmake >/dev/null 2>&1; then
    rm -rf build
    cmake -S . -B build
    cmake --build build
    ./build/demo
else
    echo "cmake: not installed on this machine"
    echo "== fallback compile/run with same sources =="
    "$CC" -O0 -Wall -Wextra main.c message.c -o demo
    ./demo
fi
