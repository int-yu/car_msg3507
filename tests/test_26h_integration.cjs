const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');

const root = join(__dirname, '..');
const read = (path) => readFileSync(join(root, path), 'utf8');

const main = read('main.c');
const app = read('Application/Core/App.c');
const display = read('Application/Debug/DebugDisplay.c');
const displayHeader = read('Application/Debug/DebugDisplay.h');
const readme = read('README.md');

assert.match(readme, /26H 手动计时/,
    'README 必须标明当前 26H 手动计时入口');
assert.match(readme, /App_Init\(\).*继续初始化并校准 MPU6050/,
    'README 必须明确 MPU6050 继续初始化和校准');
assert.match(readme, /App_Update\(\).*继续更新 Heading/,
    'README 必须明确 Heading 继续周期更新');
assert.match(readme, /T:<秒>\.<百分秒>s/,
    'README 必须记录 OLED 首行计时格式');
assert.match(readme, /第一版不启动.*电机/,
    'README 必须说明 KEY1 不启动车辆');

assert.match(main, /#include "Accomplish\/26H\.h"/,
    'main.c must select the 26H controller');
assert.match(main, /Accomplish26H_Init\(\);/,
    'main.c must initialize the 26H controller');
assert.match(main, /Accomplish26H_Update\(&updateContext\);/,
    'main.c must update 26H after a valid App update');
assert.doesNotMatch(main, /Accomplish\/25H\.h|Mission_Init\(|Mission_Update\(/,
    'the initial 26H port must not launch the legacy 25H Mission');

for (const required of [
    '#include "Application/State/Heading.h"',
    'Heading_Init();',
    'DebugDisplay_ShowHeadingCalibration(Heading_IsReady());',
    'Heading_Calibrate();',
    'Heading_Update(context->dt);',
    'Stepper_Init();',
    'Stepper_Update(elapsedTicks);',
    'Stepper_EmergencyStop();',
]) {
    assert.ok(app.includes(required), `App must retain ${required}`);
}

assert.match(display, /#include "Accomplish\/26H\.h"/,
    'OLED must read the 26H timer');
assert.match(display, /Accomplish26H_GetElapsedTicks\(\)/,
    'OLED must read accumulated integer ticks');
assert.ok(display.includes('"T:%lu.%02lus"'),
    'OLED first row must show seconds and centiseconds');
assert.doesNotMatch(display,
    /OLED_ShowString\(0,\s*0,\s*"Z:"/,
    'the normal OLED page must no longer display the Z angle on row 0');
assert.ok(display.includes('#include "Application/State/Heading.h"'),
    'the display module must retain Heading for the MPU calibration page');
assert.ok(display.includes('void DebugDisplay_ShowHeadingCalibration'),
    'the display module must retain the MPU calibration page implementation');
assert.ok(displayHeader.includes('DebugDisplay_ShowHeadingCalibration'),
    'the display header must retain the MPU calibration page API');

console.log('26H entry, MPU retention, and timer display contract passed');
