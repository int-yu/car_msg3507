const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');

const root = join(__dirname, '..');
const read = (path) => readFileSync(join(root, path), 'utf8');

const main = read('main.c');
const app = read('Application/Core/App.c');
const task = read('Accomplish/26H.c');
const taskHeader = read('Accomplish/26H.h');
const timedLine = read('Application/Control/TimedLineRun.c');
const timedLineHeader = read('Application/Control/TimedLineRun.h');
const ballTargetCapture = read('Application/Control/BallTargetCapture.c');
const ballTargetCaptureHeader = read('Application/Control/BallTargetCapture.h');
const line = read('Application/Control/MotionLine.c');
const lineHeader = read('Application/Control/MotionLine.h');
const requirement2Line = read('Application/Control/MotionLineRequirement2.c');
const requirement2LineHeader = read('Application/Control/MotionLineRequirement2.h');
const timer = read('Application/Control/TaskTimer.c');
const timerHeader = read('Application/Control/TaskTimer.h');
const makeDefs = read('makefile.defs');
const param = read('Application/Debug/Param.c');
const display = read('Application/Debug/DebugDisplay.c');
const displayHeader = read('Application/Debug/DebugDisplay.h');
const stepperHeader = read('Hardware/Motor/Stepper.h');
const grayHeader = read('Hardware/Sensors/Graydetect.h');
const readme = read('README.md');

assert.match(readme, /要求 2～6/,
    'README must document the requirement selection menu');
assert.match(readme, /KEY1.*循环选择/,
    'README must document that KEY1 cycles requirements');
assert.match(readme, /KEY1\+KEY2 同时按下/,
    'README 必须记录物理组合急停');
assert.match(readme, /计时精度为 0\.1 秒/,
    'README must document OLED timer precision');

assert.doesNotMatch(main, /MAIN_STEPPER_TEST_MODE|Main26H_Run\(\)/,
    'main.c must not keep the old switchable wrapper entry');
assert.match(main, /#include "Accomplish\/26H\.h"/,
    'main.c must select the 26H controller directly');
assert.match(main, /Accomplish26H_Init\(\);/,
    'main.c must initialize the 26H controller');
assert.match(main, /Accomplish26H_Update\(&lineContext\);/,
    'main.c must update 26H after a valid App update');
assert.doesNotMatch(main, /Accomplish\/25H\.h|Mission_Init\(|Mission_Update\(/,
    'the active entry must not launch the legacy 25H Mission');
assert.match(main, /MAIN_SELECT_KEY_MASK\s+0x01U/,
    'KEY1 must select requirements in the idle menu');
assert.match(main, /MAIN_CONFIRM_KEY_MASK\s+0x02U/,
    'KEY2 must confirm the selected requirement');
assert.match(main, /MAIN_RETURN_KEY_MASK\s+0x04U/,
    'KEY3 must return every task to the idle menu');
assert.match(main, /Main_ReturnToMenu\([\s\S]*?Accomplish26H_Cancel\(\)[\s\S]*?TimedLineRun_Cancel\(\)[\s\S]*?BALL_SEQUENCE_DEFAULT_TARGET_MM/,
    'KEY3 return must cancel line tasks and restore O-point control');
assert.match(main, /TimedLineRun_Init\(\);/,
    'main must initialize the KEY3 timed line controller');
assert.match(main, /MAIN_REQUIREMENT_4_SPEED_MMPS\s+500\.0f/,
    'requirement 4 must default to 500 mm/s');
assert.match(main, /MAIN_REQUIREMENT_4_TIME_S\s+13\.0f/,
    'requirement 4 must default to 13 seconds');
assert.match(main, /MAIN_REQUIREMENT_5_SPEED_MMPS\s+400\.0f/,
    'requirements 5 and 6 must default to 400 mm/s');
assert.match(main, /MAIN_REQUIREMENT_5_TIME_S\s+30\.0f/,
    'requirements 5 and 6 must default to 30 seconds');
assert.match(main, /TimedLineRun_Update\(&updateContext\);/,
    'main must update the KEY3 timed line controller every valid tick');
assert.match(main, /MAIN_EMERGENCY_CHORD_MASK\s+\(0x01U \| 0x02U\)/,
    'main.c must protect KEY1+KEY2 as the emergency chord');
assert.match(main, /BallSequence_Init\(\);/,
    'main.c must initialize the ball task library');
assert.match(main, /BallSequence_Update\(updateContext\.dt\);/,
    'main.c must update the ball task after 26H');
assert.match(main,
    /Main_HasSignal\(&updateContext,\s*MAIN_BALL_START_SIGNAL\)/,
    'C3 must still start the selected-position ball hold');
assert.match(main,
    /Main_HasSignal\(&updateContext,\s*MAIN_BALL_STOP_SIGNAL\)/,
    'C4 must still stop the ball task');
assert.doesNotMatch(main, /#include[^\n]*BallHold|BallHold_|MAIN_HOLD/,
    'active main flow must not restore the old KEY3 BallHold flow');
assert.match(main, /s_defaultBallHoldPending\s*=\s*1U/,
    'the no-key startup state must default to O-point ball control');
assert.match(main, /Main_BallHoldCanStart\(\)/,
    'default O hold must wait for existing readiness checks');
assert.match(main,
    /s_defaultBallHoldPending[\s\S]*?Main_StartOrRetargetBallTask\(\s*BALL_SEQUENCE_DEFAULT_TARGET_MM\)/,
    'the pending no-key state must invoke the existing O-point BallSequence path');
assert.match(main, /Telemetry_Update\(updateContext\.elapsedTicks,[\s\S]*?updateContext\.pressedKeys\);/,
    'telemetry sampling must happen after the task controllers in main');
assert.match(main, /BeamActuator_Update\(updateContext\.dt\);/,
    'main.c must apply the latest ball-task target to the stepper actuator');
assert.match(main, /DebugDisplay_Update\(\);/,
    'main.c must keep the minimal OLED time display refresh');
assert.match(main, /Main_StartOrRetargetBallTask\([\s\S]*?BALL_SEQUENCE_DEFAULT_TARGET_MM/,
    'KEY1 must start or retarget ball hold to 0 mm');
assert.match(main, /Main_StartBallSweep\(\)/,
    'KEY2 must start the -50 mm to +50 mm ball sweep');
assert.match(main, /BallTargetCapture_Start\(\)/,
    'the first KEY4 press must start multi-frame target capture');
assert.match(main, /BallTargetCapture_GetTargetMM\(\)/,
    'KEY4 must hold the captured arbitrary ball target');
assert.doesNotMatch(main, /ERR KEY4 BALL NOT STABLE|BallSequence_IsStable\(\)/,
    'KEY4 line start must not wait for the captured ball to settle');
assert.match(main,
    /Main_StartTimedLine\(BallTargetCapture_GetTargetMM\(\)/,
    'requirement 6 must run at its captured target');
assert.match(ballTargetCaptureHeader,
    /BALL_TARGET_CAPTURE_CONFIRM_FRAMES\s+8U/,
    'KEY4 target confirmation frame count must be easy to adjust');
assert.match(ballTargetCaptureHeader,
    /BALL_TARGET_CAPTURE_STABILITY_TOLERANCE_MM\s+5\.0f/,
    'KEY4 target capture stability tolerance must be explicit');
assert.match(ballTargetCapture, /BallSensor_GetFrameSequence\(\)/,
    'KEY4 capture must count independent camera frames, not 100 Hz repeats');
assert.doesNotMatch(main, /ERR KEY4 BALL LOST/,
    'KEY4 ball-control failure must not add a chassis-stop coupling');

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
    'MotionManager_StartRequirement2Line(',
    'MotionManager_SetRequirement2LineSpeed(',
    'MotionManager_RequestRequirement2LineStop(',
    'MotionManager_StartBrake(',
    'Graydetect_IsOnline()',
    'ACCOMPLISH_26H_MARKER_ADJACENT_CHANNELS',
    'ACCOMPLISH_26H_STATE_SETTLING',
    'ACCOMPLISH_26H_ERROR_TIME_LIMIT',
]) {
    assert.ok(task.includes(required), `26H must retain ${required}`);
}
assert.match(taskHeader, /ACCOMPLISH_26H_MARKER_ADJACENT_CHANNELS\s+3U/,
    '26H finish marker must require three adjacent infrared channels');
assert.doesNotMatch(task, /travelledDistanceMM\s*>=\s*s_runParameters\.maxLapDistanceMM/,
    '26H must not stop merely because a maximum distance was reached');
assert.match(taskHeader, /ACCOMPLISH_26H_FINISH_MARKER_MIN_DISTANCE_MM\s+5000\.0f/,
    '26H finish marker must be armed only after 5000 mm');
assert.match(taskHeader, /ACCOMPLISH_26H_FINISH_ROLLOUT_MM/,
    '26H header must expose the physical stop-offset calibration');
assert.match(taskHeader, /ACCOMPLISH_26H_MARKER_CONFIRM_TICKS\s+2U/,
    '26H header must require two independent marker samples');
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
assert.doesNotMatch(task, /KEY3|TimedLineRun/,
    'Accomplish26H must remain the KEY1 total-task controller');
assert.match(timedLineHeader, /TIMED_LINE_RUN_ACCELERATION_MMPS2\s+100\.0f/,
    'KEY3 default acceleration must be 100 mm/s2');
assert.match(timedLineHeader, /TIMED_LINE_RUN_CRUISE_SPEED_MMPS\s+400\.0f/,
    'KEY3 default cruise speed must be 400 mm/s');
assert.match(timedLineHeader, /TIMED_LINE_RUN_DURATION_SECONDS\s+30\.0f/,
    'KEY3 timed run must default to 30 seconds');
assert.ok(timedLine.includes('TimedLineRun_TuneAccelerationMMps2') &&
          timedLine.includes('TimedLineRun_TuneCruiseSpeedMMps'),
    'KEY3 acceleration and cruise speed must remain field-tunable');
assert.doesNotMatch(timedLine,
    /FinishMarker|MARKER|NominalLap|SetLineSpeed|StartBrake/,
    'KEY3 controller must not contain finish slowdown, marker, or distance-stop logic');
assert.ok(timer.includes('TaskTimer_Start') && timer.includes('TaskTimer_Stop'),
    'task timer must expose reusable start and stop operations');
assert.ok(timerHeader.includes('TASK_TIMER_OWNER_LINE') &&
          timerHeader.includes('TASK_TIMER_OWNER_BALL'),
    'task timer must distinguish line and ball owners');
assert.ok(makeDefs.includes('Application/Control/TaskTimer.c'),
    'CCS generated builds must include the task timer module');
assert.ok(makeDefs.includes('Application/Control/TimedLineRun.c'),
    'CCS generated builds must include the KEY3 timed line module');
assert.ok(makeDefs.includes('Application/Control/BallTargetCapture.c'),
    'CCS generated builds must include the KEY4 target capture module');
assert.ok(makeDefs.includes('Application/Control/MotionLineRequirement2.c'),
    'CCS builds must include the independent requirement-2 line module');
assert.match(requirement2LineHeader,
    /MOTION_LINE_REQUIREMENT2_LOST_CONFIRM_TICKS\s+160U/,
    'requirement 2 line-loss delay must be twenty times the old 8 ticks');
assert.match(requirement2LineHeader,
    /MOTION_LINE_REQUIREMENT2_RIGHT_TURN_RIGHT_ADJUST_RATIO\s+2\.0f/,
    'requirement 2 right-wheel reduction ratio must default to 2.0');
assert.match(requirement2Line,
    /speedAdjustMMps \* s_context\.rightTurnRightAdjustRatio/,
    'requirement 2 clockwise turns must scale right-wheel reduction');
assert.doesNotMatch(timedLine, /Requirement2|h2rr|H2Line/,
    'requirements 4-6 must not depend on requirement-2 control settings');

assert.ok(line.includes('MotionLine_SetSpeed'),
    'MotionLine must support in-place speed changes without PID reset');
assert.match(lineHeader, /MOTION_LINE_LOST_CONFIRM_TICKS\s+160U/,
    'general line-loss delay must be twenty times the old 8 ticks');
assert.ok(line.includes('MotionLine_RequestStop'),
    'MotionLine must support a zero-speed soft-stop request');
assert.ok(line.includes('MOTION_LINE_ACCELERATION_MMPS2'),
    'MotionLine must ramp cruise speed');
assert.ok(line.includes('PID_Update'),
    'MotionLine must use one PID controller for line correction');
assert.ok(line.includes('MotionLine_SnapshotTunings()'),
    'MotionLine must snapshot acceleration/deceleration at Start');
assert.doesNotMatch(line,
    /MOTION_LINE_MAX_ADJUST_RATE_MMPS2|MOTION_LINE_CURVE|curveEntryDistanceMM|MotionLine_TuneCurve/,
    'MotionLine must not keep the old curve speed or correction-rate limits');
assert.match(line, /MotionLine_TuneKpMMpsPerWeight[\s\S]*MotionLine_TuneKiMMpsPerWeight[\s\S]*MotionLine_TuneKdMMpsPerWeight/,
    'MotionLine must expose one PID parameter set');

assert.ok(param.includes('"h2off"'),
    'K parameter table must expose the H2 parking-offset calibration');
for (const name of [
    'h2cru', 'h2fin', 'h2clr', 'h2lap', 'h2app', 'h2arm', 'h2max',
    'h2lkp', 'h2lki', 'h2lkd', 'h2lac', 'h2lde', 'h2rr',
    'lacc', 'ldec', 'lra', 'lki', 'lkd',
    'k3acc', 'k3cru', 'k3dur',
    'k2kp', 'k2kd', 'k2ki', 'k2pkp', 'k2pkd', 'k2pki',
    'k2vm', 'k2ad', 'k2tv', 'k2mt', 'k2il',
    'k2pvm', 'k2pad', 'k2ptv', 'k2pmt', 'k2pil',
]) {
    assert.ok(param.includes(`{ "${name}"`),
        `K parameter table must append ${name}`);
}
assert.match(param, /"lcra"[\s\S]*?Param_GetLineCurveKp/,
    'K46 must remain a compatible alias of the unified line Kp');
assert.match(param, /"lckd"[\s\S]*?Param_GetLineCurveKd/,
    'K47 must remain a compatible alias of the unified line Kd');
assert.ok(param.indexOf('{ "bvm"') < param.indexOf('{ "lki"'),
    'bvm must retain its published K50 slot before the appended line Ki');
assert.match(param,
    /\{ "k3dur"[^]*?\{ "k2kp"[^]*?\{ "k2kd"[^]*?\{ "k2ki"[^]*?\{ "k2pkp"[^]*?\{ "k2pkd"[^]*?\{ "k2pki"[^]*?\{ "k2vm"[^]*?\{ "k2ad"[^]*?\{ "k2tv"[^]*?\{ "k2mt"[^]*?\{ "k2il"[^]*?\{ "k2pvm"[^]*?\{ "k2pad"[^]*?\{ "k2ptv"[^]*?\{ "k2pmt"[^]*?\{ "k2pil"/,
    'both KEY2 PID and motion-profile sets must be appended after K56');
assert.match(param,
    /\{ "k2pil"[^]*?\{ "h2lkp"[^]*?\{ "h2lki"[^]*?\{ "h2lkd"[^]*?\{ "h2lac"[^]*?\{ "h2lde"[^]*?\{ "h2rr"/,
    'requirement-2 line settings must be appended after all published IDs');
assert.doesNotMatch(task + read('Application/Control/MotionManager.h') +
    read('Application/Control/MotionManager.c'), /SetLineDeceleration/,
    'H2 must keep the existing MotionLine control path without a runtime deceleration setter');

assert.doesNotMatch(display, /BallSensor_/,
    'OLED must not read the ball sensor');
assert.ok(display.includes('s_elapsedTenths'),
    'OLED must display elapsed time to tenths of a second');
assert.ok(display.includes('OLED_UpdateArea('),
    'OLED refresh must be limited to the time-value area');
assert.match(display, /DEBUG_DISPLAY_TICKS_PER_TENTH\s+10U/,
    'OLED running timer must update at 0.1-second precision');
assert.match(display, /DebugDisplay_ShowMenu\([\s\S]*?REQUIRE /,
    'OLED must expose the requirement selection menu');
assert.match(display, /OLED_8X16/,
    'OLED running time must use the larger built-in font');
assert.doesNotMatch(display, /IR:|KEY:|encoderCounts|BALL:/,
    'OLED must not display unrelated diagnostics');
assert.doesNotMatch(displayHeader, /ShowHeadingCalibration/,
    'OLED must not expose the removed MPU calibration page');

assert.match(stepperHeader, /STEPPER_INITIAL_ANGLE_DEG\s+201\.0f/,
    'stepper horizontal angle must be 201 degrees');
assert.match(stepperHeader, /STEPPER_AUTO_START_ENABLED\s+0U/,
    'stepper must not move before the manual power-on pose is captured');
assert.ok(main.includes('BeamActuator_IsZeroCalibrated()'),
    'default ball hold must wait for automatic horizontal-zero capture');
assert.match(stepperHeader, /STEPPER_MIN_ANGLE_DEG\s+0\.0f/,
    'stepper minimum limit must be 0 degrees');
assert.match(stepperHeader, /STEPPER_MAX_ANGLE_DEG\s+360\.0f/,
    'stepper maximum limit must be 360 degrees');
assert.match(grayHeader, /GRAYDETECT_ENABLED\s+1U/,
    'complete 26H mode must enable the six-channel infrared sensor');

console.log('26H ring, time display, and brake-stop contract passed');
