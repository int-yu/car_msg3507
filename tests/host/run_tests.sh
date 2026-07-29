#!/usr/bin/env bash
# 在 PC 上编译并运行车端纯逻辑的单元测试。
# 只编译不碰外设的模块，硬件调用一律由 stubs.c 顶掉。
set -e

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/tests/host/build"
mkdir -p "$OUT"

CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -I$ROOT"

echo "--- test_k230link_lane ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_k230link_lane.c" \
    "$ROOT/tests/host/stubs.c" \
    "$ROOT/Application/Comms/K230Link.c" \
    -o "$OUT/test_k230link_lane.exe"
"$OUT/test_k230link_lane.exe"

echo "--- test_heading ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_heading.c" \
    "$ROOT/tests/host/stubs.c" \
    "$ROOT/Application/State/Heading.c" \
    -o "$OUT/test_heading.exe"
"$OUT/test_heading.exe"

echo "--- test_26h ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_26h.c" \
    "$ROOT/Accomplish/26H.c" \
    -o "$OUT/test_26h.exe"
"$OUT/test_26h.exe"

if [ -f "$ROOT/tests/host/test_motionlane.c" ]; then
    echo "--- test_motionlane ---"
    gcc $CFLAGS \
        "$ROOT/tests/host/test_motionlane.c" \
        "$ROOT/tests/host/stubs.c" \
        "$ROOT/Application/Comms/K230Link.c" \
        "$ROOT/Application/Control/MotionLane.c" \
        "$ROOT/Application/State/Heading.c" \
        -o "$OUT/test_motionlane.exe"
    "$OUT/test_motionlane.exe"
fi

echo "所有宿主测试通过"
