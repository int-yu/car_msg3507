const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');

const root = join(__dirname, '..');
const html = readFileSync(join(root, 'car_debug.html'), 'utf8');
const params = readFileSync(join(root, 'Application/Debug/Param.c'), 'utf8');

assert.ok(html.includes("hold: {"),
    'web page must keep the requirement 4 operating session');
assert.ok(html.includes("excite: () => 'C5'") && html.includes("stopCmd: 'C6'"),
    'web page must operate requirement 4 with C5/C6');
assert.ok(!html.includes("'h4ff'") && !html.includes("'h4to'"),
    'web page must not expose requirement 4 tuning parameters');
assert.ok(!params.includes('{ "h4ff"') && !params.includes('{ "h4to"'),
    'firmware K table must not expose requirement 4 tuning parameters');

for (const name of [
    'lra', 'lki', 'lkd', 'h2cru', 'h2fin', 'h2clr', 'h2lap', 'h2app',
    'h2arm', 'h2max', 'h2off', 'h2lkp', 'h2lki', 'h2lkd',
    'h2lac', 'h2lde', 'h2rr', 'lacc', 'ldec',
]) {
    assert.ok(params.includes(`{ "${name}"`),
        `固件参数表必须保留 ${name}`);
}

for (const required of [
    'id="lineSessionTools"',
    'data-line-channel="0"',
    'data-line-channel="5"',
    'id="lineLiveError"',
    'id="lineLiveBase"',
    'id="lineLiveTargets"',
    'id="lineLiveSpeeds"',
    'id="lineLiveYawRate"',
    'function describeLineDetection(gray)',
    'function estimateYawRateDps(samples)',
    'function renderLineMonitor()',
    'lostPct:',
    'wheelMaeMMps:',
    "line: ['h2lkp', 'h2rr'], integral: ['h2lki']",
    'lra: 1, lki: 0.1, lkd: 0.5',
    'CH.yaw | CH.lerr | CH.gray | CH.TL | CH.TR | CH.LV | CH.RV',
    "start: ['h2cru', 'h2clr', 'h2lac']",
    "position: ['h2lap', 'h2arm', 'h2max']",
    "finish: ['h2app', 'h2fin', 'h2off', 'h2lde']",
    '修改后下一次 KEY1 启动生效',
    'id="lineRunConfigSummary"',
    'function renderLineRunConfigSummary()',
    'async function prepareTelemetryStream(mask, desiredRate)',
    "await sendAndWait('G0'",
    'waitForTelemetrySample(sampleSerial)',
    'preparedRate = await prepareTelemetryStream(p.mask, p.rate)',
    'async function refreshParameters()',
    "await sendAndWait('G0'",
    "/^OK K COUNT=\\d+$/",
]) {
    assert.ok(html.includes(required), `巡线调参面板缺少 ${required}`);
}

assert.ok(html.includes('PID(h2lkp,h2lki,h2lkd)'),
    '要求2结构说明必须包含独立 PID');
assert.ok(html.includes('不因压线或弯道额外降速'),
    '巡线面板必须说明不再区分弯道或按压线降速');
assert.ok(!html.includes("'lcv'") && !html.includes("'lch'") &&
          !html.includes('curveSpeed'),
    '巡线面板不能继续暴露弯道低速参数');
assert.ok(html.includes('全 0 连续 1600 ms 判丢线'),
    '巡线面板必须说明当前固件的丢线确认时长');
assert.ok(!html.includes('五路灰度离散权重(-6..+6)'),
    '巡线面板不能继续使用旧五路灰度说明');

const lineSensors = [...html.matchAll(/data-line-channel="(\d)"/g)]
    .map((match) => Number(match[1]));
assert.deepEqual(lineSensors, [0, 1, 2, 3, 4, 5],
    '巡线面板必须按 CH1~CH6 显示六个通道');

const stateStart = html.indexOf('function describeLineDetection(gray)');
const stateEnd = html.indexOf('function estimateYawRateDps(samples)', stateStart);
assert.ok(stateStart >= 0 && stateEnd > stateStart,
    '必须能提取巡线实时状态判定函数');
const describeLineDetection = new Function(
    `${html.slice(stateStart, stateEnd)}\nreturn describeLineDetection;`)();

assert.equal(describeLineDetection(null), '—');
assert.match(describeLineDetection(0x00), /未检测/);
assert.equal(describeLineDetection(0x02), '已检测');
assert.equal(describeLineDetection(0x10), '已检测');
assert.equal(describeLineDetection(0x25), '多路 / 横线');
assert.equal(describeLineDetection(0x3F), '多路 / 横线');

const yawRateStart = html.indexOf('function estimateYawRateDps(samples)');
const yawRateEnd = html.indexOf('function renderLineMonitor()', yawRateStart);
assert.ok(yawRateStart >= 0 && yawRateEnd > yawRateStart,
    '必须能提取巡线角速度显示函数');
const estimateYawRateDps = new Function(
    `${html.slice(yawRateStart, yawRateEnd)}\nreturn estimateYawRateDps;`)();
assert.equal(estimateYawRateDps([{ ms: 1000, yaw: 20 }, { ms: 1020, yaw: 21 }]), 50);
assert.equal(estimateYawRateDps([{ ms: 1000, yaw: 20 }]), null);

const metricsStart = html.indexOf('function computeLineMetrics(samples)');
const metricsEnd = html.indexOf('/* 按环路分派指标计算', metricsStart);
assert.ok(metricsStart >= 0 && metricsEnd > metricsStart,
    '必须能提取巡线指标计算函数');
const computeLineMetrics = new Function(
    `${html.slice(metricsStart, metricsEnd)}\nreturn computeLineMetrics;`)();
const metrics = computeLineMetrics([
    { lerr: -2, gray: 0x01, TL: 100, LV: 90, TR: 120, RV: 130 },
    { lerr: 0, gray: 0x00, TL: 110, LV: 100, TR: 110, RV: 100 },
    { lerr: 6, gray: 0x20, TL: 140, LV: 130, TR: 80, RV: 100 },
]);
const near = (actual, expected) =>
    assert.ok(Math.abs(actual - expected) < 1e-9,
        `expected ${actual} to be near ${expected}`);
near(metrics.rms, Math.sqrt(40 / 3));
near(metrics.maxAbs, 6);
near(metrics.extremePct, 100 / 3);
near(metrics.lostPct, 100 / 3);
near(metrics.wheelMaeMMps, 70 / 6);

console.log('car_debug 六路红外巡线调参面板契约通过');
