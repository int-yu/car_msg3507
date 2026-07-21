import { readFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve, basename } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, '..');
const tutorialDir = join(root, 'tutorial');
const manifest = JSON.parse(readFileSync(join(tutorialDir, 'module-manifest.json'), 'utf8'));

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

function decodeCodeHtml(value) {
    return value
        .replace(/&lt;/g, '<')
        .replace(/&gt;/g, '>')
        .replace(/&quot;/g, '"')
        .replace(/&#39;|&#x27;/g, "'")
        .replace(/&#x([0-9a-f]+);/gi, (_, value) => String.fromCodePoint(Number.parseInt(value, 16)))
        .replace(/&#(\d+);/g, (_, value) => String.fromCodePoint(Number(value)))
        .replace(/&amp;/g, '&');
}

console.log('项目教程网页结构检查\n');

check('页面模板保留内容插槽且内联脚本语法有效', () => {
    const template = readFileSync(join(tutorialDir, 'template.html'), 'utf8');
    ok(template.includes('<!-- MODULE_CONTENT -->'), '缺少模块内容插槽');
    ok(template.includes('<!-- BUILD_INFO -->'), '缺少构建信息插槽');
    ok(template.includes('<!-- READING_MINUTES -->'), '缺少构建期阅读时长插槽');
    ok(template.includes('content-visibility: auto'), '长章节未启用离屏渲染优化');
    ok(template.includes('function decorateSourceBlock'), '缺少单个源码块的惰性装饰函数');
    ok(template.includes('rootMargin: "350px 0px"'), '源码章节未配置接近视口时再装饰');
    ok(!template.includes('function calculateReadingTime'), '仍在首屏遍历全文计算阅读时长');
    const scripts = [...template.matchAll(/<script>([\s\S]*?)<\/script>/g)];
    ok(scripts.length === 1, `预期 1 段内联脚本，实际 ${scripts.length} 段`);
    new Function(scripts[0][1]);
});

check('模块编号连续且正好 10 个', () => {
    ok(manifest.modules.length === 10, `实际为 ${manifest.modules.length} 个`);
    const ids = manifest.modules.map((item) => item.id).join(',');
    ok(ids === '01,02,03,04,05,06,07,08,09,10', `编号为 ${ids}`);
});

check('清单中的源码文件全部存在且只归属一个教学模块', () => {
    const owners = new Map();
    for (const module of manifest.modules) {
        for (const source of module.sources) {
            ok(existsSync(join(root, source)), `不存在：${source}`);
            ok(!owners.has(source), `${source} 同时属于模块 ${owners.get(source)} 和 ${module.id}`);
            owners.set(source, module.id);
        }
    }
});

for (const module of manifest.modules) {
    check(`模块 ${module.id} 片段结构、术语、逐行注释和自测齐全`, () => {
        const path = join(tutorialDir, module.fragment);
        ok(existsSync(path), `缺少 ${module.fragment}`);
        const html = readFileSync(path, 'utf8');
        ok(html.includes(`id="module-${module.id}"`), 'section id 不正确');
        ok(html.includes('class="module-map"'), '缺少总体位置图');
        ok(html.includes('class="source-code"'), '缺少源码块');
        ok(html.includes('class="line-notes"'), '缺少逐行解释');
        ok(html.includes('class="deep-dive"'), '缺少难函数拆解');
        ok(html.includes('class="relation-chain"'), '缺少模块调用关系');
        ok(html.includes('class="coverage-list"'), '缺少覆盖清单');
        ok(html.includes('class="quiz"'), '缺少学完自测');
        ok(/<dfn\s+data-term=/.test(html), '缺少专业术语解释');
        ok(!/\b(?:TODO|TBD|待补充|占位符)\b/i.test(html), '仍有待办占位文本');

        for (const source of module.sources) {
            ok(html.includes(basename(source)), `覆盖清单未提到 ${source}`);
        }
    });
}

check('所有源码块都有源文件与真实行号属性', () => {
    for (const module of manifest.modules) {
        const fragmentPath = join(tutorialDir, module.fragment);
        if (!existsSync(fragmentPath)) continue;
        const html = readFileSync(fragmentPath, 'utf8');
        const blocks = [...html.matchAll(/<pre class="source-code"><code([^>]*)>([\s\S]*?)<\/code><\/pre>/g)];
        ok(blocks.length > 0, `${module.id} 没有源码块`);
        for (const block of blocks) {
            ok(/data-source="[^"]+"/.test(block[1]), `${module.id} 有源码块缺 data-source`);
            ok(/data-lines="\d+(?:-\d+)?"/.test(block[1]), `${module.id} 有源码块缺合法 data-lines`);

            const sourceName = block[1].match(/data-source="([^"]+)"/)[1].replace(/\\/g, '/');
            const sourcePath = join(root, sourceName);
            ok(existsSync(sourcePath), `${module.id} 源码块引用不存在文件 ${sourceName}`);
            const lineText = block[1].match(/data-lines="(\d+)(?:-(\d+))?"/);
            const start = Number(lineText[1]);
            const end = Number(lineText[2] || lineText[1]);
            const sourceLines = readFileSync(sourcePath, 'utf8').replace(/\r\n/g, '\n').split('\n');
            const expected = sourceLines.slice(start - 1, end).join('\n').trimEnd();
            const actual = decodeCodeHtml(block[2]).replace(/\r\n/g, '\n').replace(/^\n/, '').trimEnd();
            ok(actual === expected, `${module.id} 的 ${sourceName}:${start}-${end} 与当前源码不一致`);
        }
    }
});

check('生成网页包含完整导航、交互控件与每个模块', () => {
    const outputPath = join(tutorialDir, 'index.html');
    ok(existsSync(outputPath), '尚未运行 node tutorial/build.mjs');
    const html = readFileSync(outputPath, 'utf8');
    ok(html.includes('id="tutorial-search"'), '缺少搜索框');
    ok(html.includes('id="progress-bar"'), '缺少学习进度条');
    ok(html.includes('id="glossary-panel"'), '缺少术语面板');
    ok(html.includes('function decorateSourceBlocks'), '缺少源码增强脚本');
    for (const module of manifest.modules) {
        const count = html.split(`id="module-${module.id}"`).length - 1;
        ok(count === 1, `模块 ${module.id} 出现 ${count} 次`);
    }
});

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);
