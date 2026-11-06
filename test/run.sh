#!/bin/sh
# Host tests. usage: test/run.sh [plain|address|thread]
# Builds into /tmp/orangehud-host-<mode> (or $ORANGEHUD_BUILD_DIR) and runs ctest.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
MODE=${1:-plain}
case "$MODE" in plain) SAN="" ;; address|thread) SAN="-fsanitize=$MODE -fno-omit-frame-pointer -g" ;; *) echo "usage: $0 [plain|address|thread]" >&2; exit 2 ;; esac
OUT=${ORANGEHUD_BUILD_DIR:-/tmp/orangehud-host-$MODE}
mkdir -p "$OUT"
cmake -S "$HERE" -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DORANGEHUD_BUILD_TESTS=ON -DCMAKE_C_FLAGS="$SAN" -DCMAKE_CXX_FLAGS="$SAN" -DCMAKE_EXE_LINKER_FLAGS="$SAN" > "$OUT.configure.log" 2>&1 \
    || { cat "$OUT.configure.log"; exit 1; }
cmake --build "$OUT" > "$OUT.build.log" 2>&1 || { tail -40 "$OUT.build.log"; exit 1; }
ctest --test-dir "$OUT" --output-on-failure
