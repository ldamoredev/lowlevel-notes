#!/bin/sh
# run.sh -- compila y corre demos con ASan, UBSan y TSan cuando esta disponible.
set -e
cd "$(dirname "$0")"
CC="${CC:-clang}"

rm -f asan_demo ubsan_demo tsan_demo asan.out ubsan.out tsan.out tsan-build.txt

echo "== ASan: heap buffer overflow =="
"$CC" -O0 -g -Wall -Wextra -fsanitize=address asan_demo.c -o asan_demo
set +e
ASAN_OPTIONS=abort_on_error=0:halt_on_error=1 ./asan_demo >asan.out 2>&1
asan_status=$?
set -e
echo "asan exit status: $asan_status"
grep -E 'asan before bug|ERROR: AddressSanitizer|heap-buffer-overflow|SUMMARY:' asan.out \
    | sed -E 's/==[0-9]+==/==PID==/g; s/0x[0-9a-fA-F]+/0xADDR/g; s|.*/asan_demo|asan_demo|g; s|.*/asan_demo.c|asan_demo.c|g' \
    | head -n 4

echo "== UBSan: signed integer overflow =="
"$CC" -O0 -g -Wall -Wextra -fsanitize=undefined ubsan_demo.c -o ubsan_demo
set +e
./ubsan_demo >ubsan.out 2>&1
ubsan_status=$?
set -e
echo "ubsan exit status: $ubsan_status"
grep -E 'runtime error|ubsan result' ubsan.out | sed -E 's|.*/ubsan_demo.c|ubsan_demo.c|'

echo "== TSan: data race =="
set +e
"$CC" -O1 -g -Wall -Wextra -fsanitize=thread -pthread tsan_demo.c -o tsan_demo >tsan-build.txt 2>&1
tsan_build_status=$?
set -e
if [ "$tsan_build_status" -ne 0 ]; then
    echo "tsan build failed on this host"
    sed -n '1,3p' tsan-build.txt
    exit 0
fi

set +e
TSAN_OPTIONS=halt_on_error=0:abort_on_error=0:exitcode=66 ./tsan_demo >tsan.out 2>&1
tsan_status=$?
set -e
echo "tsan exit status: $tsan_status"
grep -E 'WARNING: ThreadSanitizer|data race|tsan counter|SUMMARY:' tsan.out \
    | sed -E 's/\(pid=[0-9]+\)/(pid=PID)/g; s/0x[0-9a-fA-F]+/0xADDR/g; s|.*/tsan_demo|tsan_demo|g; s|.*/tsan_demo.c|tsan_demo.c|g' \
    | head -n 6
