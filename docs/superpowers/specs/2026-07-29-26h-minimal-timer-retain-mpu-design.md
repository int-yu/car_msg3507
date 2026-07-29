# 26H 计时器最小移植并保留 MPU6050 设计

## 背景

旧的 `codex/26h-manual-timer` 分支从 `a2defa2` 分出，包含 KEY1 手动计时、OLED 首行计时显示，以及停用 MPU6050/Heading 生命周期的改动。当前 `main` 已推进到 `c6b7208`，新增 MS42CG 步进驱动、K230 串口迁移和多处引脚重映射，并且主工作区的 `Application/Debug/DebugDisplay.c` 还有未提交的步进状态显示修改。

本次不整体合并旧分支，而是在最新 `main` 上最小移植计时功能。MPU6050 继续初始化、校准和周期更新；新版步进、串口、编码器和引脚配置全部保留。

## 目标

- 新增独立的 `Accomplish/26H.c` 和 `Accomplish/26H.h` 计时控制器。
- KEY1 第一次按下沿清零并开始计时，第二次按下沿停止并冻结，再次按下沿清零并开始新一轮。
- 使用 `App_UpdateContext_t::elapsedTicks` 的 100 Hz 整数节拍计时，分辨率为 10 ms，并在 `uint32_t` 上限处饱和。
- OLED 正常页面第 0 行显示 `T:<秒>.<百分秒>s`，停止后保持最终时间。
- `main.c` 只初始化和更新 26H 控制器，不加载 25H Mission；KEY1 不启动直流电机或步进电机。
- 保留最新 `main` 的 MPU6050、步进电机、串口、编码器和 PinMux 行为。
- 只在本地提交和合并，不访问或更新远端。

## 最小移植边界

### 新增文件

- `Accomplish/26H.c`
- `Accomplish/26H.h`
- `tests/host/test_26h.c`
- `tests/test_26h_integration.cjs`

### 修改文件

- `main.c`：把 25H Mission 入口替换为 26H 计时控制器入口。
- `Application/Debug/DebugDisplay.c`：保留 MPU6050 校准页面和 Heading 依赖，只把正常页面第 0 行从 `Z:` 改为计时。
- `tests/host/run_tests.sh`：加入 26H 宿主测试。
- `README.md`：只更新当前入口、计时行为和 OLED 第 0 行的必要说明，不重写硬件映射章节。

### 明确不修改

- `Application/Core/App.c/.h`
- `Application/State/Heading.c/.h`
- `Hardware/Sensors/MPU6050.c/.h`
- `Hardware/Motor/Stepper.c/.h`
- `Hardware/Motor/Encoder.c/.h`
- `Hardware/Comms/Serial.c/.h`
- `main.syscfg`
- `Accomplish/25H.c/.h`

因此 `App_Init()` 仍调用 `Heading_Init()`、显示 MPU6050 校准页并执行 `Heading_Calibrate()`，`App_Update()` 仍调用 `Heading_Update()`；同时保留 `Stepper_Init()`、`Stepper_Update()` 和步进急停。

## 计时状态

计时器保存 `uint32_t` 累计节拍和运行标志，只消费 App 已生成的 KEY1 `pressedEdges`：

1. 停止状态收到 KEY1 按下沿：累计值清零并进入运行状态，本次更新携带的旧节拍不计入新一轮。
2. 运行状态每次更新：累加 `elapsedTicks`，溢出前饱和到 `UINT32_MAX`。
3. 运行状态收到 KEY1 按下沿：先累加本次节拍，再停止并冻结结果。
4. 停止状态的普通更新不改变累计值。

用户选择最小移植，因此本次不新增 KEY1 软件消抖，也不调整现有 KEY2/C0 与 CarLink 命令的处理顺序。它们作为已知限制保留，不混入本次计时移植。

## OLED 与 MPU6050

开机时仍保留 `DebugDisplay_ShowHeadingCalibration()` 的 MPU6050 在线/校准页面。进入正常刷新后，第 0 行不再显示 Z 角，而是读取 `Accomplish26H_GetElapsedTicks()` 并按 `TICK_HZ` 转换为秒和百分秒。

其余 OLED 行以最新主分支为准。主工作区未提交的步进显示修改，包括 `Stepper_GetStatus()`、`EC:` 和 `EA:`，必须在最终合并后继续存在，且不能混入计时功能提交。

## 主工作区未提交改动保护

实现和验证在从 `main@c6b7208` 创建的隔离工作树中完成。最终合并前：

1. 记录主工作区状态并保存 `Application/Debug/DebugDisplay.c` 的完整补丁作为恢复凭据。
2. 只临时收起该文件的未提交修改，使 `main` 可以合并计时分支。
3. 本地合并后重新应用该修改；若 OLED 同一代码块发生冲突，最终内容必须同时保留计时首行和步进状态行。
4. 对比恢复前后的步进修改，确认 `Stepper.h`、`Stepper_GetStatus()`、`EC:` 和 `EA:` 均未丢失后，才清理临时备份。

最终 `git diff` 应继续只显示用户原有的未提交步进 OLED 修改；计时功能本身应位于已提交历史中。

## 测试与验收

- 26H 宿主测试覆盖初始化、开始、批量节拍累计、停止冻结、再次开始、空上下文和计数饱和。
- Node 集成契约确认 `main.c` 选择 26H、不加载 25H Mission，OLED 首行显示计时，并且 App 仍保留 MPU6050/Heading 初始化、校准和更新调用。
- 运行全部现有宿主测试和网页契约测试。
- 用 TI Arm Clang 编译 `26H.c`、`main.c` 和 `DebugDisplay.c`，并链接完整目标固件。
- 使用范围检查确认 `main.syscfg`、Stepper、Serial、Encoder、Heading、MPU6050 和 25H 文件与最新 `main` 基线无差异。
- 合并后在主工作区重新运行测试和目标构建，并确认未提交的步进 OLED 修改仍然存在。

## 非目标与后续

- 本次不让 KEY1 同步启动巡线或任何电机。
- 本次不自动识别 A 点或自动停止计时。
- 本次不新增按键消抖，也不重构全局急停顺序。
- 本次不恢复 OLED 正常页面上的 Z 角显示；MPU6050 仍在后台提供 Heading 数据给保留的运动和调试功能。
