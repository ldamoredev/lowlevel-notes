#!/bin/sh
# run.sh -- compila un workload y reporta disponibilidad local de profilers Linux.
set -e
cd "$(dirname "$0")"
CC="${CC:-cc}"

rm -f demo

echo "== build workload =="
"$CC" -O2 -g -Wall -Wextra workload.c -o demo
./demo

echo "== tool availability on this machine =="
if command -v valgrind >/dev/null 2>&1; then
    echo "valgrind: available"
    valgrind --tool=memcheck --leak-check=full ./demo 2>&1 | grep -E 'profile checksum|HEAP SUMMARY|definitely lost|ERROR SUMMARY' | head -n 6
else
    echo "valgrind: not installed on this machine"
fi

if command -v perf >/dev/null 2>&1; then
    echo "perf: available"
else
    echo "perf: not installed on this machine"
fi

if command -v valgrind >/dev/null 2>&1; then
    echo "cachegrind: available through valgrind --tool=cachegrind"
else
    echo "cachegrind: not available without valgrind"
fi
