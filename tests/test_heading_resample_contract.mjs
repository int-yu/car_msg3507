import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const bluetooth = readFileSync(join(root, 'Application/Comms/BluetoothDebug.c'), 'utf8');
const web = readFileSync(join(root, 'car_debug.html'), 'utf8');
const readme = readFileSync(join(root, 'README.md'), 'utf8');

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

console.log('航向重新采样 Z2 契约测试\n');

check('固件 Z 命令区分 Z1 清零与 Z2 重新采样', () => {
    const zCase = bluetooth.match(/case 'Z':[\s\S]*?case 'W':/)?.[0] ?? '';
    ok(zCase.includes('value == 1'), 'Z 命令缺少 Z1 分支');
    ok(zCase.includes('value == 2'), 'Z 命令缺少 Z2 分支');
    ok(zCase.includes('Heading_SetYaw(0.0f)'), 'Z1 未清零当前航向');
    ok(zCase.includes('Heading_Calibrate()'), 'Z2 未调用静止零漂采样');
    ok(zCase.includes('OK Z=2'), 'Z2 成功后未返回确认');
});

check('Z2 会拒绝离线传感器和冲突中的尺度标定', () => {
    const zCase = bluetooth.match(/case 'Z':[\s\S]*?case 'W':/)?.[0] ?? '';
    ok(zCase.includes('Heading_IsReady()'), 'Z2 未检查 MPU6050 是否在线');
    ok(zCase.includes('ERR Z OFFLINE'), 'Z2 缺少离线错误');
    ok(zCase.includes('Heading_IsScaleCalibActive()'), 'Z2 未检查尺度标定冲突');
    ok(zCase.includes('ERR Z CALIBRATING'), 'Z2 缺少尺度标定冲突错误');
});

check('网页把 Z2 放在 Z1 旁并提示保持静止', () => {
    const z1At = web.indexOf('data-cmd="Z1"');
    const z2At = web.indexOf('data-cmd="Z2"');
    ok(z1At >= 0, '网页缺少 Z1 按钮');
    ok(z2At > z1At && z2At - z1At < 300, 'Z2 没有放在 Z1 旁边');
    ok(/Z2[\s\S]{0,200}静止/.test(web), 'Z2 旁缺少静止采样提示');
});

check('README 记录 Z1 与 Z2 的用途和采样时长', () => {
    ok(readme.includes('`Z2`'), 'README 未记录 Z2');
    ok(readme.includes('0.8 秒'), 'README 未说明 Z2 阻塞采样时长');
    ok(readme.includes('ERR Z OFFLINE'), 'README 未记录 Z2 离线错误');
});

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);
