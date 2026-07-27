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

const ids = [...html.matchAll(/\bid="([^"]+)"/g)].map((match) => match[1]);
const duplicateIds = ids.filter((id, index) => ids.indexOf(id) !== index);
assert.deepEqual([...new Set(duplicateIds)], [], 'HTML id 必须唯一');

console.log('car_debug 视频遥控页面契约通过');
