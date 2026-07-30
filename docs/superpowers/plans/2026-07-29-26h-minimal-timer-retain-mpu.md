# 26H Minimal Timer Port With MPU6050 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在最新 `main` 硬件映射上加入 KEY1 手动计时和 OLED 首行计时显示，同时完整保留 MPU6050/Heading、步进电机及新版引脚行为。

**Architecture:** `Accomplish/26H.c` 只保存计时状态并消费现有 `App_UpdateContext_t` 的 Tick 与按下沿。`main.c` 选择 26H 而不加载 25H Mission；`Application/Core/App.c` 保持不变，因此 MPU6050 和 Stepper 仍按最新主分支运行。`DebugDisplay.c` 保留开机 MPU 校准页，只替换正常页面第 0 行。

**Tech Stack:** C99、MSPM0G3507、TI Arm Clang 5.1.1 LTS、100 Hz SysTick、OLED 6x8 API、MinGW GCC 宿主测试、Node.js CommonJS 契约测试。

## Global Constraints

- 新增独立的 `Accomplish/26H.c` 和 `Accomplish/26H.h` 计时控制器。
- KEY1 第一次按下沿清零并开始计时，第二次按下沿停止并冻结，再次按下沿清零并开始新一轮。
- 使用 `App_UpdateContext_t::elapsedTicks` 的 100 Hz 整数节拍计时，分辨率为 10 ms，并在 `uint32_t` 上限处饱和。
- OLED 正常页面第 0 行显示 `T:<秒>.<百分秒>s`，停止后保持最终时间。
- `main.c` 只初始化和更新 26H 控制器，不加载 25H Mission；KEY1 不启动直流电机或步进电机。
- 保留最新 `main` 的 MPU6050、步进电机、串口、编码器和 PinMux 行为。
- `Application/Core/App.c/.h`、Heading、MPU6050、Stepper、Encoder、Serial、`main.syscfg` 和 `Accomplish/25H.c/.h` 不得修改。
- 用户选择最小移植：不新增 KEY1 软件消抖，不调整 KEY2/C0 与 CarLink 的现有顺序。
- 只在本地提交和合并，不访问或更新远端。

## File Map

- Create `Accomplish/26H.h`: KEY1 掩码与计时器公共 API。
- Create `Accomplish/26H.c`: 开始、累计、停止、冻结、重开与饱和逻辑。
- Create `tests/host/test_26h.c`: 不依赖硬件的计时状态测试。
- Modify `tests/host/run_tests.sh`: 加入 26H 宿主测试。
- Create `tests/test_26h_integration.cjs`: 入口、OLED、MPU 保留和硬件边界契约。
- Modify `main.c`: 从 25H Mission 切换到 26H 控制器。
- Modify `Application/Debug/DebugDisplay.c`: 保留 MPU 校准页，正常页第 0 行改为计时。
- Modify `README.md`: 最小更新当前入口、计时和 MPU 保留说明。

---

### Task 1: Start And Accumulate The 26H Timer

**Files:**
- Create: `tests/host/test_26h.c`
- Create: `Accomplish/26H.h`
- Create: `Accomplish/26H.c`
- Modify: `tests/host/run_tests.sh`

**Interfaces:**
- Consumes: `App_UpdateContext_t.elapsedTicks` and `App_UpdateContext_t.pressedEdges`.
- Produces: `Accomplish26H_Init(void)`, `Accomplish26H_Update(const App_UpdateContext_t *)`, `Accomplish26H_IsTiming(void)`, `Accomplish26H_GetElapsedTicks(void)`.

- [ ] **Step 1: Write the initial failing host test**

Create `tests/host/test_26h.c`:

```c
#include "Accomplish/26H.h"
#include "tests/host/test_assert.h"

static App_UpdateContext_t make_context(
    uint8_t elapsedTicks, uint8_t pressedEdges)
{
    App_UpdateContext_t context = {0};

    context.elapsedTicks = elapsedTicks;
    context.pressedEdges = pressedEdges;
    return context;
}

static void test_init_is_stopped_at_zero(void)
{
    Accomplish26H_Init();
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

static void test_first_key_edge_starts_without_counting_old_ticks(void)
{
    App_UpdateContext_t context = make_context(
        9U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_IsTiming() != 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

static void test_running_timer_accumulates_all_elapsed_ticks(void)
{
    App_UpdateContext_t context = make_context(
        1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    context = make_context(1U, 0U);
    Accomplish26H_Update(&context);
    context = make_context(37U, 0U);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetElapsedTicks() == 38U);
}

int main(void)
{
    test_init_is_stopped_at_zero();
    test_first_key_edge_starts_without_counting_old_ticks();
    test_running_timer_accumulates_all_elapsed_ticks();

    if (s_failures == 0)
    {
        printf("test_26h: ALL PASS\n");
        return 0;
    }
    printf("test_26h: %d FAILURE(S)\n", s_failures);
    return 1;
}
```

- [ ] **Step 2: Run the test and verify RED**

```powershell
New-Item -ItemType Directory -Force tests\host\build
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -I. tests\host\test_26h.c Accomplish\26H.c -o tests\host\build\test_26h.exe
```

Expected: compilation fails because `Accomplish/26H.h` and `Accomplish/26H.c` do not exist.

- [ ] **Step 3: Add the minimal implementation**

Create `Accomplish/26H.h`:

```c
#ifndef ACCOMPLISH_26H_H
#define ACCOMPLISH_26H_H

#include "Application/Core/App.h"
#include <stdint.h>

#define ACCOMPLISH_26H_START_STOP_KEY_MASK 0x01U

void Accomplish26H_Init(void);
void Accomplish26H_Update(const App_UpdateContext_t *context);
uint8_t Accomplish26H_IsTiming(void);
uint32_t Accomplish26H_GetElapsedTicks(void);

#endif
```

Create `Accomplish/26H.c`:

```c
#include "Accomplish/26H.h"
#include <stddef.h>

static uint32_t s_elapsedTicks;
static uint8_t s_timing;

void Accomplish26H_Init(void)
{
    s_elapsedTicks = 0U;
    s_timing = 0U;
}

void Accomplish26H_Update(const App_UpdateContext_t *context)
{
    if (context == NULL)
    {
        return;
    }

    if (s_timing == 0U)
    {
        if ((context->pressedEdges &
             ACCOMPLISH_26H_START_STOP_KEY_MASK) != 0U)
        {
            s_elapsedTicks = 0U;
            s_timing = 1U;
        }
        return;
    }

    s_elapsedTicks += context->elapsedTicks;
}

uint8_t Accomplish26H_IsTiming(void)
{
    return s_timing;
}

uint32_t Accomplish26H_GetElapsedTicks(void)
{
    return s_elapsedTicks;
}
```

Add after the heading test in `tests/host/run_tests.sh`:

```bash
echo "--- test_26h ---"
gcc $CFLAGS \
    "$ROOT/tests/host/test_26h.c" \
    "$ROOT/Accomplish/26H.c" \
    -o "$OUT/test_26h.exe"
"$OUT/test_26h.exe"
```

- [ ] **Step 4: Run the focused test and verify GREEN**

```powershell
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -I. tests\host\test_26h.c Accomplish\26H.c -o tests\host\build\test_26h.exe
.\tests\host\build\test_26h.exe
```

Expected: `test_26h: ALL PASS` and exit code 0.

- [ ] **Step 5: Commit locally**

```powershell
git add Accomplish/26H.h Accomplish/26H.c tests/host/test_26h.c tests/host/run_tests.sh
git commit -m "feat(26h): add race timer start and accumulation"
```

---

### Task 2: Stop, Freeze, Restart, And Saturate

**Files:**
- Modify: `tests/host/test_26h.c`
- Modify: `Accomplish/26H.c`

**Interfaces:**
- Consumes: Task 1 API unchanged.
- Produces: complete KEY1 toggle semantics and `UINT32_MAX` saturation.

- [ ] **Step 1: Add failing transition tests**

Add before `main()`:

```c
static void test_held_key_without_new_edge_does_not_toggle(void)
{
    App_UpdateContext_t context = make_context(
        1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    context = make_context(5U, 0U);
    context.pressedKeys = ACCOMPLISH_26H_START_STOP_KEY_MASK;
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_IsTiming() != 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 5U);
}

static void test_second_key_edge_stops_and_freezes(void)
{
    App_UpdateContext_t context = make_context(
        9U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    context = make_context(8U, ACCOMPLISH_26H_START_STOP_KEY_MASK);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 8U);

    context = make_context(255U, 0U);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetElapsedTicks() == 8U);
}

static void test_later_key_edge_resets_and_starts_new_run(void)
{
    App_UpdateContext_t context = make_context(
        1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    context = make_context(42U, 0U);
    Accomplish26H_Update(&context);
    context = make_context(1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);
    Accomplish26H_Update(&context);
    context = make_context(7U, ACCOMPLISH_26H_START_STOP_KEY_MASK);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_IsTiming() != 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

static void test_null_context_does_not_change_state(void)
{
    Accomplish26H_Init();
    Accomplish26H_Update(NULL);
    CHECK(Accomplish26H_IsTiming() == 0U);
    CHECK(Accomplish26H_GetElapsedTicks() == 0U);
}

static void test_tick_counter_saturates_instead_of_wrapping(void)
{
    App_UpdateContext_t context = make_context(
        1U, ACCOMPLISH_26H_START_STOP_KEY_MASK);
    uint32_t updateIndex;

    Accomplish26H_Init();
    Accomplish26H_Update(&context);
    context = make_context(UINT8_MAX, 0U);
    for (updateIndex = 0U;
         updateIndex < (UINT32_MAX / UINT8_MAX);
         updateIndex++)
    {
        Accomplish26H_Update(&context);
    }
    CHECK(Accomplish26H_GetElapsedTicks() == UINT32_MAX);

    context = make_context(1U, 0U);
    Accomplish26H_Update(&context);
    CHECK(Accomplish26H_GetElapsedTicks() == UINT32_MAX);
}
```

Replace `main()` with:

```c
int main(void)
{
    test_init_is_stopped_at_zero();
    test_first_key_edge_starts_without_counting_old_ticks();
    test_running_timer_accumulates_all_elapsed_ticks();
    test_held_key_without_new_edge_does_not_toggle();
    test_second_key_edge_stops_and_freezes();
    test_later_key_edge_resets_and_starts_new_run();
    test_null_context_does_not_change_state();
    test_tick_counter_saturates_instead_of_wrapping();

    if (s_failures == 0)
    {
        printf("test_26h: ALL PASS\n");
        return 0;
    }
    printf("test_26h: %d FAILURE(S)\n", s_failures);
    return 1;
}
```

- [ ] **Step 2: Run and verify RED**

```powershell
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -I. tests\host\test_26h.c Accomplish\26H.c -o tests\host\build\test_26h.exe
if ($LASTEXITCODE -ne 0) { throw '26H host-test compilation failed' }
& .\tests\host\build\test_26h.exe
$redExitCode = $LASTEXITCODE
if ($redExitCode -eq 0) {
  throw 'Expected Task 2 transition tests to fail before implementation'
}
```

Expected: the executable reports stop/freeze/restart or saturation assertion failures and returns nonzero because Task 1 never stops and can wrap; the PowerShell guard accepts that expected RED result.

- [ ] **Step 3: Replace `Accomplish26H_Update()` with the final implementation**

```c
void Accomplish26H_Update(const App_UpdateContext_t *context)
{
    uint8_t keyPressed;

    if (context == NULL)
    {
        return;
    }

    keyPressed = ((context->pressedEdges &
                   ACCOMPLISH_26H_START_STOP_KEY_MASK) != 0U) ?
        1U : 0U;

    if (s_timing == 0U)
    {
        if (keyPressed != 0U)
        {
            s_elapsedTicks = 0U;
            s_timing = 1U;
        }
        return;
    }

    if ((UINT32_MAX - s_elapsedTicks) < context->elapsedTicks)
    {
        s_elapsedTicks = UINT32_MAX;
    }
    else
    {
        s_elapsedTicks += context->elapsedTicks;
    }

    if (keyPressed != 0U)
    {
        s_timing = 0U;
    }
}
```

- [ ] **Step 4: Run and verify GREEN**

```powershell
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -I. tests\host\test_26h.c Accomplish\26H.c -o tests\host\build\test_26h.exe
if ($LASTEXITCODE -ne 0) { throw '26H host-test compilation failed' }
& .\tests\host\build\test_26h.exe
if ($LASTEXITCODE -ne 0) { throw '26H host tests failed' }
```

Expected: `test_26h: ALL PASS` and exit code 0.

- [ ] **Step 5: Commit locally**

```powershell
git add Accomplish/26H.c tests/host/test_26h.c
git commit -m "feat(26h): add manual timer stop and restart"
```

---

### Task 3: Select 26H And Render Time While Retaining MPU6050

**Files:**
- Create: `tests/test_26h_integration.cjs`
- Modify: `main.c`
- Modify: `Application/Debug/DebugDisplay.c`

**Interfaces:**
- Consumes: final `Accomplish26H_*` API and `TICK_HZ`.
- Produces: 26H runtime entry and OLED string `T:<seconds>.<centiseconds>s` while retaining the calibration page.

- [ ] **Step 1: Write the failing integration contract**

Create `tests/test_26h_integration.cjs`:

```javascript
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');

const root = join(__dirname, '..');
const read = (path) => readFileSync(join(root, path), 'utf8');

const main = read('main.c');
const app = read('Application/Core/App.c');
const display = read('Application/Debug/DebugDisplay.c');
const displayHeader = read('Application/Debug/DebugDisplay.h');

assert.match(main, /#include "Accomplish\/26H\.h"/,
    'main.c 必须选择 26H 控制器');
assert.match(main, /Accomplish26H_Init\(\);/,
    'main.c 必须初始化 26H 控制器');
assert.match(main, /Accomplish26H_Update\(&updateContext\);/,
    'main.c 必须在有效 App 更新后推进 26H 控制器');
assert.doesNotMatch(main, /Accomplish\/25H\.h|Mission_Init\(|Mission_Update\(/,
    '26H 第一版不能启动旧 25H Mission');

for (const required of [
    '#include "Application/State/Heading.h"',
    'Heading_Init();',
    'DebugDisplay_ShowHeadingCalibration(Heading_IsReady());',
    'Heading_Calibrate();',
    'Heading_Update(context->dt);',
    'Stepper_Init();',
    'Stepper_Update(elapsedTicks);',
    'Stepper_EmergencyStop();',
]) {
    assert.ok(app.includes(required), `App 必须保留 ${required}`);
}

assert.match(display, /#include "Accomplish\/26H\.h"/,
    'OLED 必须读取 26H 计时器');
assert.match(display, /Accomplish26H_GetElapsedTicks\(\)/,
    'OLED 必须读取整数累计节拍');
assert.ok(display.includes('"T:%lu.%02lus"'),
    'OLED 首行必须显示秒和百分秒');
assert.doesNotMatch(display,
    /OLED_ShowString\(0,\s*0,\s*"Z:"/,
    'OLED 正常页面第 0 行不能继续显示 Z 角');
assert.ok(display.includes('#include "Application/State/Heading.h"'),
    '显示模块必须保留 Heading 以显示 MPU 校准页');
assert.ok(display.includes('void DebugDisplay_ShowHeadingCalibration'),
    '显示模块必须保留 MPU 校准页实现');
assert.ok(displayHeader.includes('DebugDisplay_ShowHeadingCalibration'),
    '显示头文件必须保留 MPU 校准页 API');

console.log('26H 入口、MPU 保留与计时显示契约通过');
```

- [ ] **Step 2: Run and verify RED**

```powershell
node tests\test_26h_integration.cjs
```

Expected: assertion `main.c 必须选择 26H 控制器` fails because `main.c` still selects 25H.

- [ ] **Step 3: Replace `main.c` with the 26H entry**

```c
#include "Accomplish/26H.h"
#include "Application/Core/App.h"
#include "System/Interrupt.h"

int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Accomplish26H_Init();
    Interrupt_Enable();

    for (;;)
    {
        if (App_Update(&updateContext) != 0U)
        {
            Accomplish26H_Update(&updateContext);
        }
    }
}
```

- [ ] **Step 4: Add elapsed-time rendering without removing Heading**

In `Application/Debug/DebugDisplay.c`, add:

```c
#include "Accomplish/26H.h"
#include "System/Tick.h"
```

Keep `#include "Application/State/Heading.h"`, `DebugDisplay_ShowHeadingCalibration()` and its declaration in `DebugDisplay.h` unchanged.

Add after `DebugDisplay_ShowHeadingCalibration()`:

```c
static void DebugDisplay_ShowElapsedTime(void)
{
    uint32_t elapsedTicks = Accomplish26H_GetElapsedTicks();
    uint32_t seconds = elapsedTicks / TICK_HZ;
    uint32_t centiseconds =
        ((elapsedTicks % TICK_HZ) * 100U) / TICK_HZ;

    OLED_Printf(0, 0, OLED_6X8, "T:%lu.%02lus",
                (unsigned long)seconds,
                (unsigned long)centiseconds);
}
```

Replace only the normal-page `Z:`/`Heading_GetYaw()` block in `DebugDisplay_Update()` with:

```c
    DebugDisplay_ShowElapsedTime();
```

- [ ] **Step 5: Run integration and behavior tests**

```powershell
node tests\test_26h_integration.cjs
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -I. tests\host\test_26h.c Accomplish\26H.c -o tests\host\build\test_26h.exe
.\tests\host\build\test_26h.exe
```

Expected: `26H 入口、MPU 保留与计时显示契约通过` and `test_26h: ALL PASS`.

- [ ] **Step 6: Prove protected firmware files are unchanged**

```powershell
git diff --exit-code c6b7208 -- Application/Core/App.c Application/Core/App.h Application/State/Heading.c Application/State/Heading.h Hardware/Sensors/MPU6050.c Hardware/Sensors/MPU6050.h Hardware/Motor/Stepper.c Hardware/Motor/Stepper.h Hardware/Motor/Encoder.c Hardware/Motor/Encoder.h Hardware/Comms/Serial.c Hardware/Comms/Serial.h main.syscfg Accomplish/25H.c Accomplish/25H.h
```

Expected: no output and exit code 0.

- [ ] **Step 7: Commit locally**

```powershell
git add main.c Application/Debug/DebugDisplay.c tests/test_26h_integration.cjs
git commit -m "feat(26h): integrate timer while retaining mpu"
```

---

### Task 4: Document The Active Entry And Verify The Full Firmware

**Files:**
- Modify: `tests/test_26h_integration.cjs`
- Modify: `README.md`

**Interfaces:**
- Consumes: Tasks 1-3 behavior.
- Produces: operator documentation and complete host/target verification evidence.

- [ ] **Step 1: Add failing README contract assertions**

In `tests/test_26h_integration.cjs`, read README and add:

```javascript
const readme = read('README.md');

assert.match(readme, /26H 手动计时/,
    'README 必须标明当前 26H 手动计时入口');
assert.match(readme, /App_Init\(\).*继续初始化并校准 MPU6050/,
    'README 必须明确 MPU6050 继续初始化和校准');
assert.match(readme, /App_Update\(\).*继续更新 Heading/,
    'README 必须明确 Heading 继续周期更新');
assert.match(readme, /T:<秒>\.<百分秒>s/,
    'README 必须记录 OLED 首行计时格式');
assert.match(readme, /第一版不启动.*电机/,
    'README 必须说明 KEY1 不启动车辆');
```

```powershell
node tests\test_26h_integration.cjs
```

Expected: README assertion failure because current documentation still selects 25H and Z-angle display.

- [ ] **Step 2: Minimally update README current-mode text**

Keep every current stepper, serial and pin table value. Update only active-entry statements so they include these exact facts:

```markdown
### 3.2 100 Hz 主循环与 26H 手动计时

当前加载 `Accomplish/26H.c` 的独立控制器。第一版不启动直流电机或步进电机：KEY1 第一次按下沿把累计 Tick 清零并开始计时，第二次按下沿停止并冻结结果，再次按下则从零开始新一轮。计时使用 100 Hz 整数系统节拍，分辨率为 10 ms。

`App_Init()` 继续初始化并校准 MPU6050，`App_Update()` 继续更新 Heading；保留的航向运动和调试命令仍可使用。正常 OLED 页面第 0 行改为 `T:<秒>.<百分秒>s`，开机 MPU6050 校准页面继续保留。
```

Replace the active `main.c` example with Task 3's exact source. Update OLED row 0 to:

```markdown
| 0 | 26H 手动计时 `T:<秒>.<百分秒>s`；运行时递增，停止后保持最终成绩 |
```

Update only current file/API indexes to identify `Accomplish/26H.c/.h` as active and `Accomplish/25H.c/.h` as retained. Do not alter any stepper, K230, UART, encoder, MPU pin or `main.syscfg` description.

- [ ] **Step 3: Run README and focused behavior contracts**

```powershell
node tests\test_26h_integration.cjs
if ($LASTEXITCODE -ne 0) { throw '26H integration contract failed' }
.\tests\host\build\test_26h.exe
if ($LASTEXITCODE -ne 0) { throw '26H host tests failed' }
```

Expected: both exit 0 with their success messages.

- [ ] **Step 4: Run all host and Node tests freshly**

Compile and run:

```powershell
New-Item -ItemType Directory -Force tests\host\build | Out-Null
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -I. tests\host\test_k230link_lane.c tests\host\stubs.c Application\Comms\K230Link.c -o tests\host\build\test_k230link_lane.exe
if ($LASTEXITCODE -ne 0) { throw 'test_k230link_lane compilation failed' }
.\tests\host\build\test_k230link_lane.exe
if ($LASTEXITCODE -ne 0) { throw 'test_k230link_lane failed' }
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -I. tests\host\test_heading.c tests\host\stubs.c Application\State\Heading.c -o tests\host\build\test_heading.exe
if ($LASTEXITCODE -ne 0) { throw 'test_heading compilation failed' }
.\tests\host\build\test_heading.exe
if ($LASTEXITCODE -ne 0) { throw 'test_heading failed' }
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -I. tests\host\test_motionlane.c tests\host\stubs.c Application\Comms\K230Link.c Application\Control\MotionLane.c Application\State\Heading.c -o tests\host\build\test_motionlane.exe
if ($LASTEXITCODE -ne 0) { throw 'test_motionlane compilation failed' }
.\tests\host\build\test_motionlane.exe
if ($LASTEXITCODE -ne 0) { throw 'test_motionlane failed' }
gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -I. tests\host\test_26h.c Accomplish\26H.c -o tests\host\build\test_26h.exe
if ($LASTEXITCODE -ne 0) { throw 'test_26h compilation failed' }
.\tests\host\build\test_26h.exe
if ($LASTEXITCODE -ne 0) { throw 'test_26h failed' }
node tests\test_car_debug_video_control.cjs
if ($LASTEXITCODE -ne 0) { throw 'car_debug video contract failed' }
node tests\test_26h_integration.cjs
if ($LASTEXITCODE -ne 0) { throw '26H integration contract failed' }
```

Expected: four host `ALL PASS` lines and two Node success messages.

- [ ] **Step 5: Compile changed target units with TI Arm Clang**

Create `.superpowers/sdd/target-build` and compile `Accomplish/26H.c`, `main.c`, `Application/Core/App.c`, and `Application/Debug/DebugDisplay.c` from the isolated worktree root. The generated configuration header and device options come from the current clean `main` Debug build, while `-I.` keeps worktree project headers first:

```powershell
$targetCompiler = 'D:\Program Files (x86)\TI\ccs\tools\compiler\ti-cgt-armllvm_5.1.1.LTS\bin\tiarmclang.exe'
$targetOptions = @(
  '-c', '@D:\real\total\car_msg3507\Debug\device.opt',
  '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft',
  '-mlittle-endian', '-mthumb', '-O2', '-I.',
  '-ID:\real\total\car_msg3507\Debug',
  '-IC:\TI\mspm0_sdk_2_10_00_04\source\third_party\CMSIS\Core\Include',
  '-IC:\TI\mspm0_sdk_2_10_00_04\source', '-gdwarf-3', '-Wall'
)
New-Item -ItemType Directory -Force .superpowers\sdd\target-build | Out-Null
$targetUnits = @(
  @{ Source = 'Accomplish\26H.c'; Output = '26H.o' },
  @{ Source = 'main.c'; Output = 'main.o' },
  @{ Source = 'Application\Core\App.c'; Output = 'App.o' },
  @{ Source = 'Application\Debug\DebugDisplay.c'; Output = 'DebugDisplay.o' }
)
foreach ($targetUnit in $targetUnits) {
  $targetObject = Join-Path '.superpowers\sdd\target-build' $targetUnit.Output
  & $targetCompiler @targetOptions '-o' $targetObject $targetUnit.Source
  if ($LASTEXITCODE -ne 0) {
    throw "TI compile failed: $($targetUnit.Source)"
  }
}
$targetUnits | ForEach-Object {
  $targetObject = Get-Item -LiteralPath (Join-Path '.superpowers\sdd\target-build' $_.Output)
  if ($targetObject.Length -eq 0) {
    throw "Empty target object: $($targetObject.FullName)"
  }
}
```

Expected: all four objects compile with exit code 0.

- [ ] **Step 6: Link a complete target image**

Generate a response file that replaces the three changed baseline objects, adds `26H.o`, and retains every unchanged object and TI linker input from `D:\real\total\car_msg3507\Debug\ccsObjs.opt`:

```powershell
$worktreeRoot = (git rev-parse --show-toplevel).Replace('\', '/')
$baselineBuild = 'D:/real/total/car_msg3507/Debug'
$targetBuild = "$worktreeRoot/.superpowers/sdd/target-build"
$baselineObjects = Get-Content -Encoding ASCII -LiteralPath "$baselineBuild/ccsObjs.opt"
$unchangedObjects = $baselineObjects | Where-Object {
  $_ -notin @(
    '"./main.o"',
    '"./Application/Core/App.o"',
    '"./Application/Debug/DebugDisplay.o"'
  )
}
$changedObjects = @(
  '"' + $targetBuild + '/main.o"',
  '"' + $targetBuild + '/App.o"',
  '"' + $targetBuild + '/DebugDisplay.o"',
  '"' + $targetBuild + '/26H.o"'
)
$responseFile = "$targetBuild/ccsObjs26-mpu.opt"
Set-Content -Encoding ASCII -LiteralPath $responseFile `
  -Value ($changedObjects + $unchangedObjects)
```

Then link from the baseline Debug directory so the relative unchanged objects, `device_linker.cmd`, and generated libraries resolve exactly as in the CCS build:

```powershell
$outputFile = "$targetBuild/ArcLineTest-26H-MPU.out"
$linkOptions = @(
  '@device.opt', '-march=thumbv6m', '-mcpu=cortex-m0plus',
  '-mfloat-abi=soft', '-mlittle-endian', '-mthumb', '-O2',
  '-gdwarf-3', '-Wall',
  "-Wl,-m$targetBuild/ArcLineTest-26H-MPU.map",
  '-Wl,-iC:/TI/mspm0_sdk_2_10_00_04/source',
  "-Wl,-i$worktreeRoot",
  '-Wl,-iD:/real/total/car_msg3507/Debug/syscfg',
  '-Wl,-iD:/Program Files (x86)/TI/ccs/tools/compiler/ti-cgt-armllvm_5.1.1.LTS/lib',
  '-Wl,--diag_wrap=off', '-Wl,--display_error_number',
  '-Wl,--warn_sections',
  "-Wl,--xml_link_info=$targetBuild/ArcLineTest-26H-MPU_linkInfo.xml",
  '-Wl,--rom_model', '-o', $outputFile, "@$responseFile"
)
Push-Location $baselineBuild
try {
  & $targetCompiler @linkOptions
  if ($LASTEXITCODE -ne 0) {
    throw 'TI target link failed'
  }
}
finally {
  Pop-Location
}
$targetImage = Get-Item -LiteralPath $outputFile
if ($targetImage.Length -eq 0) {
  throw "Empty target image: $outputFile"
}
$targetImage | Select-Object FullName, Length, LastWriteTime
```

Expected: exit code 0 and a fresh non-empty target image.

- [ ] **Step 7: Verify exact scope**

```powershell
git diff --exit-code c6b7208 -- Application/Core/App.c Application/Core/App.h Application/State/Heading.c Application/State/Heading.h Hardware/Sensors/MPU6050.c Hardware/Sensors/MPU6050.h Hardware/Motor/Stepper.c Hardware/Motor/Stepper.h Hardware/Motor/Encoder.c Hardware/Motor/Encoder.h Hardware/Comms/Serial.c Hardware/Comms/Serial.h main.syscfg Accomplish/25H.c Accomplish/25H.h
git diff --check c6b7208
git status --short --branch
```

Expected: protected-file diff is empty, whitespace check is empty, and the only pending tracked change before commit is README/test documentation work.

- [ ] **Step 8: Commit documentation and final evidence**

```powershell
git add README.md tests/test_26h_integration.cjs
git commit -m "docs: document 26H timer with mpu retained"
```

After commit, run:

```powershell
node tests\test_26h_integration.cjs
if ($LASTEXITCODE -ne 0) { throw '26H integration contract failed' }
.\tests\host\build\test_26h.exe
if ($LASTEXITCODE -ne 0) { throw '26H host tests failed' }
git diff --check c6b7208..HEAD
if ($LASTEXITCODE -ne 0) { throw 'Whitespace check failed' }
git status --short --branch
```

Expected: both tests exit 0, the whitespace check has no output, and status is clean on `codex/26h-timer-retain-mpu`. Do not push.
