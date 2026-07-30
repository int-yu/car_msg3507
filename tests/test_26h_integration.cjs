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
const readme = read('README.md');

assert.match(readme, /H 题要求 2/,
    'README 必须标明当前 26H 是要求2单圈巡线');
assert.match(readme, /KEY1.*启动.*单圈/,
    'README 必须说明 KEY1 启动单圈');
assert.match(readme, /KEY1\+KEY2 同时按下/,
    'README 必须记录物理组合急停');
assert.match(readme, /IR:CH1~CH6/,
    'README 必须记录 OLED 六路红外显示');

assert.match(main, /#include "Accomplish\/26H\.h"/,
    'main.c must select the 26H controller');
assert.match(main, /Accomplish26H_Init\(\);/,
    'main.c must initialize the 26H controller');
assert.match(main, /Accomplish26H_Update\(&updateContext\);/,
    'main.c must update 26H after a valid App update');
assert.doesNotMatch(main, /Accomplish\/25H\.h|Mission_Init\(|Mission_Update\(/,
    'the 26H entry must not launch the legacy 25H Mission');

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

assert.ok(line.includes('MotionLine_SetSpeed'),
    'MotionLine must support in-place speed changes without PID reset');
assert.ok(line.includes('MotionLine_RequestStop'),
    'MotionLine must support a zero-speed soft-stop request');
assert.ok(line.includes('MOTION_LINE_ACCELERATION_MMPS2'),
    'MotionLine must ramp cruise speed');
assert.ok(line.includes('MOTION_LINE_MAX_ADJUST_RATE_MMPS2'),
    'MotionLine must limit correction-rate steps');
assert.doesNotMatch(line, /Heading_GetYawRate|Application\/State\/Heading\.h/,
    'H2 line tracking must rely on infrared rather than the IMU');

assert.ok(param.includes('"h2off"'),
    'K parameter table must expose the H2 parking-offset calibration');

assert.match(display, /#include "Accomplish\/26H\.h"/,
    'OLED must read the 26H timer');
assert.match(display, /Accomplish26H_GetElapsedTicks\(\)/,
    'OLED must read accumulated integer ticks');
assert.ok(display.includes('"T:%lu.%02lus"'),
    'OLED first row must show seconds and centiseconds');
assert.ok(display.includes('"IR:"'),
    'OLED must render the six-channel infrared state');
assert.ok(display.includes('Accomplish26H_GetState()'),
    'OLED must expose a 26H task error');
assert.ok(displayHeader.includes('DebugDisplay_ShowHeadingCalibration'),
    'the display header must retain the MPU calibration page API');

console.log('26H ring, infrared display, and smooth-stop contract passed');
