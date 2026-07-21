import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const tutorialDir = dirname(fileURLToPath(import.meta.url));
const projectRoot = resolve(tutorialDir, '..');
const manifest = JSON.parse(readFileSync(join(tutorialDir, 'module-manifest.json'), 'utf8'));
const template = readFileSync(join(tutorialDir, 'template.html'), 'utf8');

if (!template.includes('<!-- MODULE_CONTENT -->')) {
    throw new Error('template.html 缺少 MODULE_CONTENT 占位标记');
}

const fragments = manifest.modules.map((module) => {
    const fragmentPath = join(tutorialDir, module.fragment);
    if (!existsSync(fragmentPath)) {
        throw new Error(`缺少模块 ${module.id} 讲解稿：${module.fragment}`);
    }
    for (const source of module.sources) {
        if (!existsSync(join(projectRoot, source))) {
            throw new Error(`模块 ${module.id} 引用了不存在的源码：${source}`);
        }
    }
    const html = readFileSync(fragmentPath, 'utf8').trim();
    if (!html.includes(`id="module-${module.id}"`)) {
        throw new Error(`模块 ${module.id} 的根 section id 不正确`);
    }
    return `<!-- module:${module.id}:${module.title} -->\n${html}`;
});

const buildInfo = `由 tutorial/build.mjs 生成 · ${manifest.modules.length} 个模块 · 离线单文件`;
const readingCharacters = fragments.join('\n')
    .replace(/<[^>]*>/g, '')
    .replace(/&(?:#\d+|#x[0-9a-f]+|[a-z]+);/gi, '字')
    .replace(/\s/g, '')
    .length;
const readingMinutes = Math.max(1, Math.ceil(readingCharacters / 500));
const output = template
    .replace('<!-- MODULE_CONTENT -->', fragments.join('\n\n'))
    .replace('<!-- BUILD_INFO -->', buildInfo)
    .replace('<!-- READING_MINUTES -->', String(readingMinutes));

const outputPath = join(tutorialDir, 'index.html');
writeFileSync(outputPath, output, 'utf8');
console.log(`已生成 ${outputPath}`);
console.log(`模块数：${manifest.modules.length}，文件大小：${Buffer.byteLength(output)} 字节`);
