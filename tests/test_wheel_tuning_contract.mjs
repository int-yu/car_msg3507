import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const wheelH = readFileSync(join(root, 'Application/Control/MotionWheel.h'), 'utf8');
const wheelC = readFileSync(join(root, 'Application/Control/MotionWheel.c'), 'utf8');
const paramC = readFileSync(join(root, 'Application/Debug/Param.c'), 'utf8');
const web = readFileSync(join(root, 'car_debug.html'), 'utf8');

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

console.log('左右轮独立调参契约测试\n');

check('MotionWheel 导出左右独立 PI、前馈与静摩擦参数', () => {
    for (const name of [
        'MotionWheel_TuneLeftKp', 'MotionWheel_TuneLeftKi',
        'MotionWheel_TuneLeftIntegralLimit',
        'MotionWheel_TuneRightKp', 'MotionWheel_TuneRightKi',
        'MotionWheel_TuneRightIntegralLimit',
        'MotionWheel_TuneLeftFeedforwardPWMPerMMps',
        'MotionWheel_TuneRightFeedforwardPWMPerMMps',
        'MotionWheel_TuneLeftStaticFrictionPWM',
        'MotionWheel_TuneRightStaticFrictionPWM',
    ]) {
        ok(wheelH.includes(`extern float ${name};`), `头文件缺少 ${name}`);
        ok(wheelC.includes(`float ${name} =`), `源文件缺少 ${name} 默认值`);
    }
});

check('左右 PID 初始化和运行时应用不再共用同一组增益', () => {
    ok(wheelC.includes('MotionWheel_TuneLeftKp, MotionWheel_TuneLeftKi'), '左 PID 未使用左轮增益');
    ok(wheelC.includes('MotionWheel_TuneRightKp, MotionWheel_TuneRightKi'), '右 PID 未使用右轮增益');
    ok(wheelC.includes('s_leftPID.integralMax = MotionWheel_TuneLeftIntegralLimit'), '左积分限幅未独立');
    ok(wheelC.includes('s_rightPID.integralMax = MotionWheel_TuneRightIntegralLimit'), '右积分限幅未独立');
});

check('左右轮使用各自的速度→PWM 前馈映射', () => {
    ok(wheelC.includes('MotionWheel_TuneLeftFeedforwardPWMPerMMps'), '缺左前馈变量');
    ok(wheelC.includes('MotionWheel_TuneRightFeedforwardPWMPerMMps'), '缺右前馈变量');
    ok(/MotionWheel_GetFeedforward\([\s\S]*MotionWheel_TuneLeftFeedforwardPWMPerMMps/.test(wheelC), '左输出未使用左前馈');
    ok(/MotionWheel_GetFeedforward\([\s\S]*MotionWheel_TuneRightFeedforwardPWMPerMMps/.test(wheelC), '右输出未使用右前馈');
});

check('Param 保留旧双轮参数并在表尾追加十个独立参数', () => {
    for (const legacy of ['wkp', 'wki', 'wil', 'wff', 'wsf']) {
        ok(paramC.includes(`{ "${legacy}"`), `旧参数 ${legacy} 被删除`);
    }
    const names = ['lwkp', 'lwki', 'lwil', 'lwff', 'lwsf', 'rwkp', 'rwki', 'rwil', 'rwff', 'rwsf'];
    let previous = paramC.indexOf('{ "cpm"');
    for (const name of names) {
        const at = paramC.indexOf(`{ "${name}"`);
        ok(at > previous, `${name} 没有按兼容要求追加在表尾`);
        previous = at;
    }
});

check('网页轮速试验使用左右独立参数并提供映射预估', () => {
    for (const name of ['lwkp', 'lwki', 'lwff', 'lwsf', 'rwkp', 'rwki', 'rwff', 'rwsf']) {
        ok(web.includes(`${name}:`), `PARAM_LABELS 缺少 ${name}`);
    }
    ok(web.includes("params: ['lwkp', 'lwki', 'lwil', 'lwff', 'lwsf',"), '轮速试验未快照左轮参数');
    ok(web.includes("'rwkp', 'rwki', 'rwil', 'rwff', 'rwsf']"), '轮速试验未快照右轮参数');
    ok(web.includes('function estimateWheelBasePwm'), '缺少速度→基础PWM计算函数');
    ok(web.includes('id="wheelMap"'), '参数面板缺少PWM映射提示区域');
});

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);

