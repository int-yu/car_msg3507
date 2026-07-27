const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');

const html = readFileSync(join(__dirname, '..', 'car_debug.html'), 'utf8');

for (const required of [
    'id="debugView"',
    'id="videoView"',
    'data-workspace-view="debug"',
    'data-workspace-view="video"',
    'id="videoFeed"',
    'id="videoRtspUrl"',
    'id="btnVideoStart"',
    'id="videoZoom"',
    'id="videoRotation"',
    'id="btnVideoRotateLeft"',
    'id="btnVideoRotateRight"',
    'id="btnVideoFit"',
    'id="btnVideoReset"',
    'id="btnVideoDriveMode"',
    'data-video-drive="forward"',
    'data-video-drive="left"',
    'data-video-drive="back"',
    'data-video-drive="right"',
    'id="btnVideoCapture"',
    'function setWorkspaceView(',
    'async function startVideo(',
    'async function pullLatestVideoFrames(',
    'async function fetchLatestVideoFrame(',
    '/frame.jpg?generation=',
    'clearDriveSlave(true)',
    'videoOperationId',
    'const sent = await send(\'P\' + count)',
    'VIEWER_PAGE_SERVED',
]) {
    assert.ok(html.includes(required), `car_debug.html 缺少 ${required}`);
}

assert.ok(!html.includes('/stream.mjpg?generation='),
    'car_debug.html 不应使用会追赶历史帧的 MJPEG 长连接');

// 低延迟取帧循环的两条结构约束，回归到串行版本会让每帧多等一个来回。
const pullLoop = html.slice(
    html.indexOf('async function pullLatestVideoFrames('),
    html.indexOf('function showVideoStopped('));
assert.ok(pullLoop.length > 0, '找不到 pullLatestVideoFrames 函数体');
const nextRequestAt = pullLoop.lastIndexOf('pending = fetchLatestVideoFrame(');
const decodeAt = pullLoop.indexOf('await showLatestVideoFrame(');
assert.ok(nextRequestAt > 0 && decodeAt > 0,
    'pullLatestVideoFrames 必须既发起下一帧请求又解码当前帧');
assert.ok(nextRequestAt < decodeAt,
    '必须先发出下一帧请求再解码当前帧，否则解码期间链路空闲');

const showFrame = html.slice(
    html.indexOf('async function showLatestVideoFrame('),
    html.indexOf('async function fetchLatestVideoFrame('));
assert.ok(showFrame.includes('await image.decode()'),
    'showLatestVideoFrame 应使用 decode() 等待解码完成');
assert.ok(!showFrame.includes('image.onload'),
    'showLatestVideoFrame 不应回到 onload：上屏时会再触发一次同步解码');

const ids = [...html.matchAll(/\bid="([^"]+)"/g)].map((match) => match[1]);
const duplicateIds = ids.filter((id, index) => ids.indexOf(id) !== index);
assert.deepEqual([...new Set(duplicateIds)], [], 'HTML id 必须唯一');

console.log('car_debug 视频遥控页面契约通过');
