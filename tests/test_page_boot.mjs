/*
 * 页面启动冒烟测试。
 *
 * car_debug.html 的脚本是一大段顶层代码，语法检查（node --check）过不代表
 * 能跑起来：函数声明会提升而 let/const 不会，初始化顺序错了就是运行期
 * ReferenceError，而这类错误只有真正打开页面才暴露。这里用最小 DOM 桩把
 * 整段脚本执行一遍，把「打开页面就白屏」挡在提交之前。
 *
 * 跑法：node tests/test_page_boot.mjs
 */
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const html = readFileSync(join(root, 'car_debug.html'), 'utf8');

const scriptStart = html.indexOf('<script>');
const scriptEnd = html.lastIndexOf('</script>');
if (scriptStart < 0 || scriptEnd < 0) {
    throw new Error('car_debug.html 里找不到 <script> 块');
}
const source = html.slice(scriptStart + '<script>'.length, scriptEnd);

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

/* ---- 最小 DOM 桩 ---- */

function makeElement(id = '') {
    const listeners = {};
    const element = {
        id,
        tagName: 'DIV',
        dataset: {},
        style: {},
        value: '20',
        textContent: '',
        innerHTML: '',
        checked: false,
        disabled: false,
        childElementCount: 0,
        scrollTop: 0,
        scrollHeight: 0,
        clientHeight: 100,
        clientWidth: 300,
        width: 300,
        height: 100,
        firstChild: null,
        classList: {
            add() {}, remove() {}, toggle() {}, contains: () => false,
        },
        append() {}, appendChild() {}, removeChild() {}, remove() {},
        querySelectorAll: () => [],
        querySelector: () => null,
        contains: () => false,
        addEventListener(type, fn) { (listeners[type] ||= []).push(fn); },
        removeEventListener() {},
        getBoundingClientRect: () => ({ left: 0, top: 0, width: 300, height: 100 }),
        getContext: () => ({
            setTransform() {}, clearRect() {}, beginPath() {}, moveTo() {},
            lineTo() {}, stroke() {}, fill() {}, fillText() {}, fillRect() {},
            save() {}, restore() {}, translate() {}, rotate() {},
            roundRect() {}, setLineDash() {}, arc() {}, closePath() {},
            measureText: () => ({ width: 10 }),
        }),
        /* 事件处理器以属性形式赋值（onclick/oninput/onchange 等）。 */
        _listeners: listeners,
    };
    return element;
}

function buildEnvironment() {
    const registry = new Map();
    const created = [];
    const documentListeners = {};

    const doc = {
        getElementById(id) {
            if (!registry.has(id)) {
                registry.set(id, makeElement(id));
            }
            return registry.get(id);
        },
        createElement(tag) {
            const element = makeElement();
            element.tagName = String(tag).toUpperCase();
            created.push(element);
            return element;
        },
        querySelectorAll: () => [],
        querySelector: () => null,
        addEventListener(type, fn) { (documentListeners[type] ||= []).push(fn); },
        body: makeElement('body'),
        activeElement: null,
    };

    const timers = [];
    return {
        registry,
        created,
        documentListeners,
        timers,
        globals: {
            document: doc,
            window: {
                addEventListener() {},
                devicePixelRatio: 1,
                matchMedia: () => ({ matches: false, addEventListener() {} }),
            },
            localStorage: {
                _data: {},
                getItem(key) { return this._data[key] ?? null; },
                setItem(key, value) { this._data[key] = String(value); },
                removeItem(key) { delete this._data[key]; },
            },
            navigator: {},
            requestAnimationFrame: () => 0,
            cancelAnimationFrame: () => {},
            setTimeout: (fn, ms) => { timers.push({ fn, ms }); return timers.length; },
            clearTimeout: () => {},
            setInterval: () => 0,
            clearInterval: () => {},
            performance: { now: () => 0 },
            TextDecoder: class { decode() { return ''; } },
            TextEncoder: class { encode() { return new Uint8Array(); } },
            URL: { createObjectURL: () => 'blob:', revokeObjectURL() {} },
            Blob: class {},
            alert: () => {},
        },
    };
}

console.log('页面启动冒烟测试\n');

check('1. 整段脚本能在最小 DOM 环境下执行完毕（无 TDZ / 未定义引用）', () => {
    const env = buildEnvironment();
    const names = Object.keys(env.globals);
    const values = names.map((n) => env.globals[n]);
    /* 末尾把要断言的内部状态交出来。 */
    const factory = new Function(...names,
        source + '\nreturn { MARK_LABELS, CAPTURE_CH, TUNING_FOCUS, SESSIONS,' +
        ' driveEnabled, recording, focusState };');
    const exported = factory(...values);
    ok(exported, '脚本没有返回内部状态，可能提前抛错');
    ok(Array.isArray(exported.MARK_LABELS), 'MARK_LABELS 未初始化');
    ok(exported.recording === null, '初始不应处于录制状态');
    ok(exported.driveEnabled === false, '初始不应启用键盘驾驶');
});

check('2. 初始化会渲染关键面板，不是等到连接后才有内容', () => {
    const env = buildEnvironment();
    const names = Object.keys(env.globals);
    new Function(...names, source)(...names.map((n) => env.globals[n]));

    /* 这些元素必须在启动阶段就被写过一次，否则用户看到的是空面板。 */
    for (const id of ['markButtons', 'sessionList', 'rateLimitHint']) {
        ok(env.registry.has(id), `启动时未触及 #${id}`);
    }
});

check('3. 键盘事件在启动时就注册，且驾驶键与标记键分开处理', () => {
    const env = buildEnvironment();
    const names = Object.keys(env.globals);
    new Function(...names, source)(...names.map((n) => env.globals[n]));

    const keydown = env.documentListeners.keydown || [];
    ok(keydown.length >= 1, '未注册 keydown 监听');

    /* 未启用驾驶、未录制时，按 W 不应发出任何命令——这是防误触的底线。 */
    let sent = 0;
    const fakeEvent = (key) => ({
        key, repeat: false, target: { tagName: 'BODY' },
        preventDefault() { sent += 0; },
    });
    for (const handler of keydown) {
        handler(fakeEvent('w'));
    }
    ok(sent === 0, '未启用驾驶模式时不应响应 W 键');
});

check('4. HTML 与脚本的元素 id 对得上（防止改版漏改一侧）', () => {
    /* 脚本里 $('xxx') 引用的 id，HTML 必须真的有。 */
    const referenced = new Set(
        [...source.matchAll(/\$\('([A-Za-z][\w-]*)'\)/g)].map((m) => m[1]));
    const declared = new Set(
        [...html.matchAll(/\bid="([A-Za-z][\w-]*)"/g)].map((m) => m[1]));
    const missing = [...referenced].filter((id) => !declared.has(id));
    ok(missing.length === 0,
        `脚本引用了 HTML 中不存在的 id：${missing.join(', ')}`);
});

check('5. 遥控采集面板的控件齐全', () => {
    for (const id of ['btnDriveMode', 'btnRecord', 'driveSpeed', 'driveTurn',
        'markButtons', 'sessionList', 'recordStatus', 'drivePanel']) {
        ok(html.includes(`id="${id}"`), `HTML 缺少 #${id}`);
    }
    ok(html.includes('data-drive="forward"'), '缺少前进键');
    ok(html.includes('data-drive="stop"'), '缺少停止键');
    ok(html.includes('data-drive="left"') && html.includes('data-drive="right"'),
        '缺少转向键');
});

console.log(`\n通过 ${passed}，失败 ${failed}`);
process.exit(failed === 0 ? 0 : 1);
