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
    countCaptureChannels, segmentByMark, summarizeSession,
} = new Function(
    parserSource + '\nreturn { TelemetryParser, samplesToCsv, buildAiPack,' +
    ' estimateWheelBasePwm, computeStepMetrics, computeTurnMetrics,' +
    ' analyzeRun, suggestFeedforward, downsampleForPlot,' +
    ' computeWheelSteadyStats, parseCapabilityReply, estimateRowBytes,' +
    ' estimateMaxRateHz, countCaptureChannels, segmentByMark,' +
    ' summarizeSession };'
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

console.log('遥测纯函数测试（帧解析见 test_binary_frame.mjs）\n');

/* 二进制协议后，帧解析类测试全部迁到 tests/test_binary_frame.mjs。
   本文件只保留与协议无关的纯函数测试（指标、AI 包、会话、频率估算）。 */

check('17. 静态 unitOf 兜底量纲分组（无 schema 上下文时）', () => {
    /* 实例方法 unitOf 用 schema 单位码，见 test_binary_frame.mjs；
       静态版是无实例时的兜底，按已知列名给量纲。 */
    eq(TelemetryParser.unitOf('LV'), 'mmps', 'LV');
    eq(TelemetryParser.unitOf('TL'), 'mmps', 'TL 与 LV 同组才能同刻度对比');
    eq(TelemetryParser.unitOf('PL'), 'pwm', 'PL');
    eq(TelemetryParser.unitOf('yaw'), 'deg', 'yaw');
    eq(TelemetryParser.unitOf('navE'), 'deg', 'navE 与 yaw 同组');
    eq(TelemetryParser.unitOf('lerr'), 'lerr', '未知列自成一组');
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
    /* 直接塞样本，绕开帧解析（帧解析在 test_binary_frame.mjs 测）。 */
    p.samples = [{ ms: 0, TL: 300, LV: 12.5 }, { ms: 10, TL: 300, LV: 40 }];
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

/* 帧解析行为已迁至 tests/test_binary_frame.mjs，此处不再重复。 */

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

/* ---- 自适应频率（捕获帧解析在 test_binary_frame.mjs）---- */

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

check('37. 二进制帧字节数与固件公式一致（帧开销 + ms + 通道×4）', () => {
    /* 固件 Telemetry.c：SAMPLE 帧 = 帧开销 7 + payload(ms:4 + 通道数×4)。 */
    eq(estimateRowBytes(0), 11, '空掩码：7 开销 + 4 ms');
    eq(estimateRowBytes(0x01), 15, '单通道：+4 字节');
    eq(estimateRowBytes(0x03), 19, '双通道：+8 字节');
    /* 全 12 通道：7 + 4 + 12×4 = 59。 */
    eq(estimateRowBytes(0x0FFF), 7 + 4 + 12 * 4, '全 12 通道帧长');
});

check('38. DMA 二进制后单通道可达远超控制环的频率', () => {
    /* 上限公式：(11520 × 70) / (帧长 × 100)，夹到 1..100。 */
    const single = estimateMaxRateHz(0x01);      /* 15 字节 */
    const all = estimateMaxRateHz(0x0FFF);       /* 59 字节 */
    eq(single, 100, '单通道达到硬上限 100Hz（DMA 后带宽绰绰有余）');
    eq(all >= 100, true, '全 12 通道也能到 100Hz（59B×100Hz 远小于带宽）');
    /* 通道越少帧越短、可用频率越高（未夹紧时）。 */
    eq(estimateRowBytes(0x01) < estimateRowBytes(0x0FFF), true,
        '通道越少帧越短');
});

check('39. 通道计数覆盖全 12 位（自查是否超过固件 8 通道捕获上限）', () => {
    eq(countCaptureChannels(0), 0, '空掩码');
    eq(countCaptureChannels(0x0001 | 0x0002 | 0x0004), 3, '三通道');
    eq(countCaptureChannels(0x0FFF), 12, '全 12 通道');
    /* 捕获上限 8：9 通道以上应被固件拒绝，网页先自查。 */
    eq(countCaptureChannels(0x01FF), 9, '9 通道（超捕获上限）');
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

/* ---- 遥控采集：标记分段与会话摘要 ---- */

check('41. segmentByMark 按标记切分，连续同标记归为一段', () => {
    const samples = [
        { ms: 0, mark: 0 }, { ms: 10, mark: 0 },
        { ms: 20, mark: 1 }, { ms: 30, mark: 1 }, { ms: 40, mark: 1 },
        { ms: 50, mark: 2 },
        { ms: 60, mark: 0 },
    ];
    const segments = segmentByMark(samples);
    eq(segments.length, 4, '段数');
    eq(segments[0].mark, 0, '第一段标记');
    eq(segments[0].samples.length, 2, '第一段样本数');
    eq(segments[1].mark, 1, '第二段标记');
    eq(segments[1].samples.length, 3, '第二段样本数');
    eq(segments[1].startMs, 20, '第二段起点');
    eq(segments[1].endMs, 40, '第二段终点');
    eq(segments[3].mark, 0, '标记归零后另起一段');
});

check('42. segmentByMark 处理无标记列与空输入', () => {
    eq(segmentByMark([]).length, 0, '空输入');
    /* 没有 mark 字段时按 0 处理，全部归一段。 */
    const noMark = segmentByMark([{ ms: 0 }, { ms: 10 }, { ms: 20 }]);
    eq(noMark.length, 1, '无标记列时只有一段');
    eq(noMark[0].mark, 0, '缺省标记为 0');
});

check('43. summarizeSession 统计时长、实际频率与各标记样本数', () => {
    /* 20ms 一拍 = 50 Hz，共 6 个样本跨 100ms。 */
    const samples = [
        { ms: 0, mark: 0 }, { ms: 20, mark: 1 }, { ms: 40, mark: 1 },
        { ms: 60, mark: 1 }, { ms: 80, mark: 2 }, { ms: 100, mark: 0 },
    ];
    const s = summarizeSession(samples);
    eq(s.count, 6, '样本数');
    eq(s.durationMs, 100, '时长');
    eq(s.rateHz, 50, '实际频率按间隔数而非样本数计算');
    eq(s.markCounts[1], 3, '标记 1 的样本数');
    eq(s.markCounts[2], 1, '标记 2 的样本数');
    eq(s.markCounts[0], 2, '未标记样本数');
});

check('44. summarizeSession 处理空输入与单样本，不produce NaN', () => {
    const empty = summarizeSession([]);
    eq(empty.count, 0, '空输入样本数');
    eq(empty.rateHz, 0, '空输入频率');
    eq(empty.durationMs, 0, '空输入时长');

    /* 单样本时长为 0，不能除零得到 Infinity。 */
    const single = summarizeSession([{ ms: 500, mark: 0 }]);
    eq(single.count, 1, '单样本数');
    eq(single.durationMs, 0, '单样本时长');
    eq(single.rateHz, 0, '单样本频率应为 0 而不是 Infinity');
    eq(Number.isFinite(single.rateHz), true, '频率必须是有限值');

    eq(summarizeSession(null).count, 0, 'null 输入');
    eq(summarizeSession(undefined).count, 0, 'undefined 输入');
});

check('45. 采集数据导出的 CSV 保留 mark 列', () => {
    const csv = samplesToCsv([
        { ms: 0, LV: 100, mark: 0 },
        { ms: 10, LV: 120, mark: 2 },
    ]);
    const head = csv.split('\n')[0];
    eq(head.includes('mark'), true, '表头应含 mark 列');
    eq(csv.split('\n')[1], '0,100,0', '第一行');
    eq(csv.split('\n')[2], '10,120,2', '第二行含标记值');
});

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);
