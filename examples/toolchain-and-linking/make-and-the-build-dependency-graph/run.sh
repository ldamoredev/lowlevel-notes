#!/bin/sh
# run.sh -- muestra el grafo de dependencias incremental de make.
set -e
cd "$(dirname "$0")"

echo "== clean build =="
make clean >/dev/null
make

echo "== run =="
./demo

echo "== no-op build query =="
if make -q; then
    echo "make -q: graph is up to date"
else
    echo "unexpected dirty graph"
    exit 1
fi

echo "== header timestamp change rebuilds dependents =="
sleep 1
touch math.h
make
