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
    "$ROOT/Application/Control/TaskTimer.c" \
    -o "$OUT/test_26h.exe"
"$OUT/test_26h.exe"

echo "--- test_motionline ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_motionline.c" \
    "$ROOT/Application/Control/MotionLine.c" \
    "$ROOT/Application/Control/PID.c" \
    -lm \
    -o "$OUT/test_motionline.exe"
"$OUT/test_motionline.exe"

echo "--- test_timedlinerun ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_timedlinerun.c" \
    "$ROOT/Application/Control/TimedLineRun.c" \
    "$ROOT/Application/Control/TaskTimer.c" \
    -lm \
    -o "$OUT/test_timedlinerun.exe"
"$OUT/test_timedlinerun.exe"

echo "--- test_motionline_requirement2 ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_motionline_requirement2.c" \
    "$ROOT/Application/Control/MotionLineRequirement2.c" \
    "$ROOT/Application/Control/PID.c" \
    -lm \
    -o "$OUT/test_motionline_requirement2.exe"
"$OUT/test_motionline_requirement2.exe"

echo "--- test_ballsensor ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_ballsensor.c" \
    "$ROOT/Application/Control/BallSensor.c" \
    -lm \
    -o "$OUT/test_ballsensor.exe"
"$OUT/test_ballsensor.exe"

echo "--- test_balltargetcapture ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_balltargetcapture.c" \
    "$ROOT/Application/Control/BallTargetCapture.c" \
    -lm \
    -o "$OUT/test_balltargetcapture.exe"
"$OUT/test_balltargetcapture.exe"

echo "--- test_26h_ball ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_26h_ball.c" \
    "$ROOT/Application/Control/BallSequence.c" \
    "$ROOT/Application/Control/TaskTimer.c" \
    -lm \
    -o "$OUT/test_26h_ball.exe"
"$OUT/test_26h_ball.exe"

echo "--- test_tasktimer ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_tasktimer.c" \
    "$ROOT/Application/Control/TaskTimer.c" \
    -o "$OUT/test_tasktimer.exe"
"$OUT/test_tasktimer.exe"

echo "--- test_26h_ball4 ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_26h_ball4.c" \
    "$ROOT/Application/Control/BallHold.c" \
    -lm \
    -o "$OUT/test_26h_ball4.exe"
"$OUT/test_26h_ball4.exe"

echo "--- test_ballbalance ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_ballbalance.c" \
    "$ROOT/Application/Control/BallBalance.c" \
    -lm \
    -o "$OUT/test_ballbalance.exe"
"$OUT/test_ballbalance.exe"

echo "--- test_beamactuator ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_beamactuator.c" \
    "$ROOT/Application/Control/BeamActuator.c" \
    -lm \
    -o "$OUT/test_beamactuator.exe"
"$OUT/test_beamactuator.exe"

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
