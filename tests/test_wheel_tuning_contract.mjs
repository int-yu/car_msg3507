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

check('已测安全参数分别固化为左右默认值', () => {
    const expected = [
        ['MOTION_WHEEL_LEFT_KP', '0.6f'],
        ['MOTION_WHEEL_LEFT_KI', '0.0f'],
        ['MOTION_WHEEL_LEFT_INTEGRAL_LIMIT', '0.0f'],
        ['MOTION_WHEEL_LEFT_FEEDFORWARD_PWM_PER_MMPS', '0.43114f'],
        ['MOTION_WHEEL_LEFT_STATIC_FRICTION_PWM', '19.884f'],
        ['MOTION_WHEEL_RIGHT_KP', '0.6f'],
        ['MOTION_WHEEL_RIGHT_KI', '0.0f'],
        ['MOTION_WHEEL_RIGHT_INTEGRAL_LIMIT', '0.0f'],
        ['MOTION_WHEEL_RIGHT_FEEDFORWARD_PWM_PER_MMPS', '0.44289f'],
        ['MOTION_WHEEL_RIGHT_STATIC_FRICTION_PWM', '22.205f'],
    ];
    for (const [name, value] of expected) {
        ok(new RegExp(`#define\\s+${name}\\s+${value.replace('.', '\\.')}`).test(wheelH),
            `${name} 未固化为 ${value}`);
    }
});

check('网页包含长样本精测、左轮反转提醒和绘图降载', () => {
    ok(web.includes('data-wheel-speed="600"'), '缺少 600 mm/s 快捷目标');
    ok(web.includes('左轮硬件当前不能反转'), '缺少左轮反转硬件提醒');
    ok(web.includes('function downsampleForPlot'), '缺少绘图抽样降载');
    ok(web.includes('durDefault: 8'), '轮速精测试验默认时长不足 8 秒');
});

check('调参工作台按任务、对象和阶段聚焦，不再默认堆出全部参数', () => {
    for (const id of [
        'focusTaskNav', 'focusObjectNav', 'focusStageNav',
        'focusPath', 'focusStageHelp', 'sessParams', 'advancedParams',
    ]) {
        ok(web.includes(`id="${id}"`), `缺少聚焦工作台节点 ${id}`);
    }
    ok(web.includes('const TUNING_FOCUS ='), '缺少任务/对象/阶段配置');
    ok(web.includes("let focusState = { loop: 'wheel', side: 'left', stage: 'p' }"),
        '默认焦点不是轮速→左轮→P');
    ok(web.includes("feedforward: ['lwff', 'lwsf']"), '左轮前馈阶段参数不完整');
    ok(web.includes("p: ['lwkp']"), '左轮 P 阶段没有只保留 lwkp');
    ok(web.includes("i: ['lwki', 'lwil']"), '左轮 I 阶段参数不完整');
    ok(web.includes('function focusedParamNames'), '缺少聚焦参数选择函数');
    ok(web.includes('function focusedSeriesNames'), '缺少曲线自动选择函数');
    ok(web.includes('function renderFocusNavigation'), '缺少聚焦导航渲染');
});

check('完整 K 参数仍然保留，但默认收进高级工具', () => {
    ok(/<details[^>]+id="advancedParams"[^>]*>[\s\S]*id="paramTable"/.test(web),
        '完整参数表未放入高级折叠区');
    ok(web.includes('全部 K 参数'), '高级入口标题不明确');
});

check('阶段切换会直接准备参数卡、遥测字段和推荐曲线，同时保留手动选择能力', () => {
    ok(/id="focusStageHelp"[\s\S]*id="sessParams"[\s\S]*<\/div>\s*<\/div>/.test(web),
        '阶段参数卡没有放在聚焦区域内部');
    ok(web.includes('function applyFocusedTelemetrySelection'), '缺少阶段遥测字段自动选择');
    ok(web.includes('function syncMaskCheckboxes'), '缺少遥测复选框同步');
    ok(web.includes('let pendingFocusedSeriesSelection = true'), '缺少推荐曲线一次性自动选择状态');
    ok(web.includes('id="btnFocusSeries"'), '缺少恢复当前阶段曲线按钮');
    ok(web.includes('连接后读取'), '离线状态没有显示阶段参数占位卡');
});

check('自动遥测切换串行执行，连接与断开会同步当前聚焦状态', () => {
    ok(web.includes('let focusedTelemetryApplyChain = Promise.resolve()'),
        '自动遥测切换没有串行队列，快速点击可能交错发送');
    ok(web.includes('let focusedTelemetryGeneration = 0'),
        '自动遥测切换缺少连接代标记，断开时无法作废在途请求');
    ok(/readTask = readLoop\(\);[\s\S]{0,300}await applyFocusedTelemetrySelection\(true\)/.test(web),
        '连接成功后没有下发当前聚焦任务的遥测预设');
    const disconnectSource = web.slice(
        web.indexOf('async function disconnect()'),
        web.indexOf('async function send(cmd)'));
    ok(disconnectSource.includes('paramCache = {}'), '断开后没有清除失效的参数缓存');
    ok(disconnectSource.includes('scheduleParamRender()'), '断开后没有恢复离线参数占位卡');
    ok(disconnectSource.includes('focusedTelemetryGeneration += 1'),
        '断开入口没有立即作废在途自动遥测请求');
    const telemetrySource = web.slice(
        web.indexOf('async function applyFocusedTelemetrySelection'),
        web.indexOf('function runMatchesFocus'));
    ok(telemetrySource.includes('generation !== focusedTelemetryGeneration'),
        '自动遥测队列没有在每步发送前检查连接代');
});

check('聚焦参数卡不在浏览器端强制夹值，曲线允许用户手动清空', () => {
    const paramSource = web.slice(
        web.indexOf('function renderSessionParams()'),
        web.indexOf('function startSession()'));
    ok(!paramSource.includes('const clamped ='), '聚焦参数卡新增了浏览器端参数夹值限制');
    const seriesSource = web.slice(
        web.indexOf('function rebuildSeries()'),
        web.indexOf('function drawPlot()'));
    ok(!seriesSource.includes('selected.add(cols[0])'), '用户手动清空曲线后仍会被自动勾回');
});

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);
