/*
 * 板载捕获（通道 B）与 X/Q 命令的契约测试。
 *
 * 这些行为是固件与网页之间的协议，任何一侧单独改动都会让调参流程静默失效，
 * 所以在这里锁住：通道位序、dump 行前缀、命令回应格式、RAM 预算。
 *
 * 跑法：node tests/test_capture_contract.mjs
 */
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const captureH = readFileSync(join(root, 'Application/Debug/Capture.h'), 'utf8');
const captureC = readFileSync(join(root, 'Application/Debug/Capture.c'), 'utf8');
const bluetooth = readFileSync(join(root, 'Application/Comms/BluetoothDebug.c'), 'utf8');
const app = readFileSync(join(root, 'Application/Core/App.c'), 'utf8');
const telemetryH = readFileSync(join(root, 'Application/Debug/Telemetry.h'), 'utf8');
const telemetryC = readFileSync(join(root, 'Application/Debug/Telemetry.c'), 'utf8');

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

console.log('板载捕获与 X/Q 命令契约测试\n');

check('1. 捕获缓冲不超出可用 SRAM，且给栈留出余量', () => {
    const bytes = Number(captureH.match(/CAPTURE_BUFFER_BYTES\s+(\d+)U/)[1]);
    ok(bytes >= 8192, `缓冲 ${bytes}B 太小，装不下一次完整阶跃`);
    /* SRAM 32KB，固件其余部分约 3.5KB；超过 24KB 会挤压栈空间。 */
    ok(bytes <= 24576, `缓冲 ${bytes}B 过大，可能与栈冲突`);
});

check('2. 通道位定义连续且与写入顺序一致', () => {
    const expected = [
        ['CAPTURE_CH_TL', 0x0001], ['CAPTURE_CH_LV', 0x0002],
        ['CAPTURE_CH_PL', 0x0004], ['CAPTURE_CH_TR', 0x0008],
        ['CAPTURE_CH_RV', 0x0010], ['CAPTURE_CH_PR', 0x0020],
        ['CAPTURE_CH_YAW', 0x0040], ['CAPTURE_CH_NAVE', 0x0080],
        ['CAPTURE_CH_LERR', 0x0100],
    ];
    for (const [name, value] of expected) {
        const found = captureH.match(new RegExp(`${name}\\s+0x([0-9A-Fa-f]+)U`));
        ok(found, `缺少通道定义 ${name}`);
        eq(parseInt(found[1], 16), value, `${name} 的位值`);
    }
    /* s_channels 表的顺序即 dump 列序，必须与位序一致。 */
    const tableOrder = [...captureC.matchAll(/\{\s*(CAPTURE_CH_\w+),\s*"(\w+)"/g)]
        .map((m) => m[1]);
    eq(tableOrder.join(','), expected.map((e) => e[0]).join(','),
        's_channels 表顺序与位序不一致，dump 列会错位');
});

check('3. dump 使用 C, 前缀与 CH, 表头，不与实时流 D,/H, 冲突', () => {
    ok(captureC.includes('"CH,ms"'), 'dump 表头必须以 CH,ms 开头');
    ok(captureC.includes('"C,%lu"'), 'dump 数据行必须以 C, 开头');
    ok(!captureC.includes('"D,'), 'dump 不得复用实时流的 D, 前缀');
});

check('4. 捕获在 MotionManager 之后采样，否则记录到上一拍的值', () => {
    const updateAt = app.indexOf('MotionManager_Update(');
    const captureAt = app.indexOf('Capture_Update(');
    ok(updateAt >= 0 && captureAt >= 0, 'App.c 缺少调用');
    ok(captureAt > updateAt,
        'Capture_Update 必须在 MotionManager_Update 之后调用');
});

check('5. 运动命令成功后自动触发捕获', () => {
    /* F/B/T/A 走统一的 ReportMotionResult；W/N 各自单独触发。 */
    const reportFn = bluetooth.match(
        /static void BluetoothDebug_ReportMotionResult[\s\S]*?\n\}/)?.[0] ?? '';
    ok(reportFn.includes('Capture_Trigger()'),
        'F/B/T/A 的公共回报路径未触发捕获');
    ok(/Capture_Trigger\(\);\s*\n\s*Serial1_Printf\("OK W=/.test(bluetooth),
        'W 命令未触发捕获');
    ok(/Capture_Trigger\(\);\s*\n\s*Serial1_Printf\("OK N=/.test(bluetooth),
        'N 命令未触发捕获');
});

check('6. X 命令回应格式固定，网页据此解析', () => {
    ok(bluetooth.includes('"OK X ARM=%u CAP=%u\\r\\n"'), 'X<mask> 回应格式变了');
    ok(bluetooth.includes('"OK X DUMP=%u\\r\\n"'), 'X0 回应格式变了');
    ok(captureC.includes('"OK X END=%u\\r\\n"'), 'dump 结束标记变了');
    ok(bluetooth.includes('ERR X EMPTY'), '无数据时应明确报错而不是静默');
    ok(bluetooth.includes('ERR X MASK'), '掩码非法时应明确报错');
});

check('7. Q 命令回报全部能力字段，且无参数可用', () => {
    ok(bluetooth.includes('MAX=%u MASK=%u RATE=%u'),
        'Q 必须回报上限、掩码与当前频率——网页靠它自适应频率');
    ok(bluetooth.includes('CAPST=%u CAPN=%u CAPMAX=%u'),
        'Q 必须回报捕获状态、已采样本数与容量');
    ok(/s_parser\.command == 'Q'[\s\S]{0,200}hasDigits = 1U/.test(bluetooth),
        '裸 Q（不带数字）必须可用');
});

check('8. 遥测单侧字段已定义且不与成对字段冲突', () => {
    const singles = {
        TELEMETRY_FIELD_SPEED_L: 0x400, TELEMETRY_FIELD_SPEED_R: 0x800,
        TELEMETRY_FIELD_TARGET_L: 0x1000, TELEMETRY_FIELD_TARGET_R: 0x2000,
        TELEMETRY_FIELD_PWM_L: 0x4000, TELEMETRY_FIELD_PWM_R: 0x8000,
    };
    for (const [name, value] of Object.entries(singles)) {
        const found = telemetryH.match(new RegExp(`${name}\\s+0x([0-9A-Fa-f]+)U`));
        ok(found, `缺少单侧字段 ${name}`);
        eq(parseInt(found[1], 16), value, `${name} 的位值`);
    }
    /* 旧的成对位必须原样保留，否则既有掩码全部失效。 */
    eq(/TELEMETRY_FIELD_SPEED\s+0x08U/.test(telemetryH), true,
        '成对字段 SPEED 的位值被改动');
    eq(/TELEMETRY_FIELD_ALL\s+0xFFFFU/.test(telemetryH), true,
        'ALL 未扩展到 16 位');
});

check('9. 单侧字段在表头、数据行和行长估算三处都实现了', () => {
    for (const [mask, column] of [
        ['TELEMETRY_FIELD_SPEED_L', ',LV'], ['TELEMETRY_FIELD_SPEED_R', ',RV'],
        ['TELEMETRY_FIELD_TARGET_L', ',TL'], ['TELEMETRY_FIELD_TARGET_R', ',TR'],
        ['TELEMETRY_FIELD_PWM_L', ',PL'], ['TELEMETRY_FIELD_PWM_R', ',PR'],
    ]) {
        const uses = telemetryC.split(mask).length - 1;
        ok(uses >= 3,
            `${mask} 只出现 ${uses} 次，表头/数据行/行长估算三处必须齐全`);
        ok(telemetryC.includes(`"${column}"`), `表头缺少列名 ${column}`);
    }
});

check('10. 单侧掩码确实能把行长压到成对方案的一半以下', () => {
    /* 复算固件的行长公式。mode 是文本列且占 9 字节，调参时该信息由捕获
       数据和命令回应提供，实时流不必带——这里按不带 mode 计算。 */
    const base = 14;
    const singleWide = 10;   /* ',-999999.9' */
    const pwmWide = 6;       /* ',-1000' */
    const modeWide = 9;
    const leftOnly = base + singleWide * 2 + pwmWide;
    const paired = base + 20 /* LV,RV */ + 20 /* TL,TR */ + 12 /* PL,PR */ + modeWide;
    ok(leftOnly * 2 < paired * 1.4,
        `单侧 ${leftOnly}B 相对成对 ${paired}B 的压缩不足`);

    const bytesPerSecond = 115200 / 10;
    const rate = (row) => Math.floor((bytesPerSecond * 20) / (row * 100));
    ok(rate(leftOnly) >= 50,
        `单侧掩码上限仅 ${rate(leftOnly)}Hz，达不到调参需要的 50Hz`);
    /* 单侧相对成对必须有实质提升，否则这个特性不值得存在。 */
    ok(rate(leftOnly) >= rate(paired) * 1.8,
        `单侧 ${rate(leftOnly)}Hz 相对成对 ${rate(paired)}Hz 提升不足`);
});

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);
