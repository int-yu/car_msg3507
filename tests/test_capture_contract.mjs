/*
 * 板载捕获（二进制）与 X/Q 命令的固件侧契约测试。
 *
 * 二进制架构后捕获与遥测共用通道定义（TELEMETRY_CH_*）和帧编码（TelemFrame），
 * dump 走 CAP_META/CAP_SAMPLE/CAP_END 二进制帧。这里锁住固件侧的结构约束，
 * 帧字节级行为由 test_binary_frame.mjs 覆盖。
 *
 * 跑法：node tests/test_capture_contract.mjs
 */
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const captureH = readFileSync(join(root, 'Application/Debug/Capture.h'), 'utf8');
const captureC = readFileSync(join(root, 'Application/Debug/Capture.c'), 'utf8');
const telemetryH = readFileSync(join(root, 'Application/Debug/Telemetry.h'), 'utf8');
const telemetryC = readFileSync(join(root, 'Application/Debug/Telemetry.c'), 'utf8');
const frameH = readFileSync(join(root, 'Application/Debug/TelemFrame.h'), 'utf8');
const bluetooth = readFileSync(join(root, 'Application/Comms/BluetoothDebug.c'), 'utf8');
const app = readFileSync(join(root, 'Application/Core/App.c'), 'utf8');

let passed = 0;
let failed = 0;

function check(name, fn) {
    try {
        fn();
        passed += 1;
        console.log(`  PASS  ${name}`);
    } catch (error) {
        failed += 1;
        console.log(`  FAIL  ${name}`);
        console.log(`        ${error.message}`);
    }
}

function ok(value, message) {
    if (!value) throw new Error(message);
}

function eq(actual, expected, what) {
    if (actual !== expected) {
        throw new Error(`${what}: 期望 ${JSON.stringify(expected)}，实际 ${JSON.stringify(actual)}`);
    }
}

console.log('板载捕获二进制契约测试\n');

check('1. 捕获缓冲不超出可用 SRAM，且够录一段', () => {
    const bytes = Number(captureH.match(/CAPTURE_BUFFER_BYTES\s+(\d+)U/)[1]);
    ok(bytes >= 8192, `缓冲 ${bytes}B 太小`);
    /* SRAM 32KB，固件其余约 3.5KB；超过 28KB 会挤压栈。 */
    ok(bytes <= 28672, `缓冲 ${bytes}B 过大，可能与栈冲突`);
});

check('2. 遥测 12 通道位定义连续且与固件一致', () => {
    const expected = [
        ['TELEMETRY_CH_TL', 0x0001], ['TELEMETRY_CH_LV', 0x0002],
        ['TELEMETRY_CH_PL', 0x0004], ['TELEMETRY_CH_TR', 0x0008],
        ['TELEMETRY_CH_RV', 0x0010], ['TELEMETRY_CH_PR', 0x0020],
        ['TELEMETRY_CH_YAW', 0x0040], ['TELEMETRY_CH_NAVE', 0x0080],
        ['TELEMETRY_CH_LERR', 0x0100], ['TELEMETRY_CH_GRAY', 0x0200],
        ['TELEMETRY_CH_LD', 0x0400], ['TELEMETRY_CH_RD', 0x0800],
    ];
    for (const [name, value] of expected) {
        const found = telemetryH.match(new RegExp(`${name}\\s+0x([0-9A-Fa-f]+)U`));
        ok(found, `缺少通道定义 ${name}`);
        eq(parseInt(found[1], 16), value, `${name} 的位值`);
    }
    eq(/TELEMETRY_CH_ALL\s+0x0FFFU/.test(telemetryH), true, 'ALL 应为 0x0FFF');
});

check('3. 捕获与遥测共用同一套通道，不再各定义一份', () => {
    /* 捕获头文件不应再有独立的 CAPTURE_CH_* 定义。 */
    ok(!/#define\s+CAPTURE_CH_TL/.test(captureH),
        '捕获不应再独立定义 CAPTURE_CH_*，应复用 TELEMETRY_CH_*');
    /* 捕获取值应复用 Telemetry_SampleChannels，保证列语义一致。 */
    ok(captureC.includes('Telemetry_SampleChannels'),
        '捕获应复用 Telemetry_SampleChannels 取值');
});

check('4. dump 走二进制帧 CAP_META/CAP_SAMPLE/CAP_END', () => {
    ok(captureC.includes('TELEM_FRAME_TYPE_CAP_META'), 'dump 缺 CAP_META');
    ok(captureC.includes('TELEM_FRAME_TYPE_CAP_SAMPLE'), 'dump 缺 CAP_SAMPLE');
    ok(captureC.includes('TELEM_FRAME_TYPE_CAP_END'), 'dump 缺 CAP_END');
    ok(captureC.includes('TelemFrame_Build'), 'dump 应用 TelemFrame_Build 组帧');
    /* 不应再有旧的 ASCII dump 前缀。 */
    ok(!captureC.includes('"CH,ms"') && !captureC.includes('"C,%lu"'),
        '不应残留 ASCII dump 格式');
});

check('5. 帧类型定义齐全且与协议一致', () => {
    const types = {
        TELEM_FRAME_TYPE_SCHEMA: 0x30, TELEM_FRAME_TYPE_SAMPLE: 0x31,
        TELEM_FRAME_TYPE_CAP_META: 0x32, TELEM_FRAME_TYPE_CAP_SAMPLE: 0x33,
        TELEM_FRAME_TYPE_CAP_END: 0x34,
    };
    for (const [name, value] of Object.entries(types)) {
        const found = frameH.match(new RegExp(`${name}\\s+0x([0-9A-Fa-f]+)U`));
        ok(found, `缺少帧类型 ${name}`);
        eq(parseInt(found[1], 16), value, `${name} 的值`);
    }
    /* 0xAA 帧头是二进制与 ASCII 共存的关键。 */
    ok(/TELEM_FRAME_MAGIC_0\s+0xAAU/.test(frameH), '帧头魔术字节应为 0xAA');
});

check('6. 捕获在 MotionManager 之后采样', () => {
    const updateAt = app.indexOf('MotionManager_Update(');
    const captureAt = app.indexOf('Capture_Update(');
    ok(updateAt >= 0 && captureAt >= 0, 'App.c 缺少调用');
    ok(captureAt > updateAt,
        'Capture_Update 必须在 MotionManager_Update 之后');
});

check('7. 运动命令成功后自动触发捕获', () => {
    const reportFn = bluetooth.match(
        /static void BluetoothDebug_ReportMotionResult[\s\S]*?\n\}/)?.[0] ?? '';
    ok(reportFn.includes('Capture_Trigger()'),
        'F/B/T/A 的公共回报路径未触发捕获');
    ok(/Capture_Trigger\(\);\s*\n\s*Serial1_Printf\("OK W=/.test(bluetooth),
        'W 命令未触发捕获');
    ok(/Capture_Trigger\(\);\s*\n\s*Serial1_Printf\("OK N=/.test(bluetooth),
        'N 命令未触发捕获');
});

check('8. X/Q 命令回应格式固定，网页据此解析', () => {
    ok(bluetooth.includes('"OK X ARM=%u CAP=%u\\r\\n"'), 'X<mask> 回应格式变了');
    ok(bluetooth.includes('"OK X DUMP=%u\\r\\n"'), 'X0 回应格式变了');
    ok(bluetooth.includes('ERR X EMPTY'), '无数据时应明确报错');
    ok(bluetooth.includes('MAX=%u MASK=%u RATE=%u'),
        'Q 必须回报上限、掩码与当前频率——网页靠它自适应频率');
});

check('9. 捕获通道上限放宽（可变长度带来更长录制）', () => {
    const maxCh = Number(captureH.match(/CAPTURE_MAX_CHANNELS\s+(\d+)U/)[1]);
    ok(maxCh >= 6, `捕获上限 ${maxCh} 通道，二进制后应放宽以支持更多通道`);
    /* 单通道 8B/样本，24KB 可录约 30 秒——可变长度的核心收益。 */
    const bufBytes = Number(captureH.match(/CAPTURE_BUFFER_BYTES\s+(\d+)U/)[1]);
    const singleChSeconds = Math.floor(bufBytes / 8) / 100;
    ok(singleChSeconds >= 25,
        `单通道可录 ${singleChSeconds.toFixed(0)}s，应达到 25s 以上`);
});

check('10. 遥测发二进制帧而非 ASCII', () => {
    ok(telemetryC.includes('TELEM_FRAME_TYPE_SAMPLE'), '遥测应发 SAMPLE 帧');
    ok(telemetryC.includes('Telemetry_SendSchema'), '遥测应发 SCHEMA 帧');
    /* 不应再有旧的 ASCII 表头/数据行。 */
    ok(!telemetryC.includes('"H,ms"') && !telemetryC.includes('"D,%lu"'),
        '不应残留 ASCII 遥测格式');
});

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);
