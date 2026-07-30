const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');

const root = join(__dirname, '..');
const read = (path) => readFileSync(join(root, path), 'utf8');

const main = read('main.c');
const fullMain = read('Application/Core/Main26H.c');
const app = read('Application/Core/App.c');
const task = read('Accomplish/26H.c');
const taskHeader = read('Accomplish/26H.h');
const line = read('Application/Control/MotionLine.c');
const param = read('Application/Debug/Param.c');
const display = read('Application/Debug/DebugDisplay.c');
const displayHeader = read('Application/Debug/DebugDisplay.h');
const readme = read('README.md');
const ball4 = read('Application/Control/BallHold.c');
const ball4Header = read('Application/Control/BallHold.h');

assert.match(readme, /H 题要求 2/,
    'README 必须标明当前 26H 是要求2单圈巡线');
assert.match(readme, /KEY1.*启动.*单圈/,
    'README 必须说明 KEY1 启动单圈');
assert.match(readme, /KEY1\+KEY2 同时按下/,
    'README 必须记录物理组合急停');
assert.match(readme, /IR:CH1~CH6/,
    'README 必须记录 OLED 六路红外显示');

assert.match(main, /MAIN_STEPPER_TEST_MODE\s+1U/,
    'main.c must default to the current stepper test');
assert.match(main, /Main26H_Run\(\);/,
    'main.c must retain a switchable complete 26H entry');
assert.match(fullMain, /#include "Accomplish\/26H\.h"/,
    'Main26H.c must select the 26H controller');
assert.match(fullMain, /Accomplish26H_Init\(\);/,
    'Main26H.c must initialize the 26H controller');
assert.match(fullMain, /Accomplish26H_Update\(&updateContext\);/,
    'Main26H.c must update 26H after a valid App update');
assert.doesNotMatch(fullMain, /Accomplish\/25H\.h|Mission_Init\(|Mission_Update\(/,
    'the 26H entry must not launch the legacy 25H Mission');
assert.match(fullMain, /MAIN_HOLD_START_SIGNAL\s+5U/,
    'C5 must be reserved for requirement 4 O-point hold start');
assert.match(fullMain, /MAIN_HOLD_STOP_SIGNAL\s+6U/,
    'C6 must be reserved for requirement 4 O-point hold stop');
assert.match(fullMain, /holdStartRequested[\s\S]*?Main26H_ReportHoldStart\(\);/,
    'C5 must invoke the requirement 4 start path');
assert.match(fullMain, /if \(holdStopRequested != 0U\)[\s\S]*?BallHold_Stop\(\);/,
    'C6 must stop the requirement 4 balance task');
assert.ok(ball4.includes('BallBalance_SetTarget(BALL_HOLD_TARGET_MM)'),
    'requirement 4 must use its fixed O-point target');
assert.ok(ball4.includes('BallBalance_SetCarAcceleration(0.0f)'),
    'stationary O-point hold must not apply chassis feedforward');
assert.doesNotMatch(ball4Header, /TuneFeedforward|TuneConvergeTimeout/,
    'requirement 4 must not publish runtime tuning variables');

for (const required of [
    '#include "Application/State/Heading.h"',
    'Heading_Init();',
    'DebugDisplay_ShowHeadingCalibration(Heading_IsReady());',
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

assert.match(display, /#include "Accomplish\/26H\.h"/,
    'OLED must read the 26H timer');
assert.match(display, /Accomplish26H_GetElapsedTicks\(\)/,
    'OLED must read accumulated integer ticks');
assert.ok(display.includes('"T:%lu.%02lus"'),
    'OLED first row must show seconds and centiseconds');
assert.ok(display.includes('"IR:"'),
    'OLED must render the six-channel infrared state');
assert.match(display, /#include "Hardware\/Motor\/Stepper\.h"/,
    'OLED must include the stepper status interface');
assert.ok(display.includes('Stepper_GetStatus(&stepperStatus);'),
    'OLED must read the stepper status');
assert.ok(display.includes('"EC:"') && display.includes('encoderCounts'),
    'OLED must retain the historical stepper encoder-count row');
assert.ok(display.includes('"EA:"') && display.includes('multiTurnAngleDeg'),
    'OLED must retain the historical stepper angle row');
assert.ok(display.includes('Accomplish26H_GetState()'),
    'OLED must expose a 26H task error');
assert.ok(displayHeader.includes('DebugDisplay_ShowHeadingCalibration'),
    'the display header must retain the MPU calibration page API');

console.log('26H ring, infrared display, and smooth-stop contract passed');
