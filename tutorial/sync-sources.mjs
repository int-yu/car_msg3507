/*
 * 把教程里内联的源码块重新对齐到当前源文件。
 *
 * 教程用 data-source + data-lines 把源码逐字抄进 HTML，好处是离线可读、
 * 带行号，代价是源文件一改行号就偏。手工改几十处极易出错，所以用这个脚本：
 * 按每块声明的行号重新截取，并在源码位移时尝试用原内容重新定位行号。
 *
 * 跑法：node tutorial/sync-sources.mjs [--dry]
 */
import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..');
const sectionsDir = join(here, 'sections');
const dryRun = process.argv.includes('--dry');

function encodeCodeHtml(text) {
    return text
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;');
}

function decodeCodeHtml(text) {
    return text
        .replace(/&lt;/g, '<')
        .replace(/&gt;/g, '>')
        .replace(/&quot;/g, '"')
        .replace(/&#39;/g, "'")
        .replace(/&amp;/g, '&');
}

/*
 * 源码位移后按内容重新定位：拿原块的首尾非空行去当前文件里找。
 * 找到唯一匹配才认，避免把块对到相似但不同的位置上。
 */
function relocate(sourceLines, oldBlockLines) {
    const meaningful = oldBlockLines.filter((line) => line.trim() !== '');
    if (meaningful.length === 0) {
        return null;
    }
    const firstLine = meaningful[0];
    const candidates = [];
    for (let i = 0; i < sourceLines.length; i += 1) {
        if (sourceLines[i] === firstLine) {
            candidates.push(i);
        }
    }
    if (candidates.length === 0) {
        return null;
    }

    /* 首行可能重复（例如多处相同的注释），用整块内容筛出唯一解。 */
    const exact = candidates.filter((start) => {
        const slice = sourceLines.slice(start, start + oldBlockLines.length);
        return slice.join('\n').trimEnd() === oldBlockLines.join('\n').trimEnd();
    });
    if (exact.length === 1) {
        return { start: exact[0] + 1, end: exact[0] + oldBlockLines.length };
    }
    if (candidates.length === 1) {
        /* 内容有微调但首行唯一：保持原块行数，交由后续逐字覆盖。 */
        return {
            start: candidates[0] + 1,
            end: candidates[0] + oldBlockLines.length,
        };
    }
    return null;
}

let scanned = 0;
let updated = 0;
let relocated = 0;
const problems = [];

const files = readdirSync(sectionsDir).filter((name) => name.endsWith('.html'));
for (const name of files) {
    const filePath = join(sectionsDir, name);
    const original = readFileSync(filePath, 'utf8');
    let changed = false;

    const next = original.replace(
        /(<pre class="source-code"><code([^>]*)>)([\s\S]*?)(<\/code><\/pre>)/g,
        (whole, openTag, attrs, body, closeTag) => {
            const sourceMatch = attrs.match(/data-source="([^"]+)"/);
            const linesMatch = attrs.match(/data-lines="(\d+)(?:-(\d+))?"/);
            if (!sourceMatch || !linesMatch) {
                return whole;
            }
            scanned += 1;

            const sourceName = sourceMatch[1].replace(/\\/g, '/');
            let sourceText;
            try {
                sourceText = readFileSync(join(root, sourceName), 'utf8');
            } catch (err) {
                problems.push(`${name}: 找不到源文件 ${sourceName}`);
                return whole;
            }
            const sourceLines = sourceText.replace(/\r\n/g, '\n').split('\n');

            let start = Number(linesMatch[1]);
            let end = Number(linesMatch[2] || linesMatch[1]);
            const currentBody = decodeCodeHtml(body)
                .replace(/\r\n/g, '\n').replace(/^\n/, '').trimEnd();
            const atDeclared = sourceLines.slice(start - 1, end)
                .join('\n').trimEnd();

            if (atDeclared === currentBody) {
                return whole;   /* 已一致，不动 */
            }

            /* 声明的行号对不上了，按内容重新定位。 */
            const found = relocate(sourceLines, currentBody.split('\n'));
            if (!found) {
                problems.push(
                    `${name}: ${sourceName}:${start}-${end} 内容已大改，需人工确认`);
                return whole;
            }
            if (found.start !== start || found.end !== end) {
                relocated += 1;
            }
            start = found.start;
            end = found.end;

            const freshBody = sourceLines.slice(start - 1, end)
                .join('\n').trimEnd();
            const newAttrs = attrs.replace(/data-lines="[^"]*"/,
                `data-lines="${start}-${end}"`);
            changed = true;
            updated += 1;
            return `<pre class="source-code"><code${newAttrs}>` +
                encodeCodeHtml(freshBody) + closeTag;
        });

    if (changed && !dryRun) {
        writeFileSync(filePath, next, 'utf8');
    }
    if (changed) {
        console.log(`${dryRun ? '[dry] ' : ''}${name} 已更新`);
    }
}

console.log(`\n扫描 ${scanned} 个源码块，更新 ${updated} 个（其中重定位行号 ${relocated} 个）`);
if (problems.length > 0) {
    console.log('\n需要人工处理：');
    problems.forEach((p) => console.log('  - ' + p));
    process.exit(1);
}
