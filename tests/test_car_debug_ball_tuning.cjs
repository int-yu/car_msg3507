const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');

const root = join(__dirname, '..');
const read = (path) => readFileSync(join(root, path), 'utf8');

const html = read('car_debug.html');
const main = read('main.c');
const app = read('Application/Core/App.c');
const telemetry = read('Application/Debug/Telemetry.c');
const telemetryHeader = read('Application/Debug/Telemetry.h');
const param = read('Application/Debug/Param.c');
const bluetooth = read('Application/Comms/BluetoothDebug.c');
const bluetoothHeader = read('Application/Comms/BluetoothDebug.h');
const readme = read('README.md');

for (const required of [
    '#define MAIN_BALL_START_SIGNAL 3U',
    '#define MAIN_BALL_STOP_SIGNAL  4U',
    'static uint8_t Main_StartBallTask(float targetMM)',
    'OK BALL START',
    'ERR BALL VISION',
    'ERR BALL CAR BUSY',
    'OK BALL STOP',
    'Telemetry_Update(updateContext.elapsedTicks, updateContext.pressedKeys);',
]) {
    assert.ok(main.includes(required), `main.c missing ${required}`);
}

assert.match(main,
    /Main_HasSignal\(&updateContext, MAIN_BALL_START_SIGNAL\)/,
    'C3 must start the selected-position ball hold');
assert.match(main,
    /Main_HasSignal\(&updateContext, MAIN_BALL_STOP_SIGNAL\)/,
    'C4 must stop and recenter the ball task');
assert.ok(main.includes('BluetoothDebug_TakeBallTargetMM('),
    'targeted X command must pass its requested position to the ball task');
assert.ok(main.includes('BallSequence_Start(targetMM)'),
    'ball task must start with the selected target position');
assert.ok(main.includes('BallSequence_SetTarget(s_ballTargetMM)'),
    'X must retarget an active requirement-3 task without restarting it');
assert.ok(main.includes('s_defaultBallHoldPending = 1U'),
    'the no-key startup state must wait to hold the ball at O');
assert.ok(main.includes('Main_BallHoldCanStart()'),
    'default O hold must wait for vision and stepper readiness');
assert.match(main,
    /s_defaultBallHoldPending[\s\S]*?Main_StartOrRetargetBallTask\(\s*BALL_SEQUENCE_DEFAULT_TARGET_MM\)/,
    'the default state must reuse BallSequence to hold O');
assert.doesNotMatch(main, /#include[^\n]*BallHold|BallHold_|MAIN_HOLD/,
    'current main flow must not restore the old BallHold task');
assert.doesNotMatch(app, /Telemetry_Update\(/,
    'telemetry sampling must happen after the requirement-3 update in main');

for (const required of [
    '#define TELEMETRY_CH_BPOS   0x4000U',
    '#define TELEMETRY_CH_BREF   0x8000U',
    '#define TELEMETRY_CH_SANG   0x10000UL',
    '#define TELEMETRY_CH_BVEL   0x20000UL',
    '#define TELEMETRY_CH_BVREF  0x40000UL',
    '#define TELEMETRY_CH_ALL    0x7FFFFUL',
]) {
    assert.ok(telemetryHeader.includes(required),
        `Telemetry.h missing ${required}`);
}
for (const required of [
    'Telemetry_ReadBallPosition',
    'Telemetry_ReadBallReference',
    'Telemetry_ReadStepperAngle',
    '{ TELEMETRY_CH_BPOS,  "bpos"',
    '{ TELEMETRY_CH_BREF,  "bref"',
    '{ TELEMETRY_CH_SANG,  "sang"',
    '{ TELEMETRY_CH_BVEL,  "bvel"',
    '{ TELEMETRY_CH_BVREF, "bvref"',
]) {
    assert.ok(telemetry.includes(required),
        `Telemetry.c missing ${required}`);
}

for (const required of [
    'id="ballSessionTools"',
    'id="ballTaskPanel"',
    'id="btnBallVideoRun"',
    'id="btnBallVideoStop"',
    'id="ballTargetMM"',
    'data-ball-field="position"',
    'data-ball-field="stepperAngle"',
    'data-ball-marker="position"',
    'bpos: 0x4000',
    'bref: 0x8000',
    'sang: 0x10000',
    'bvel: 0x20000',
    'bvref: 0x40000',
    'const CH_ALL = 0x7FFFF',
    "ampLabel: '目标位置 mm'",
    'mask: CH.bpos | CH.bref | CH.bvel | CH.bvref | CH.sang',
    "targetCommand: (target) => 'X' + Math.round(target)",
    "excite: () => 'C3'",
    "stopCmd: 'C4'",
    'function computeBallMetrics(',
    'function renderBallMonitor(',
    'function selectFocusLoop(',
    "$('sessAmp').value = $('ballTargetMM').value",
    'if (p.targetCommand)',
    'async function applyBallTarget()',
    "$('ballTargetMM').addEventListener('change', applyBallTarget)",
    '未启动任务',
]) {
    assert.ok(html.includes(required), `car_debug.html missing ${required}`);
}

for (const required of [
    "(value == 'X')",
    "case 'X':",
    'BluetoothDebug_TakeBallTargetMM',
]) {
    assert.ok(bluetooth.includes(required),
        `BluetoothDebug.c missing targeted ball command support: ${required}`);
}
assert.doesNotMatch(bluetooth,
    /case 'X':[^]*?BluetoothDebug_PublishSignal\(3U\)[^]*?break;/,
    'X must only store the ball target and must not publish a vehicle event');
assert.ok(bluetoothHeader.includes(
    'uint8_t BluetoothDebug_TakeBallTargetMM(float *targetMM);'),
    'BluetoothDebug.h must expose the one-shot ball target');

for (const name of [
    'bgr', 'bzo', 'bhl', 'bki', 'bkd', 'bkp', 'bvm',
    'k2kp', 'k2kd', 'k2ki',
]) {
    assert.ok(html.includes(`${name}:`),
        `ball parameter metadata missing ${name}`);
}
assert.match(html,
    /ball:\s*\{[^]*key2:[^]*\['k2kp', 'k2kd', 'k2ki'\]/,
    'ball tuning focus must expose the KEY2-only PID stage');
assert.match(param,
    /"bzo"[\s\S]*?STEPPER_MIN_ANGLE_DEG,\s*STEPPER_MAX_ANGLE_DEG/,
    'bzo tuning must use the same absolute range as the stepper');
for (const name of ['k2kp', 'k2kd', 'k2ki']) {
    assert.ok(param.includes(`{ "${name}"`),
        `K parameter table missing KEY2-only gain ${name}`);
}
assert.match(param,
    /\{ "k3dur"[^]*?\{ "k2kp"[^]*?\{ "k2kd"[^]*?\{ "k2ki"/,
    'KEY2-only gains must be appended after the existing K56 entry');

for (const required of [
    "if (p.startReply)",
    "if (command === 'C4') return (line) => line === 'OK BALL STOP'",
    "if (session.loop === 'ball' && !session.stopRequested)",
    "!Number.isFinite(sample.bref) || Math.abs(sample.bref) <= 0.5",
]) {
    assert.ok(html.includes(required), `ball session lifecycle missing ${required}`);
}

assert.match(html,
    /ball:\s*\{[^]*geometry:[^]*sensor:[^]*position:[^]*damping:[^]*integral:[^]*verify:/,
    'ball focus stages must follow the physical calibration order');
assert.match(readme, /`C3`[^\n]*要求 3[^\n]*KEY2/,
    'README must document C3 as the remote KEY2/requirement-3 start');
assert.match(readme, /`C4`[^\n]*回中/,
    'README must document C4 as the non-emergency recenter stop');

const metricsStart = html.indexOf('function computeBallMetrics(samples)');
const metricsEnd = html.indexOf('/* 按环路分派指标计算', metricsStart);
assert.ok(metricsStart >= 0 && metricsEnd > metricsStart,
    'must be able to extract requirement-3 metrics');
const computeBallMetrics = new Function(
    `${html.slice(metricsStart, metricsEnd)}\nreturn computeBallMetrics;`)();
const metrics = computeBallMetrics([
    { ms: 0, bref: 0, bpos: 0 },
    { ms: 100, bref: 0, bpos: 22 },
    { ms: 200, bref: 0, bpos: 14 },
    { ms: 300, bref: 0, bpos: 8 },
    { ms: 450, bref: 0, bpos: 4 },
    { ms: 600, bref: 0, bpos: 3 },
    { ms: 700, bref: 0, bpos: 2 },
    { ms: 900, bref: 0, bpos: 1 },
]);
assert.ok(metrics, 'requirement-3 metrics should be available');
assert.equal(metrics.targetMM, 0);
assert.equal(metrics.convergeMs, 300);
assert.equal(metrics.maxAbsErrorMM, 22);
assert.ok(metrics.steadyRmsMM <= 5,
    'steady RMS must describe target-hold error');
assert.equal(metrics.pass, true);

console.log('car_debug requirement-3 ball tuning contract passed');
