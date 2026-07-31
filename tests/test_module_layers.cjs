const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..');
const fromRoot = (...parts) => path.join(root, ...parts);

const accomplishFiles = fs.readdirSync(fromRoot('Accomplish'))
    .filter((file) => /\.(c|h)$/.test(file))
    .sort();

assert.deepEqual(accomplishFiles, [
    '25E.c', '25E.h', '25H.c', '25H.h', '26H.c', '26H.h',
    'Brushless_Motor_Test.c', 'Brushless_Motor_Test.h',
], 'Accomplish retains legacy tasks, but 26H support controllers move out');

[
    'Application/Control/BallSequence.c',
    'Application/Control/BallSequence.h',
    'Application/Control/BallHold.c',
    'Application/Control/BallHold.h',
    'Application/Control/BallTargetCapture.c',
    'Application/Control/BallTargetCapture.h',
].forEach((relativePath) => {
    assert.ok(fs.existsSync(fromRoot(relativePath)),
        `${relativePath} must be in its layer`);
});

console.log('module layer contract passed');
