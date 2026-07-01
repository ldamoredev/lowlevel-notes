#!/bin/sh
# run.sh -- compila con debug info y usa LLDB en modo batch si esta disponible.
set -e
cd "$(dirname "$0")"
CC="${CC:-gcc}"

rm -f demo lldb-output.txt

echo "== compile with debug info =="
"$CC" -O0 -g -Wall -Wextra demo.c -o demo
./demo

echo "== lldb batch session =="
if command -v lldb >/dev/null 2>&1; then
    lldb -b -s lldb-commands.txt ./demo >lldb-output.txt 2>&1 || {
        sed -n '1,8p' lldb-output.txt
        exit 1
    }
    sed -E 's/0x[0-9a-fA-F]+/0xADDR/g; s/Process [0-9]+/Process PID/g; s/process [0-9]+/process PID/g' lldb-output.txt \
        | grep -E 'Breakpoint|\(int\) value|\(int\) doubled|\(int\) biased|frame #0|rip =|rsp =|rbp =|Process PID exited'
else
    echo "lldb: not installed on this machine"
fi
