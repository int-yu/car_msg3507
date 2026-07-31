const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');

const root = join(__dirname, '..');
const read = (path) => readFileSync(join(root, path), 'utf8');

const main = read('main.c');
const app = read('Application/Core/App.c');
const task = read('Accomplish/26H.c');
const taskHeader = read('Accomplish/26H.h');
const line = read('Application/Control/MotionLine.c');
const param = read('Application/Debug/Param.c');
const display = read('Application/Debug/DebugDisplay.c');
const displayHeader = read('Application/Debug/DebugDisplay.h');
const stepperHeader = read('Hardware/Motor/Stepper.h');
const grayHeader = read('Hardware/Sensors/Graydetect.h');
const readme = read('README.md');

assert.match(readme, /H 题要求 2/,
    'README 必须标明当前 26H 是要求2单圈巡线');
assert.match(readme, /KEY1.*启动.*单圈/,
    'README 必须说明 KEY1 启动单圈');
assert.match(readme, /KEY1\+KEY2 同时按下/,
    'README 必须记录物理组合急停');
assert.match(readme, /OLED[^\n]*时间/,
    'README must document that OLED only displays time');

assert.doesNotMatch(main, /MAIN_STEPPER_TEST_MODE|Main26H_Run\(\)/,
    'main.c must not keep the old switchable wrapper entry');
assert.match(main, /#include "Accomplish\/26H\.h"/,
    'main.c must select the 26H controller directly');
assert.match(main, /Accomplish26H_Init\(\);/,
    'main.c must initialize the 26H controller');
assert.match(main, /Accomplish26H_Update\(&updateContext\);/,
    'main.c must update 26H after a valid App update');
assert.doesNotMatch(main, /Accomplish\/25H\.h|Mission_Init\(|Mission_Update\(/,
    'the active entry must not launch the legacy 25H Mission');
assert.match(main, /MAIN_BALL_START_KEY_MASK\s+0x02U/,
    'KEY2 must remain the physical ball-task start key');
assert.match(main, /MAIN_EMERGENCY_CHORD_MASK \(0x01U \| 0x02U\)/,
    'main.c must protect KEY1+KEY2 as the emergency chord');
assert.match(main, /BallSequence_Init\(\);/,
    'main.c must initialize the ball task library');
assert.match(main, /BallSequence_Update\(updateContext\.dt\);/,
    'main.c must update the ball task after 26H');
assert.match(main,
    /Main_HasSignal\(&updateContext, MAIN_BALL_START_SIGNAL\)/,
    'C3 must still start the same ball task as KEY2');
assert.match(main,
    /Main_HasSignal\(&updateContext, MAIN_BALL_STOP_SIGNAL\)/,
    'C4 must still stop the ball task');
assert.doesNotMatch(main, /BallHold|MAIN_HOLD|AutoStart|BALL_AUTO/,
    'active main flow must not load the old KEY3/auto ball flows');
assert.match(main, /Telemetry_Update\(updateContext\.elapsedTicks,[\s\S]*?updateContext\.pressedKeys\);/,
    'telemetry sampling must happen after the task controllers in main');
assert.match(main, /BeamActuator_Update\(updateContext\.dt\);/,
    'main.c must apply the latest ball-task target to the stepper actuator');
assert.match(main, /DebugDisplay_Update\(updateContext\.elapsedTicks\);/,
    'main.c must keep the minimal OLED time display refresh');

for (const required of [
    '#include "Application/State/Heading.h"',
    'Heading_Init();',
    'Heading_Calibrate();',
    'Heading_Update(context->dt);',
    'Graydetect_Update();',
    'Stepper_Init();',
    'Stepper_Update(elapsedTicks);',
    'Stepper_EmergencyStop();',
]) {
    assert.ok(app.includes(required), `App must retain ${required}`);
}

assert.match(app, /#define APP_STOP_KEY_CHORD_MASK \(0x01U \| 0x02U\)/,
    'App must define KEY1+KEY2 as the physical emergency-stop chord');
assert.match(app, /context->pressedKeys & APP_STOP_KEY_CHORD_MASK/,
    'App must require both keys to be held for physical emergency stop');

for (const required of [
    'MotionManager_StartLine(',
    'MotionManager_SetLineSpeed(',
    'MotionManager_RequestLineStop(',
    'Graydetect_IsOnline()',
    'Accomplish26H_CountActiveChannels()',
    'ACCOMPLISH_26H_STATE_SETTLING',
    'ACCOMPLISH_26H_ERROR_TIME_LIMIT',
]) {
    assert.ok(task.includes(required), `26H must retain ${required}`);
}
assert.match(taskHeader, /ACCOMPLISH_26H_MARKER_MIN_ACTIVE_CHANNELS/,
    '26H header must expose the six-channel finish-marker criterion');
assert.match(taskHeader, /ACCOMPLISH_26H_FINISH_ROLLOUT_MM/,
    '26H header must expose the physical stop-offset calibration');
assert.match(taskHeader, /ACCOMPLISH_26H_MARKER_CONFIRM_TICKS/,
    '26H header must require several independent marker samples');
assert.match(taskHeader, /Accomplish26H_TuneFinishRolloutMM/,
    '26H header must expose the field parking-offset calibration');
for (const name of [
    'Accomplish26H_TuneCruiseSpeedMMps',
    'Accomplish26H_TuneFinishCrawlSpeedMMps',
    'Accomplish26H_TuneStartClearDistanceMM',
    'Accomplish26H_TuneNominalLapDistanceMM',
    'Accomplish26H_TuneFinishApproachDistanceMM',
    'Accomplish26H_TuneFinishMarkerArmDistanceMM',
    'Accomplish26H_TuneMaxLapDistanceMM',
]) {
    assert.ok(taskHeader.includes(name), `26H must expose ${name}`);
}
assert.ok(task.includes('Accomplish26H_SnapshotParameters()'),
    '26H must snapshot Param values only when KEY1 starts a new run');

assert.ok(line.includes('MotionLine_SetSpeed'),
    'MotionLine must support in-place speed changes without PID reset');
assert.ok(line.includes('MotionLine_RequestStop'),
    'MotionLine must support a zero-speed soft-stop request');
assert.ok(line.includes('MOTION_LINE_ACCELERATION_MMPS2'),
    'MotionLine must ramp cruise speed');
assert.ok(line.includes('MOTION_LINE_MAX_ADJUST_RATE_MMPS2'),
    'MotionLine must limit correction-rate steps');
assert.ok(line.includes('MotionLine_SnapshotTunings()'),
    'MotionLine must snapshot acceleration/deceleration at Start');
assert.match(line, /Odometry_GetDistanceMM|Application\/State\/Odometry\.h/,
    'MotionLine must use encoder odometry to measure the curve hold distance');
assert.ok(line.includes('MOTION_LINE_CURVE_TRIGGER_MASK'),
    'MotionLine must use CH2/CH5 as the curve-entry signal');
assert.ok(line.includes('MOTION_LINE_CURVE_HOLD_DISTANCE_MM') &&
          line.includes('curveEntryDistanceMM'),
    'MotionLine must exit the low-speed arc zone after the configured encoder distance');
assert.ok(line.includes('MotionLine_TuneCurveSpeedMMps'),
    'MotionLine must snapshot the curve speed limit');

assert.ok(param.includes('"h2off"'),
    'K parameter table must expose the H2 parking-offset calibration');
for (const name of [
    'h2cru', 'h2fin', 'h2clr', 'h2lap', 'h2app', 'h2arm', 'h2max',
    'lacc', 'ldec', 'lcra', 'lckd', 'lcv', 'lch',
]) {
    assert.ok(param.includes(`{ "${name}"`),
        `K parameter table must append ${name}`);
}
assert.doesNotMatch(task + read('Application/Control/MotionManager.h') +
    read('Application/Control/MotionManager.c'), /SetLineDeceleration/,
    'H2 must keep the existing MotionLine control path without a runtime deceleration setter');

assert.doesNotMatch(display, /BallSensor_/,
    'OLED must not read the ball sensor');
assert.ok(display.includes('s_elapsedSeconds'),
    'OLED must display elapsed time');
assert.ok(display.includes('OLED_UpdateArea('),
    'OLED refresh must be limited to the time-value area');
assert.doesNotMatch(display, /OLED_Update\(\)|IR:|KEY:|encoderCounts|BALL:/,
    'OLED must not perform full-screen or unrelated status refreshes');
assert.doesNotMatch(displayHeader, /ShowHeadingCalibration/,
    'OLED must not expose the removed MPU calibration page');

assert.match(stepperHeader, /STEPPER_INITIAL_ANGLE_DEG\s+238\.0f/,
    'stepper horizontal angle must be 238 degrees');
assert.match(stepperHeader, /STEPPER_MIN_ANGLE_DEG\s+106\.0f/,
    'stepper minimum limit must be 106 degrees');
assert.match(stepperHeader, /STEPPER_MAX_ANGLE_DEG\s+309\.0f/,
    'stepper maximum limit must be 309 degrees');
assert.match(grayHeader, /GRAYDETECT_ENABLED\s+1U/,
    'complete 26H mode must enable the six-channel infrared sensor');

console.log('26H ring, time display, and smooth-stop contract passed');
