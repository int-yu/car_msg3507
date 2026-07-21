/*
 * 二进制遥测帧契约测试。
 *
 * 固件（TelemFrame.c/Telemetry.c/Capture.c）与网页（car_debug.html 的字节流
 * 解析器）之间是二进制协议。任一侧单独改都会让调参链路静默失效，所以这里：
 *   1. 用 JS 独立复刻固件的编码规则（CRC8、小端、帧布局），组出帧字节；
 *   2. 喂给从 car_debug.html 抽取的真实解析器，验证解出的样本逐字段正确；
 *   3. 覆盖最易错的边界：PAYLOAD 内含 0x0A(\n)/0xAA 字节、CRC 错误、SEQ 跳变、
 *      二进制帧与 ASCII 命令回应混流。
 *
 * 跑法：node tests/test_binary_frame.mjs
 */
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const htmlPath = process.env.CAR_DEBUG_HTML || join(here, '..', 'car_debug.html');
const html = readFileSync(htmlPath, 'utf8');
const begin = html.indexOf('/* ==PARSER_BEGIN== */');
const end = html.indexOf('/* ==PARSER_END== */');
if (begin < 0 || end < 0) {
    throw new Error('car_debug.html 中找不到 PARSER_BEGIN/PARSER_END 标记');
}
const parserSource = html.slice(begin, end);
const { TelemetryParser } = new Function(
    parserSource + '\nreturn { TelemetryParser };')();

let passed = 0;
let failed = 0;

function check(name, fn) {
    try {
        fn();
        passed += 1;
        console.log(`  PASS  ${name}`);
    } catch (err) {
        failed += 1;
        console.log(`  FAIL  ${name}`);
        console.log(`        ${err.message}`);
    }
}

function eq(actual, expected, what) {
    if (actual !== expected) {
        throw new Error(`${what}: 期望 ${JSON.stringify(expected)}，实际 ${JSON.stringify(actual)}`);
    }
}

function approx(actual, expected, what, tol = 1e-3) {
    if (Math.abs(actual - expected) > tol) {
        throw new Error(`${what}: 期望约 ${expected}，实际 ${actual}`);
    }
}

/* ---- 独立复刻固件编码规则（与 TelemFrame.c 一致，但不共享代码，形成对拍）---- */

const MAGIC0 = 0xAA;
const MAGIC1 = 0x55;
const VER = 0x01;
const T_SCHEMA = 0x30;
const T_SAMPLE = 0x31;
const T_CAP_META = 0x32;
const T_CAP_SAMPLE = 0x33;
const T_CAP_END = 0x34;
const UNIT = { raw: 0, mmps: 1, pwm: 2, deg: 3, mm: 4, bits: 5 };

function refCrc8(bytes) {
    let crc = 0;
    for (const b of bytes) {
        crc ^= b;
        for (let i = 0; i < 8; i += 1) {
            crc = (crc & 0x80) ? (((crc << 1) ^ 0x07) & 0xFF) : ((crc << 1) & 0xFF);
        }
    }
    return crc;
}

/* 组一帧：magic + ver + type + seq + len + payload + crc（crc 覆盖 ver..payload）。 */
function buildFrame(type, seq, payload) {
    const head = [VER, type, seq & 0xFF, payload.length];
    const crc = refCrc8([...head, ...payload]);
    return Uint8Array.from([MAGIC0, MAGIC1, ...head, ...payload, crc]);
}

function u16le(v) { return [v & 0xFF, (v >> 8) & 0xFF]; }
function u32le(v) {
    return [v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF];
}
function f32le(v) {
    const buf = new ArrayBuffer(4);
    new DataView(buf).setFloat32(0, v, true);
    return [...new Uint8Array(buf)];
}

/* SCHEMA payload：mask(u16) + 每通道 { nameLen(u8), name[], unit(u8) }。 */
function schemaPayload(mask, channels) {
    const p = [...u16le(mask)];
    for (const [name, unit] of channels) {
        p.push(name.length);
        for (const ch of name) {
            p.push(ch.charCodeAt(0));
        }
        p.push(UNIT[unit]);
    }
    return p;
}

/* SAMPLE payload：ms(u32) + 各通道 float32。 */
function samplePayload(ms, values) {
    const p = [...u32le(ms)];
    for (const v of values) {
        p.push(...f32le(v));
    }
    return p;
}

console.log('二进制遥测帧契约测试\n');

const WHEEL_CHANNELS = [['TL', 'mmps'], ['LV', 'mmps'], ['PL', 'pwm']];

check('1. schema + sample：解出的样本逐字段正确', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x07, WHEEL_CHANNELS)));
    eq(p.columns.join(','), 'ms,TL,LV,PL', 'schema 列名');
    p.feed(buildFrame(T_SAMPLE, 1, samplePayload(100, [300, 285.5, 612])));
    eq(p.samples.length, 1, 'sample 数');
    eq(p.samples[0].ms, 100, 'ms');
    approx(p.samples[0].TL, 300, 'TL');
    approx(p.samples[0].LV, 285.5, 'LV');
    approx(p.samples[0].PL, 612, 'PL');
});

check('2. 单位码进 unitOf：同量纲共享刻度组', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x07,
        [['TL', 'mmps'], ['LV', 'mmps'], ['yaw', 'deg']])));
    eq(p.unitOf('TL'), 'mmps', 'TL 量纲');
    eq(p.unitOf('LV'), 'mmps', 'LV 与 TL 同组');
    eq(p.unitOf('yaw'), 'deg', 'yaw 量纲');
    /* raw/bits 各自独立（组名即列名）。 */
    p.feed(buildFrame(T_SCHEMA, 1, schemaPayload(0x03,
        [['lerr', 'raw'], ['gray', 'bits']])));
    eq(p.unitOf('lerr'), 'lerr', 'raw 独立');
    eq(p.unitOf('gray'), 'gray', 'bits 独立');
});

check('3. 新 schema 到达清空旧样本（掩码切换）', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x07, WHEEL_CHANNELS)));
    p.feed(buildFrame(T_SAMPLE, 1, samplePayload(10, [1, 2, 3])));
    eq(p.samples.length, 1, '切换前');
    p.feed(buildFrame(T_SCHEMA, 2, schemaPayload(0x40, [['yaw', 'deg']])));
    eq(p.samples.length, 0, '切换后清空');
    eq(p.columns.join(','), 'ms,yaw', '新列名');
    p.feed(buildFrame(T_SAMPLE, 3, samplePayload(20, [45.5])));
    eq(p.samples[0].yaw, 45.5, '新列取值');
    eq(p.samples[0].TL, undefined, '旧列不残留');
});

check('4. PAYLOAD 内含 0x0A(\\n) 字节不被误当换行', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x01, [['TL', 'mmps']])));
    /* ms=10 的低字节是 0x0A（换行符）。若解析器按 \n 分段，这里会崩。 */
    eq(u32le(10)[0], 0x0A, '前提：10 的低字节确实是 0x0A');
    p.feed(buildFrame(T_SAMPLE, 1, samplePayload(10, [256])));
    eq(p.samples.length, 1, '含 0x0A 的帧应正常解析');
    eq(p.samples[0].ms, 10, 'ms 正确');
    approx(p.samples[0].TL, 256, 'TL 正确');
});

check('5. PAYLOAD 内含 0xAA 字节不被误当帧头', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x01, [['TL', 'mmps']])));
    /* ms=0xAA=170，payload 里会出现 0xAA。严格按 LEN 读满就不会误判。 */
    p.feed(buildFrame(T_SAMPLE, 1, samplePayload(0xAA, [128])));
    eq(p.samples.length, 1, '含 0xAA 的帧应正常解析');
    eq(p.samples[0].ms, 0xAA, 'ms 正确');
});

check('6. 帧跨 feed 边界（半帧）先缓冲再拼接', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x01, [['TL', 'mmps']])));
    const frame = buildFrame(T_SAMPLE, 1, samplePayload(50, [99]));
    p.feed(frame.slice(0, 5));    /* 只喂前 5 字节 */
    eq(p.samples.length, 0, '半帧不产生样本');
    p.feed(frame.slice(5));       /* 喂剩余 */
    eq(p.samples.length, 1, '拼接后成帧');
    approx(p.samples[0].TL, 99, 'TL');
});

check('7. CRC 错误的帧整帧丢弃并计数', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x01, [['TL', 'mmps']])));
    const frame = buildFrame(T_SAMPLE, 1, samplePayload(10, [1]));
    frame[frame.length - 1] ^= 0xFF;   /* 篡改 CRC */
    p.feed(frame);
    eq(p.samples.length, 0, 'CRC 错帧不进样本');
    eq(p.droppedFrames > 0, true, '应计入丢帧');
});

check('8. sample 长度与列数不符整帧丢弃', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x07, WHEEL_CHANNELS)));
    /* schema 声明 3 通道，但只给 2 个 float。 */
    p.feed(buildFrame(T_SAMPLE, 1, samplePayload(10, [1, 2])));
    eq(p.samples.length, 0, '长度不符应丢弃');
});

check('9. 二进制帧与 ASCII 命令回应混流：各归各', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x01, [['TL', 'mmps']])));
    /* 中间插一条 ASCII 回应，再来一帧样本。 */
    const enc = (s) => Uint8Array.from([...s].map((c) => c.charCodeAt(0)));
    const r1 = p.feed(enc('OK K1=1.5000\r\n'));
    eq(r1.textLines.includes('OK K1=1.5000'), true, 'ASCII 回应进 textLines');
    const r2 = p.feed(buildFrame(T_SAMPLE, 1, samplePayload(10, [42])));
    eq(r2.samplesAdded, 1, '样本帧进 samples');
    approx(p.samples[0].TL, 42, 'TL');
});

check('10. 捕获帧 CAP_META/SAMPLE/END 独立于实时流', () => {
    const p = new TelemetryParser();
    /* 先有实时流。 */
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x01, [['TL', 'mmps']])));
    p.feed(buildFrame(T_SAMPLE, 1, samplePayload(10, [5])));
    eq(p.samples.length, 1, '实时流样本');

    /* 捕获 dump：CAP_META 带 schema（用 CAP_META 类型），CAP_SAMPLE，CAP_END。 */
    const capChannels = [['LV', 'mmps'], ['yaw', 'deg']];
    const meta = p.feed(buildFrame(T_CAP_META, 2, schemaPayload(0x42, capChannels)));
    eq(meta.captureStarted, true, 'CAP_META 报告捕获开始');
    eq(p.captureColumns.join(','), 'ms,LV,yaw', '捕获列名');
    eq(p.samples.length, 1, '实时流样本不受影响');

    p.feed(buildFrame(T_CAP_SAMPLE, 3, samplePayload(0, [100, 1.5])));
    p.feed(buildFrame(T_CAP_SAMPLE, 4, samplePayload(10, [102, 1.6])));
    eq(p.captureSamples.length, 2, '捕获样本数');
    approx(p.captureSamples[1].LV, 102, '捕获 LV');
    approx(p.captureSamples[1].yaw, 1.6, '捕获 yaw');

    const end = p.feed(buildFrame(T_CAP_END, 5, u16le(2)));
    eq(end.captureEnded, true, 'CAP_END 报告结束');
});

check('11. SEQ 跳变计入丢帧（检测 Web Serial 丢包）', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x01, [['TL', 'mmps']])));
    p.feed(buildFrame(T_SAMPLE, 1, samplePayload(10, [1])));
    const before = p.droppedFrames;
    /* 跳过 seq=2，直接来 seq=5。 */
    p.feed(buildFrame(T_SAMPLE, 5, samplePayload(20, [2])));
    eq(p.droppedFrames > before, true, 'SEQ 跳变应计丢帧');
    eq(p.samples.length, 2, '样本本身仍正常收下');
});

check('12. 连续 0xAA 0xAA 后接 0x55 仍能识别帧头', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x01, [['TL', 'mmps']])));
    /* 噪声 0xAA + 正常帧（帧本身以 0xAA 0x55 开头）。 */
    p.feed(Uint8Array.from([0xAA]));
    p.feed(buildFrame(T_SAMPLE, 1, samplePayload(10, [7])));
    eq(p.samples.length, 1, '前导 0xAA 噪声不影响后续帧');
    approx(p.samples[0].TL, 7, 'TL');
});

check('13. gray 通道（bits 单位）按 float 存，值可还原为整数', () => {
    const p = new TelemetryParser();
    p.feed(buildFrame(T_SCHEMA, 0, schemaPayload(0x200, [['gray', 'bits']])));
    p.feed(buildFrame(T_SAMPLE, 1, samplePayload(10, [31])));  /* 0x1F */
    eq(p.samples[0].gray, 31, 'gray 位图值');
});

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);
