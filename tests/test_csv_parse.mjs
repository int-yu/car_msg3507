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
const { TelemetryParser, samplesToCsv, buildAiPack } = new Function(
    parserSource + '\nreturn { TelemetryParser, samplesToCsv, buildAiPack };'
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

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);
