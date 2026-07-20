/*
 * 遥测 CSV 解析器单元测试。
 *
 * car_debug.html 必须是单文件无外部依赖，所以解析器代码内联在页面里。
 * 为了让测试跑的是真代码而不是副本（副本会随时间漂移），这里直接从
 * car_debug.html 中抽取 PARSER_BEGIN/PARSER_END 标记之间的源码执行。
 *
 * 跑法：node tests/test_csv_parse.mjs
 */
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
/* 允许指定别的文件，便于用变异版本反证这些测试确实能失败。 */
const htmlPath = process.env.CAR_DEBUG_HTML || join(here, '..', 'car_debug.html');

const html = readFileSync(htmlPath, 'utf8');
const begin = html.indexOf('/* ==PARSER_BEGIN== */');
const end = html.indexOf('/* ==PARSER_END== */');
if (begin < 0 || end < 0) {
    throw new Error('car_debug.html 中找不到 PARSER_BEGIN/PARSER_END 标记');
}
const parserSource = html.slice(begin, end);

/* 抽出来的源码里只有定义，末尾补一句把要测的东西交出来。 */
const {
    TelemetryParser, samplesToCsv, buildAiPack, estimateWheelBasePwm,
    computeStepMetrics, computeTurnMetrics, analyzeRun, suggestFeedforward,
    downsampleForPlot, computeWheelSteadyStats,
    parseCapabilityReply, estimateRowBytes, estimateMaxRateHz,
    countCaptureChannels,
} = new Function(
    parserSource + '\nreturn { TelemetryParser, samplesToCsv, buildAiPack,' +
    ' estimateWheelBasePwm, computeStepMetrics, computeTurnMetrics,' +
    ' analyzeRun, suggestFeedforward, downsampleForPlot,' +
    ' computeWheelSteadyStats, parseCapabilityReply, estimateRowBytes,' +
    ' estimateMaxRateHz, countCaptureChannels };'
)();

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

const FULL_HEADER = 'H,ms,yaw,gray,keys,LD,RD,LV,RV,mode,k230\r\n';
const FULL_ROW = 'D,12340,-91.25,1F,0,1203.4,1198.7,0.0,0.0,IDLE,1:-123:-45\r\n';

console.log('遥测 CSV 解析器测试\n');

check('1. 正常表头+数据行，各列取值正确', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed(FULL_ROW);
    eq(p.samples.length, 1, 'samples 条数');
    const s = p.samples[0];
    eq(s.ms, 12340, 'ms');
    eq(s.yaw, -91.25, 'yaw');
    eq(s.keys, 0, 'keys');
    eq(s.LD, 1203.4, 'LD');
    eq(s.RD, 1198.7, 'RD');
    eq(s.LV, 0, 'LV');
    eq(s.RV, 0, 'RV');
    eq(s.mode, 'IDLE', 'mode');
});

check('2. 数据行列数少于表头 → 丢弃', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed('D,12340,-91.25,1F\r\n');
    eq(p.samples.length, 0, 'samples 条数');
});

check('3. 数据行列数多于表头 → 丢弃', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed('D,12340,-91.25,1F,0,1203.4,1198.7,0.0,0.0,IDLE,1:-123:-45,999\r\n');
    eq(p.samples.length, 0, 'samples 条数');
});

check('4. 中途收到新表头（掩码切换）→ 清空旧 samples 并按新表头解析', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed(FULL_ROW);
    eq(p.samples.length, 1, '切换前 samples 条数');

    p.feed('H,ms,yaw\r\n');
    eq(p.samples.length, 0, '切换后应清空');

    p.feed('D,500,12.50\r\n');
    eq(p.samples.length, 1, '新表头下 samples 条数');
    eq(p.samples[0].yaw, 12.5, '新表头下 yaw');
    eq(p.samples[0].LD, undefined, '旧列不应残留');
});

check('5. OK/ERR 行不进 samples，只进文本行', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    const out = p.feed('OK G=20\r\nERR RANGE MAX=23\r\n');
    eq(p.samples.length, 0, 'samples 条数');
    eq(out.textLines.length, 2, '文本行条数');
    eq(out.textLines[0], 'OK G=20', '第一条文本行');
    eq(out.textLines[1], 'ERR RANGE MAX=23', '第二条文本行');
});

check('6. 半截行先缓冲，拼接完整后才解析', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed('D,12340,-91.25,1F,0,1203.4,');
    eq(p.samples.length, 0, '半截行不应产生 sample');
    p.feed('1198.7,0.0,0.0,IDLE,1:-123:-45\r\n');
    eq(p.samples.length, 1, '拼接后应产生 sample');
    eq(p.samples[0].RD, 1198.7, '拼接后 RD');
});

check('7. \\r\\n 与 \\n 两种换行都能拆', () => {
    const p = new TelemetryParser();
    p.feed('H,ms,yaw\n');
    p.feed('D,100,1.00\nD,200,2.00\r\n');
    eq(p.samples.length, 2, 'samples 条数');
    eq(p.samples[0].yaw, 1, '第一条 yaw');
    eq(p.samples[1].yaw, 2, '第二条 yaw');
});

check('8. gray 列按十六进制解析（1F → 31，不是 1）', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed(FULL_ROW);
    eq(p.samples[0].gray, 31, 'gray');
});

check('9. k230 列展开为三个虚拟列', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed(FULL_ROW);
    const s = p.samples[0];
    eq(s.k230_valid, 1, 'k230_valid');
    eq(s.k230_x, -123, 'k230_x');
    eq(s.k230_y, -45, 'k230_y');
    eq(s.k230, undefined, '原始复合列不应保留');
});

check('10. k230 为 0:0:0 时也能正确展开', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed('D,1,0.00,00,0,0.0,0.0,0.0,0.0,IDLE,0:0:0\r\n');
    const s = p.samples[0];
    eq(s.k230_valid, 0, 'k230_valid');
    eq(s.k230_x, 0, 'k230_x');
    eq(s.k230_y, 0, 'k230_y');
});

/* 以下两条不在需求清单里，但同属解析器最容易出错的地方，一并锁住。 */

check('11. k230 分段数不为 3 的畸形行 → 整行丢弃', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed('D,12340,-91.25,1F,0,1203.4,1198.7,0.0,0.0,IDLE,1:-123\r\n');
    eq(p.samples.length, 0, 'samples 条数');
});

check('12. 曲线选择器只列数值列：排除文本列 mode 和横轴列 ms', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    const numeric = p.numericColumns();
    eq(numeric.includes('mode'), false, 'mode 是文本，不应可画图');
    eq(numeric.includes('ms'), false, 'ms 是横轴，画出来只是单调直线');
    eq(numeric.includes('k230'), false, '复合原始列不应可画图');
    eq(numeric.includes('k230_x'), true, 'k230_x 应可画图');
    eq(numeric.includes('yaw'), true, 'yaw 应可画图');
    eq(numeric.includes('gray'), true, 'gray 应可画图');
});

check('13. gray 为非法十六进制 → 整行丢弃', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed('D,12340,-91.25,GG,0,1203.4,1198.7,0.0,0.0,IDLE,1:-123:-45\r\n');
    eq(p.samples.length, 0, 'samples 条数');
});

check('14. 还没收到任何表头就来了数据行 → 整行丢弃', () => {
    const p = new TelemetryParser();
    const out = p.feed('D,12340,-91.25,1F\r\n');
    eq(p.samples.length, 0, 'samples 条数');
    eq(out.textLines.length, 0, '数据行不该混进终端文本');
});

check('15. 导出 CSV 仍保留 ms 与 mode（只是不画图，不是不记录）', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed(FULL_ROW);
    const csv = p.toCsv();
    const head = csv.split('\n')[0].split(',');
    eq(head.includes('ms'), true, '导出应含 ms');
    eq(head.includes('mode'), true, '导出应含 mode');
    eq(head.includes('k230_x'), true, '导出应含展开后的 k230_x');
    eq(csv.split('\n')[1], '12340,-91.25,31,0,1203.4,1198.7,0,0,IDLE,1,-123,-45', '数据行内容');
});

/* ---- 遥测调参升级（分支 update-遥测）新增行为 ---- */

check('16. 新调参列 TL/TR、PL/PR、navT/navE、lerr 正常解析', () => {
    const p = new TelemetryParser();
    p.feed('H,ms,LV,RV,mode,TL,TR,PL,PR,navT,navE,lerr\r\n');
    p.feed('D,100,198.5,201.0,SPEED,200.0,200.0,412,-388,90.00,-1.25,-3.0\r\n');
    eq(p.samples.length, 1, 'samples 条数');
    const s = p.samples[0];
    eq(s.TL, 200, 'TL');
    eq(s.PR, -388, 'PR');
    eq(s.navT, 90, 'navT');
    eq(s.navE, -1.25, 'navE');
    eq(s.lerr, -3, 'lerr');
    eq(s.mode, 'SPEED', 'mode 支持新模式文本');
});

check('17. unitOf 量纲分组：目标与实测同组，未知列自成一组', () => {
    eq(TelemetryParser.unitOf('LV'), 'mmps', 'LV');
    eq(TelemetryParser.unitOf('TL'), 'mmps', 'TL 与 LV 同组才能同刻度对比');
    eq(TelemetryParser.unitOf('PL'), 'pwm', 'PL');
    eq(TelemetryParser.unitOf('yaw'), 'deg', 'yaw');
    eq(TelemetryParser.unitOf('navT'), 'deg', 'navT 与 yaw 同组');
    eq(TelemetryParser.unitOf('lerr'), 'lerr', '未知列自成一组');
    eq(TelemetryParser.unitOf('k230_x'), 'k230_x', '复合虚拟列自成一组');
});

check('18. buildAiPack 包含参数、结构说明与完整 CSV 数据', () => {
    const samples = [
        { ms: 0, TL: 0, LV: 0 },
        { ms: 20, TL: 300, LV: 12.5 },
    ];
    const pack = buildAiPack({
        loop: '轮速 PI',
        structure: 'PI+前馈',
        params: 'wkp=1 wki=0',
        excitation: 'W300',
        rateHz: 30,
        columnsNote: 'TL=目标 LV=实测',
        expectation: '无振荡',
    }, samples);
    eq(pack.includes('轮速 PI'), true, '环路名');
    eq(pack.includes('wkp=1 wki=0'), true, '参数快照');
    eq(pack.includes('W300'), true, '激励');
    eq(pack.includes('```csv'), true, 'CSV 代码块');
    eq(pack.includes('ms,TL,LV'), true, 'CSV 表头');
    eq(pack.includes('20,300,12.5'), true, 'CSV 数据行');
});

check('19. samplesToCsv 与 toCsv 输出一致', () => {
    const p = new TelemetryParser();
    p.feed(FULL_HEADER);
    p.feed(FULL_ROW);
    eq(p.toCsv(), samplesToCsv(p.samples), '两个入口共用一份实现');
});

check('20. 速度→基础PWM：正反转静摩擦符号正确', () => {
    const forward = estimateWheelBasePwm(300, 2, 50);
    const backward = estimateWheelBasePwm(-300, 2, 50);
    eq(forward.raw, 650, '正转基础PWM');
    eq(backward.raw, -650, '反转基础PWM');
    eq(forward.saturated, false, '正转不应饱和');
});

check('21. 速度→基础PWM：零速不施加静摩擦', () => {
    const stopped = estimateWheelBasePwm(0, 2, 50);
    eq(stopped.raw, 0, '零速基础PWM');
    eq(stopped.clamped, 0, '零速限幅值');
});

check('22. 速度→基础PWM：超过±1000时明确报告饱和', () => {
    const result = estimateWheelBasePwm(800, 2, 0);
    eq(result.raw, 1600, '限幅前PWM');
    eq(result.clamped, 1000, '限幅后PWM');
    eq(result.saturated, true, '必须标记饱和');
    eq(estimateWheelBasePwm(500, 2, 0).saturated, true, '刚好触及1000也没有PI余量');
});

/* ---- 自动调参指标（网页二期）---- */

/* 构造 20ms 一拍的阶跃数据：目标 300，实测按给定序列。 */
function stepSamples(actuals) {
    return actuals.map((v, i) => ({ ms: i * 20, TL: i === 0 ? 0 : 300, LV: v }));
}

check('23. 阶跃指标：上升时间、超调、稳定时间、稳态误差', () => {
    /* 实测：0 →(20ms 目标出现) 100, 200, 280, 330, 310, 300, 300, 300, 300 */
    const samples = stepSamples([0, 100, 200, 280, 330, 310, 300, 300, 300, 300]);
    const m = computeStepMetrics(samples, 'TL', 'LV');
    eq(m !== null, true, '应能识别阶跃');
    eq(m.target, 300, '目标');
    /* 窗口从 i=1(ms=20) 开始；实测首次 ≥270 在 i=3(ms=60) → 上升 40ms */
    eq(m.riseMs, 40, '上升时间');
    eq(Math.round(m.overshootPct), 10, '超调=330/300-1');
    /* 从 i=5(ms=100,310) 起有出带（310<315 在带内!）——±5% 带为 285~315，
       330(i=4) 出带，其后 310、300... 都在带内 → 稳定点 i=5(ms=100) − 20 = 80ms */
    eq(m.settleMs, 80, '稳定时间');
    eq(m.ssePct !== null && Math.abs(m.ssePct) < 2, true, '稳态误差接近 0');
});

check('24. 阶跃指标：负速度阶跃同样可分析', () => {
    const samples = [0, -120, -250, -290, -300, -300, -300]
        .map((v, i) => ({ ms: i * 20, TL: i === 0 ? 0 : -300, LV: v }));
    const m = computeStepMetrics(samples, 'TL', 'LV');
    eq(m.target, -300, '负目标');
    eq(m.riseMs, 40, '负阶跃上升时间（首次 ≤ −270 在 ms=60，窗口起点 20）');
    eq(m.overshootPct, 0, '无超调');
});

check('25. 阶跃指标：没有阶跃返回 null', () => {
    const flat = [{ ms: 0, TL: 0, LV: 0 }, { ms: 20, TL: 0, LV: 1 }, { ms: 40, TL: 0, LV: 0 }];
    eq(computeStepMetrics(flat, 'TL', 'LV'), null, '全零目标');
});

check('26. 转向指标：到角时间、过冲与残差', () => {
    const navE = [90, 60, 30, 10, -4, -1, 0.5, 0.3, 0.2, 0.1];
    const samples = navE.map((e, i) => ({ ms: i * 50, navE: e }));
    const m = computeTurnMetrics(samples, 2);
    eq(m.initialErrDeg, 90, '初始误差');
    /* 最后一次 |navE|>2 在 i=4(-4, ms=200)，其后进带 → settle = 250-0 */
    eq(m.settleMs, 250, '到角时间');
    eq(m.overshootDeg, 4, '过冲 4°');
    eq(m.finalErrDeg < 2, true, '残差');
});

check('27. analyzeRun：按环路分派并容忍字段缺失', () => {
    const wheel = analyzeRun('wheel',
        stepSamples([0, 100, 200, 290, 300, 300, 300]), 300, 2);
    eq(wheel.left !== null, true, '轮速左侧有指标');
    eq(wheel.right, null, '缺 TR/RV 列时右侧为 null 而不是抛错');
    eq(analyzeRun('line', [{ ms: 0 }, { ms: 20 }, { ms: 40 }], 200, 2), null,
        '巡线缺 lerr 列返回 null');
    eq(analyzeRun('wheel', [], 300, 2), null, '空样本返回 null');
});

check('28. 前馈建议：按稳态差比例修正，异常输入拒绝', () => {
    /* 目标 300 实测稳态 270 → ff 2.0 → 2.222 */
    const next = suggestFeedforward(2.0, 300, 270);
    eq(Math.abs(next - 2.2222) < 0.001, true, '比例外推');
    eq(suggestFeedforward(2.0, 300, 297), null, '3% 内不建议');
    eq(suggestFeedforward(2.0, 300, 30), null, '稳态过低视为堵转，不外推');
    eq(suggestFeedforward(0, 300, 270), null, 'ff=0 无从缩放');
    eq(suggestFeedforward(2.0, 0, 0), null, '零目标拒绝');
});

check('29. AI 包携带自动指标行（可选字段，不给不出现）', () => {
    const meta = {
        loop: 'x', structure: 's', params: 'p', excitation: 'e',
        rateHz: 30, columnsNote: 'c', expectation: 'g',
    };
    const bare = buildAiPack(meta, [{ ms: 0, LV: 1 }]);
    eq(bare.includes('网页自动指标'), false, '未提供时不输出');
    const withMetrics = buildAiPack(
        { ...meta, metricsNote: '左 上升40ms' }, [{ ms: 0, LV: 1 }]);
    eq(withMetrics.includes('- 网页自动指标: 左 上升40ms'), true, '提供时输出');
});

/* ---- 精测、保护与绘图降载 ---- */

check('30. feed 回报新增样本数，纯文本不触发曲线重绘', () => {
    const p = new TelemetryParser();
    eq(p.feed('OK G=20\r\n').samplesAdded, 0, '纯文本新增样本数');
    p.feed('H,ms,LV\r\n');
    eq(p.feed('D,10,1.5\r\nD,20,2.5\r\n').samplesAdded, 2, '两行数据');
});

check('31. 绘图抽样限制点数并保留首尾与窄尖峰', () => {
    const samples = Array.from({ length: 1000 }, (_, i) => ({
        ms: i, LV: i === 503 ? 999 : 0,
    }));
    const out = downsampleForPlot(samples, 120, ['LV']);
    eq(out.length <= 120, true, '抽样后点数上限');
    eq(out[0], samples[0], '保留首点');
    eq(out[out.length - 1], samples[samples.length - 1], '保留末点');
    eq(out.some((s) => s.ms === 503), true, '保留尖峰');
});

check('32. 轮速稳态统计分别计算均值、标准差、MAE 与 PWM', () => {
    const samples = [
        { ms: 0, TL: 300, LV: 100, PL: 100 },
        { ms: 1000, TL: 300, LV: 200, PL: 120 },
        { ms: 2000, TL: 300, LV: 290, PL: 150 },
        { ms: 3000, TL: 300, LV: 310, PL: 170 },
        { ms: 4000, TL: 300, LV: 300, PL: 160 },
    ];
    const m = computeWheelSteadyStats(samples, 'TL', 'LV', 'PL', 2000);
    eq(m.samples, 3, '最后 2 秒样本数');
    eq(m.mean, 300, '稳态均值');
    eq(Math.abs(m.sd - Math.sqrt(200 / 3)) < 1e-9, true, '总体标准差');
    eq(Math.abs(m.mae - 20 / 3) < 1e-9, true, '平均绝对误差');
    eq(m.meanPwm, 160, '平均 PWM');
    eq(m.maxAbsPwm, 170, '最大绝对 PWM');
});

/* ---- 板载捕获与自适应频率（数据平台重构）---- */

check('33. 捕获 dump 与实时流分开解析，互不污染', () => {
    const p = new TelemetryParser();
    p.feed('H,ms,LV,mode\r\n');
    p.feed('D,100,120.5,SPEED\r\n');
    /* 捕获表头到达时，实时流样本不应被清空。 */
    const r = p.feed('CH,ms,TL,LV,PL\r\n');
    eq(r.captureStarted, true, '应报告捕获开始');
    eq(p.samples.length, 1, '实时流样本不受影响');

    const r2 = p.feed('C,0,300.0,0.0,120\r\nC,10,300.0,45.2,180\r\n');
    eq(r2.captureAdded, 2, '应报告新增捕获样本数');
    eq(p.captureSamples.length, 2, '捕获样本数');
    eq(p.captureSamples[1].ms, 10, '捕获时间戳');
    eq(p.captureSamples[1].LV, 45.2, '捕获实测值');
    eq(p.captureSamples[0].TL, 300, '捕获目标值');
});

check('34. 捕获行列数不符与半截行按同样规则丢弃/缓冲', () => {
    const p = new TelemetryParser();
    p.feed('CH,ms,TL,LV\r\n');
    p.feed('C,0,300.0\r\n');            /* 少一列 */
    eq(p.captureSamples.length, 0, '列数不足应丢弃');
    p.feed('C,10,300.0,50.0,999\r\n');  /* 多一列 */
    eq(p.captureSamples.length, 0, '列数超出应丢弃');
    p.feed('C,20,300.0,');
    eq(p.captureSamples.length, 0, '半截行先缓冲');
    p.feed('60.5\r\n');
    eq(p.captureSamples.length, 1, '拼接后成行');
    eq(p.captureSamples[0].LV, 60.5, '拼接后的值');
});

check('35. 捕获列表排除时间轴列，且可单独清空', () => {
    const p = new TelemetryParser();
    p.feed('CH,ms,TL,LV,PL\r\n');
    p.feed('C,0,300,0,120\r\n');
    eq(p.captureNumericColumns().join(','), 'TL,LV,PL', '可画图的捕获列');
    p.clearCapture();
    eq(p.captureSamples.length, 0, '清空捕获样本');
    eq(p.captureColumns.length, 0, '清空捕获列定义');
});

check('36. parseCapabilityReply 解析 Q 回应，非 Q 行返回 null', () => {
    const full = parseCapabilityReply(
        'OK Q MAX=57 MASK=5136 RATE=50 CAPST=1 CAPN=812 CAPMAX=1024');
    eq(full.max, 57, 'MAX');
    eq(full.mask, 5136, 'MASK');
    eq(full.rate, 50, 'RATE');
    eq(full.captureState, 1, 'CAPST');
    eq(full.captureCount, 812, 'CAPN');
    eq(full.capMax, 1024, 'CAPMAX');

    /* 老固件可能只回前三项，不能因此整条丢弃。 */
    const partial = parseCapabilityReply('OK Q MAX=14 MASK=1023 RATE=14');
    eq(partial.max, 14, '简版 MAX');
    eq(partial.capMax, null, '简版无捕获字段');

    eq(parseCapabilityReply('OK G=20'), null, '非 Q 行');
    eq(parseCapabilityReply(''), null, '空行');
    eq(parseCapabilityReply(undefined), null, 'undefined');
});

check('37. 本地频率估算与固件公式一致（行长与上限）', () => {
    /* 固件 Telemetry.c：基座 14 B，各字段宽度见 EstimateRowBytes。 */
    eq(estimateRowBytes(0), 14, '空掩码只有行基座');
    eq(estimateRowBytes(0x01), 25, 'yaw 单字段行长');
    /* 全部 16 位字段。 */
    const allBytes = estimateRowBytes(0xFFFF);
    eq(allBytes, 14 + 11 + 8 + 20 + 20 + 9 + 16 + 20 + 12 + 22 + 6
        + 10 + 10 + 10 + 10 + 6 + 6, '全字段行长');

    /* 上限公式：(11520 × 20) / (行长 × 100)，夹到 1..100。 */
    eq(estimateMaxRateHz(0x01), 92, '只开 yaw 可达 92 Hz');
    eq(estimateMaxRateHz(0xFFFF) <= 14, true, '全字段不超过 14 Hz');
    eq(estimateMaxRateHz(0xFFFF) >= 1, true, '上限不小于 1');
});

check('38. 单侧掩码的可用频率显著高于成对掩码', () => {
    /* 左轮单侧：TL(0x1000) + LV(0x400) + PL(0x4000) + mode(0x10) */
    const single = estimateMaxRateHz(0x1000 | 0x400 | 0x4000 | 0x10);
    /* 成对：TL/TR(0x40) + LV/RV(0x08) + PL/PR(0x80) + mode(0x10) */
    const paired = estimateMaxRateHz(0x40 | 0x08 | 0x80 | 0x10);
    eq(single > paired, true, `单侧 ${single}Hz 应高于成对 ${paired}Hz`);
    eq(single >= paired * 1.5, true,
        `单侧提升不足：${single}Hz vs ${paired}Hz`);
});

check('39. 捕获通道计数用于自查是否超过固件 4 通道上限', () => {
    eq(countCaptureChannels(0), 0, '空掩码');
    eq(countCaptureChannels(0x0001 | 0x0002 | 0x0004), 3, '三通道');
    eq(countCaptureChannels(0x0001 | 0x0002 | 0x0004 | 0x0008), 4, '四通道');
    eq(countCaptureChannels(0x01FF), 9, '全部九通道（超限，应被拒绝）');
});

check('40. AI 包在使用捕获数据时明确标注采样率与同频事实', () => {
    const meta = {
        loop: '轮速', structure: 's', params: 'p', excitation: 'W300',
        rateHz: 100, columnsNote: 'TL=目标。数据来自板载 100 Hz 捕获，与 100 Hz 控制环同频',
        expectation: 'g',
    };
    const pack = buildAiPack(meta, [{ ms: 0, TL: 0 }, { ms: 10, TL: 300 }]);
    eq(pack.includes('100 Hz'), true, '应标注采样率');
    eq(pack.includes('同频'), true, '应说明与控制环同频');
});

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);
