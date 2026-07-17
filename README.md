# MSPM0G3507 天猛星小车工程索引

> **强制同步规则**
>
> 对项目进行任何修改时，只要涉及硬件连接、Pin 口、外设实例、时钟树、波特率、定时器、主循环调用关系或程序功能，就必须同时修改本文的 **Pin 口表、时钟表和对应程序说明**。禁止只修改代码而不更新本文档，否则后续检查无法判断接线、时钟和程序是否一致。
>
> `main.syscfg` 是引脚和外设配置的唯一源文件。`Debug/`、`Release/`、`ti_msp_dl_config.*`、`device.opt` 和 `device_linker.cmd` 均为生成内容，不手工维护；更改源码目录或 `main.syscfg` 后应执行 SysConfig，并在 CCS 中 Clean/Rebuild。

## 1. 时钟与定时资源占用

| 时钟/外设资源 | 当前配置 | 占用模块 | 用途 |
|---|---:|---|---|
| CPUCLK / SYSCLK | 32 MHz | 全工程 | SysConfig 默认时钟源；延时、总线外设和 SysTick 的基准时钟 |
| SysTick | 32 MHz / 320000 = 100 Hz | `System/Tick`、`Application/Core/App`、`MotionManager`、`Mission`、`Accomplish/25H` | 10 ms 系统节拍；`App_Update()` 传递实际累计 Tick 和 dt；当前巡线连续全白 50 拍（约 500 ms）后安全返回等待 |
| TIMG8 | 32 MHz，周期 1600 = 20 kHz | `Hardware/Motor/PWM`、`MotionWheel`、`MotionManager`、`Accomplish/25H` | 25H 通过 MotionManager 依次选择巡线、定距直线和绝对角转向，再由 MotionWheel 输出双轮 PWM |
| TIMA0 | BUSCLK / 32 = 1 MHz，周期 20000 = 50 Hz | `Application/Servo` | 双路舵机 PWM；1 个计数等于 1 us |
| I2C0 | BUSCLK 32 MHz，SCL 400 kHz | `Hardware/Display/OLED` | OLED 控制器通信 |
| UART1 | BUSCLK 32 MHz，9600 baud，8N1，RX 中断 | `Hardware/Comms/Serial`、`Application/Comms/BluetoothDebug`、`App`、`Mission` | `C0` 全局停车；保留其他任务事件和手动调试命令，当前 25H 使用 KEY1 启动 |
| UART2 | BUSCLK 32 MHz，115200 baud，8N1，当前不启用 RX NVIC | `main.syscfg` 预留 | K230 硬件配置保留；当前 App 不调用 `K230Link_Init/Update()`，不发送握手帧 |
| GPIO 软件 I2C | CPU 延时产生时序 | `Hardware/Sensors/MPU6050`、`Application/State/Heading`、`Accomplish/25H` | 提供连续多圈航向；25H 保存 KEY1 启动基准，左转绝对目标依次为基准减 90°、180°、270°……；不占用 I2C 外设实例 |
| GPIOA GROUP1 IRQ | A/B 相双边沿 | `Hardware/Motor/Encoder`、`MotionWheel` | 左右编码器软件正交解码，为公共双轮速度 PI 提供速度反馈 |

## 2. Pin 口占用

| Pin | 方向/复用 | 占用对象 | 程序映射与说明 |
|---|---|---|---|
| PA10 | 开漏式 GPIO | MPU6050 SCL | 软件 I2C 时钟；用于连续航向显示和运动闭环；25H 记录 KEY1 启动航向并生成连续绝对左转目标 |
| PA11 | 开漏式 GPIO | MPU6050 SDA | 软件 I2C 数据；25H 绝对目标依次为启动航向减 90°、180°、270°…… |
| PA12 | GPIO 输出 | 右电机 AIN2 | TB6612 A 通道方向；由 MotionManager 当前模式经 MotionWheel 输出 |
| PA13 | GPIO 输出 | 右电机 AIN1 | TB6612 A 通道方向；由 MotionManager 当前模式经 MotionWheel 输出 |
| PA14 | GPIO 输入、上拉 | 灰度 CH3 | `Graydetect` 位图 bit3，右内侧，检测黑线为 1；25H 巡线权重 `+3` |
| PA15 | GPIO 输入、上拉、双边沿中断 | 右编码器 A | GPIOA GROUP1 IRQ；`MotionWheel` 右轮反馈 |
| PA16 | GPIO 输入、上拉、双边沿中断 | 右编码器 B | GPIOA GROUP1 IRQ；`MotionWheel` 右轮反馈 |
| PA17 | GPIO 输入、上拉、双边沿中断 | 左编码器 A | GPIOA GROUP1 IRQ；`MotionWheel` 左轮反馈 |
| PA19 | SWDIO | 下载调试 | 不作为普通 GPIO 使用 |
| PA20 | SWCLK | 下载调试 | 不作为普通 GPIO 使用 |
| PA21 | UART2 TX | K230 RX 预留 | 当前 App 不发送数据；恢复 K230Link 时连接 K230 GPIO4（RX） |
| PA22 | UART2 RX | K230 TX 预留 | 当前 App 不启用接收中断；恢复 K230Link 时连接 K230 GPIO3（TX） |
| PA24 | GPIO 输入、上拉、双边沿中断 | 左编码器 B | GPIOA GROUP1 IRQ；`MotionWheel` 左轮反馈 |
| PA28 | I2C0 SDA | OLED | 400 kHz |
| PA30 | GPIO 输入、上拉 | KEY1 | 低电平按下，按键位图 bit0；App 生成按下沿事件，当前用于启动 25H |
| PA31 | I2C0 SCL | OLED | 400 kHz |
| PB0 | GPIO 输出 | 左电机 BIN1 | TB6612 B 通道方向；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB1 | GPIO 输出 | 左电机 BIN2 | TB6612 B 通道方向；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB6 | UART1 TX | 蓝牙 | MCU 发送到蓝牙 RX |
| PB7 | UART1 RX、上拉 | 蓝牙 | 蓝牙 TX 发送到 MCU，RX 中断接收；`C0` 全局停车，`C1` 当前未绑定 25H |
| PB8 | TIMA0 CCP0 | 横向舵机 | `D` 命令，`Servo_SetHorizontalAngle()` |
| PB9 | TIMA0 CCP1 | 纵向舵机 | `O` 命令，`Servo_SetVerticalAngle()` |
| PB10 | GPIO 输入、上拉 | KEY4 | 低电平按下，按键位图 bit3；App 生成按下沿事件，具体含义由 Mission 决定 |
| PB11 | GPIO 输入、上拉 | KEY2 | 低电平按下，按键位图 bit1；App 生成按下沿事件，具体含义由 Mission 决定 |
| PB14 | GPIO 输入、上拉 | KEY3 | 低电平按下，按键位图 bit2；App 生成按下沿事件，具体含义由 Mission 决定 |
| PB15 | TIMG8 CCP0 | 右电机 PWM | TB6612 A 通道，20 kHz；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB16 | TIMG8 CCP1 | 左电机 PWM | TB6612 B 通道，20 kHz；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB17 | GPIO 输出 | 蜂鸣器 | 低电平有效 |
| PB18 | GPIO 输入、上拉 | 灰度 CH4 | `Graydetect` 位图 bit4，右最外侧，检测黑线为 1；25H 巡线权重 `+6` |
| PB20 | GPIO 输入、上拉 | 灰度 CH2 | `Graydetect` 位图 bit2，中间，检测黑线为 1；25H 巡线权重 `0` |
| PB23 | GPIO 输出 | LED1 | 高电平点亮 |
| PB24 | GPIO 输入、上拉 | 灰度 CH1 | `Graydetect` 位图 bit1，左内侧，检测黑线为 1；25H 巡线权重 `-3`，与 bit0 同时为 1 时触发标志 |
| PB25 | GPIO 输入、上拉 | 灰度 CH0 | `Graydetect` 位图 bit0，最左侧，检测黑线为 1；25H 巡线权重 `-6`，与 bit1 同时为 1 时触发标志 |
| PB27 | GPIO 输出 | LED2 | 高电平点亮；蜂鸣提示同步使用 |

## 3. 当前程序调用关系

### 3.1 启动阶段

```c
App_Init();
Mission_Init(Accomplish25H_GetMissionGraph());
Interrupt_Enable();
```

1. `App_Init()` 先关闭全局中断，应用 SysConfig，然后初始化 Tick、板级器件、灰度、电机、舵机、UART1、里程和 OLED。
2. 初始化 MPU6050 并阻塞执行 400 次 Z 轴零漂采样；标定期间小车必须静止。
3. 标定后清除 SysTick 待处理状态和编码器累计值，初始化蓝牙解析器和 MotionManager。
4. `MotionManager_Init()` 统一初始化 MotionStraight、MotionLine 和 Nav；三个模块仍共用唯一 MotionWheel。
5. `Accomplish25H_GetMissionGraph()` 返回 25H 的静态状态图；`Mission_Init()` 校验节点数、起始/错误状态和所有转换目标后进入等待状态。
6. `Interrupt_Enable()` 最后开启全局硬件中断。UART、编码器和 SysTick ISR 只采集数据，不执行 Mission 或运动切换。

K230 握手继续关闭：App 不调用 `K230Link_Init/Update()`。K230Link 源码和 UART2 SysConfig 配置保留。

### 3.2 100 Hz 主循环与 Mission

```c
if (App_Update(&updateContext) != 0U)
{
    Mission_Update(&updateContext);
}
```

`App_Update()` 无 Tick 时执行 `__WFI()`；有 Tick 时按真实累计值生成 `elapsedTicks` 和 `dt`，随后执行：

```text
Heading -> Odometry -> 按键边沿 -> BluetoothDebug
        -> C0 全局停车 -> MotionManager -> Beep -> OLED
        -> Mission_Update
```

Mission 使用静态状态图，不使用 `malloc`。每个状态含 `onEnter/onUpdate/onExit`、有序转换表和 `interruptible`。动作运行时只检查打断转换；动作完成后只检查正常转换；每拍最多转换一次。被打断状态调用退出回调并停车，不保存或恢复原进度。

当前加载 `Accomplish/25H.c` 的静态状态图。KEY1 按下沿启动 200 mm/s 巡线，并保存 MPU6050 连续航向作为 0°基准；最左侧 bit0 与左内侧 bit1 同时为 1 时立即打断巡线，向前直行 150 mm 并减速至零，再调用 `MotionManager_TurnTo()` 指向下一绝对左转目标。目标依次为启动航向减 90°、180°、270°……；到角后继续巡线并循环。巡线连续全白 50 拍时停车并返回等待；`C0` 始终能够停车、返回等待并清除航向基准。

OLED 每 10 个系统节拍刷新一次，即 10 Hz。页面内容为：

| 行 | 显示内容 |
|---|---|
| 0 | MPU6050 解算的 Z 轴连续累计角度，不做 ±180° 归一化，可显示多圈转角 |
| 1 | 五路灰度 `CH0 CH1 CH2 CH3 CH4`，用 `0/1` 显示 ，1代表检测到黑色，0代表检测到白色|
| 2 | 四个按键 `KEY1 KEY2 KEY3 KEY4`，`1` 表示按下 |
| 3 | 左轮累计路程 `LD`，单位 mm |
| 4 | 左轮编码器实测速度 `LV`，单位 mm/s |
| 5 | 右轮累计路程 `RD`，单位 mm |
| 6 | 右轮编码器实测速度 `RV`，单位 mm/s |
| 7 | 统一运动状态：`M:IDLE`、`M:LINE`、`M:STRAIGHT`、`M:TURN` 或 `M:ERROR` |

K230Link 库保留的统一帧格式如下，当前 App 不调用：

```text
AA 55 | VER | TYPE | SEQ | LEN | PAYLOAD | CRC8
```

- `VER=0x01`；`TYPE` 为 `READY=0x01`、`READY_ACK=0x02` 或 `TARGET=0x10`。
- `SEQ` 为 8 位帧序号，`LEN` 最大为 32。
- CRC 使用 CRC-8/ATM，多项式 `0x07`、初始值 0，校验范围为 `VER` 到 `PAYLOAD`。
- TARGET 的 PAYLOAD 固定为 `valid:u8 + offsetX:int16_LE + offsetY:int16_LE`，共 5 字节。
- 恢复 K230 联调后，K230 `uart_io.py` 测试入口可在握手后持续发送 `valid=1、offsetX=123、offsetY=-45`。

### 3.3 蓝牙任务与调试协议

命令不区分大小写。推荐以 `\r` 或 `\n` 结束；也支持空格、逗号、分号作为分隔符。没有结束符时，接收空闲 3 个系统节拍（30 ms）后执行。每条命令都会返回 `OK ...` 或 `ERR ...`。

`C` 命令进入单槽任务事件邮箱；App 每个有效节拍最多向 Mission 传递一个任务信号。普通信号不排队，同一拍只保留最后收到的一条；`C0` 具有最高优先级，待处理时不会被普通信号覆盖，并立即停车。

| 命令 | 作用 | 输入范围与限位 | 示例 |
|---|---|---|---|
| `C<number>` | 发送 Mission 单次任务信号；`C0` 固定全局停车，`C1~C255` 当前未绑定 25H | `0~255`，超限返回 `ERR RANGE` | `C1` |
| `L<number>` | 只更新左轮 PWM，右轮保持上次指令 | `-1000~1000`，超限自动夹紧 | `L10` |
| `R<number>` | 只更新右轮 PWM，左轮保持上次指令 | `-1000~1000`，超限自动夹紧 | `R10` |
| `U<number>` | 左右轮使用相同 PWM | `-1000~1000`；正数前进，负数后退 | `U100` |
| `O<number>` | 纵向舵机移动到指定角度 | 当前限位 `0°~270°` | `O10` |
| `D<number>` | 横向舵机移动到指定角度 | 当前限位 `0°~270°` | `D10` |

`L/R/U` 只在 MotionManager 空闲时执行，自动运动期间返回 `ERR BUSY`。数字仍是开环 PWM，不是 mm/s；OLED 上的 `LV/RV` 是编码器实测速度。

当前舵机限位仅对应源码中 270° 舵机的电气量程。实车连杆若存在更小的机械行程，通电调试前必须先收紧 `SERVO_VERTICAL_*_ANGLE` 和 `SERVO_HORIZONTAL_*_ANGLE`，并同步修改本文的 Pin 口表、协议表和公共参数表。

### 3.4 `MotionStraight` 直线行驶控制库

`MotionStraight` 已完成本轮实车调试，当前由 MotionManager 统一初始化和按模式更新。Mission 状态回调通过 `MotionManager_StartForward/StartBackward()` 使用它，不再直接修改主循环。

控制结构分为三层：

1. 距离层读取左右编码器相对路程的平均值，优先在全程 `5/6` 处进入末段减速，并连续过渡到本次调用指定的终点速度。
2. 航向层锁定启动瞬间的 MPU6050 连续累计偏航角，使用 PD 生成左右轮差速 PWM 修正，不做 ±180° 归一化。
3. 公共 `MotionWheel` 分别对左右轮编码器实测速度执行 PI，叠加速度前馈、静摩擦补偿和航向修正，再统一限幅输出。

调用顺序：

```text
上电且 MPU 零漂完成
    -> App_Init() 内部完成 MotionManager_Init()
    -> Mission 状态 onEnter
    -> MotionManager_StartForward(距离, 巡航速度, 终点速度)
    -> App_Update() 每拍更新 MotionManager
```

直线运动规划和航向参数位于 `MotionStraight.h` 顶部；双轮速度 PI、前馈和最终 PWM 限幅位于 `MotionWheel.h` 顶部。Mission 进入回调使用以下形式：

```c
#include "Application/Control/MotionManager.h"

return (MotionManager_StartForward(distanceMM, speedMMps, endSpeedMMps) ==
        MOTION_MANAGER_RESULT_OK) ?
    MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
```

- `MotionManager_StartForward/StartBackward()` 的 `distanceMM` 都填写正整数，方向由函数名决定。
- `speedMMps` 必须为正数；超过 `MOTION_STRAIGHT_MAX_SPEED_MMPS` 时自动限幅。
- `endSpeedMMps` 是非负速度大小且不能高于 `speedMMps`。设为 `0` 时平滑降速后释放电机；设为正数时到达目标后继续按该速度闭环前进，直到状态转换、`C0` 或 `MotionManager_Stop()` 停止当前运动。
- 常用流程使用 `MotionStraight_StartForward()` 或 `MotionStraight_StartBackward()`；距离参数填写正整数，巡航速度和终点速度直接填写 mm/s 浮点值，不使用枚举。
- `MotionStraight_Start()` 保留带符号距离的底层调用方式，终点速度方向自动跟随距离方向。
- 默认优先在全程 `5/6` 处开始减速；若 `MOTION_STRAIGHT_DECELERATION_MMPS2` 不足以在最后 `1/6` 内达到终点速度，程序会按运动学制动距离自动提前减速，避免终点速度突变。
- 进入 `distanceToleranceMM` 后只继续完成速度斜坡；终点速度为零时调用 `Motor_StopAll()` 清零 PWM 和方向脚，不再调用 `Motor_Brake()`。
- MPU6050 掉线、里程换算无效或更新周期非法时立即停止并进入错误状态。
- `MotionStraight` 直接使用 `Odometry` 已更新的左右路程和速度，不能再次读取 `Encoder_Get()`，否则会提前清空编码器增量。
- `MotionStraight` 运行期间不要调用 `Heading_SetYaw()` 重置角度，否则会改变本次直线行驶的航向基准。

实车调参顺序：

1. MotionManager 空闲时使用 `U100/U200/...` 记录 OLED 稳态速度，在 `MotionWheel.h` 顶部调整速度前馈、静摩擦 PWM 与最大可靠输出；自动运动开始后固件会拒绝 `L/R/U`，蓝牙仍可发送 Mission 的 `C` 信号。
2. 先令 `ki=0`，逐步增加速度 `kp` 到响应足够快且不持续振荡，再少量加入 `ki` 消除稳态误差。
3. 低速测试航向 `kp`；若偏差被放大，将 `correctionSign` 从 `1` 改为 `-1` 或反向。随后少量增加 `kd` 抑制摆动。
4. 最后调整加速度、最大减速度、减速起点比例、每次任务的终点速度和距离允许误差。

### 3.5 `MotionWheel`、`MotionManager` 与 `MotionLine`

统一命名层级如下：

| 层级 | 模块前缀 | 职责 |
|---|---|---|
| 任务调度 | `Mission_*` | 静态状态图、状态回调、转换条件和打断规则 |
| 统一运动入口 | `MotionManager_*` | 自动停止旧模式并选择直线、巡线或转向 |
| 上层运动模式 | `MotionStraight_*` / `MotionLine_*` / `Nav_*` | 具体运动算法和状态 |
| 下层公共执行 | `MotionWheel_*` | 双轮速度 PI、前馈、差速合成和电机 PWM 输出 |

`MotionWheel` 是三个运动模块共用的唯一双轮速度闭环与电机输出层。Mission 不直接调用三个模块的 Update，而是由 MotionManager 每拍只更新当前模式；启动新模式时 MotionManager 自动停止旧模式。

Mission 状态的进入回调可按以下方式启动巡线：

```c
#include "Accomplish/25H.h"
#include "Application/Control/MotionManager.h"
#include "Application/Mission/Mission.h"

static Mission_CallbackResult_t LineEnter(void)
{
    return (MotionManager_StartLine(ACCOMPLISH_25H_LINE_SPEED_MMPS) ==
            MOTION_MANAGER_RESULT_OK) ?
        MISSION_CALLBACK_OK : MISSION_CALLBACK_ERROR;
}

static Mission_ActionStatus_t LineUpdate(float dt)
{
    (void)dt;
    return Mission_GetMotionActionStatus();
}
```

模式切换顺序：

```text
Mission onEnter
    -> MotionManager_StartLine(...)
    -> App_Update() 每拍调用 MotionManager_Update(dt)
    -> 状态完成或被打断
    -> Mission onExit -> MotionManager_Stop()
```

- MotionLine 不使用 PID。五路从左到右的权重为 `-6、-3、0、+3、+6`，检测到黑线时把对应权重相加，并把最终结果限制在 `-6~+6`。
- 当前调整比例为 `0.2`：权重为 `-6` 时，左轮目标速度为巡线速度的 `0.8` 倍，右轮为 `1.2` 倍；权重为 `+6` 时左右相反。权重为正负 `3` 时，每侧增减巡线速度的 `10%`。
- 灰度位为 `1` 表示检测到黑线，五路全白为 `0x00`。连续全白达到 `MOTION_LINE_LOST_CONFIRM_TICKS` 后进入 `MOTION_LINE_STATE_FINISHED`；当前 25H 将该结果作为安全返回等待的条件。确认前保持上一拍的左右轮目标速度，任一路恢复为 1 时立即清零丢线计数。更新周期或公共轮速层异常才进入 `MOTION_LINE_STATE_ERROR`。
- 五路全黑 `0x1F` 当前按误差 0 继续直行；十字、停止线和任务标志必须在后续任务状态机中根据连续采样单独判断。
- `MotionLine.h` 顶部参数是当前首轮低速测试值，仍需根据实车循迹效果标定。

### 3.6 Mission 状态节点格式

`Mission.c` 是通用状态图执行器，不保存具体题目流程。当前 25H 的状态编号、回调、转换表和状态表全部位于 `Accomplish/25H.c`，并保持为 `static`。添加新题目时在根目录 `Accomplish/` 新建对应 `.c/.h`，按以下顺序书写。完整教程见根目录 `状态机.md`：

```c
static Mission_CallbackResult_t StateEnter(void);
static Mission_ActionStatus_t StateUpdate(float dt);
static void StateExit(Mission_ExitReason_t reason);
static uint8_t StateCondition(
    const Mission_Runtime_t *runtime,
    const App_UpdateContext_t *updateContext);

static const Mission_Transition_t s_transitions[] = {
    {StateCondition, TARGET_STATE, MISSION_TRANSITION_NORMAL},
};

/* 在 s_stateDefinitions[] 对应状态编号处登记回调和转换表。 */

static const Mission_GraphDefinition_t s_missionGraph = {
    .states = s_stateDefinitions,
    .stateCount = STATE_COUNT,
    .startState = STATE_WAITING,
    .errorState = STATE_ERROR,
};
```

- 转换数组从前到后就是优先级；同一拍只执行第一条满足条件的转换。
- 动作运行中只检查 `MISSION_TRANSITION_INTERRUPT`，并要求当前状态 `interruptible=1`；动作完成后只检查正常转换。
- 转换依次执行当前状态 `onExit()`、`MotionManager_Stop()` 和目标状态 `onEnter()`；内部活动标记保证一次进入最多对应一次退出。
- 空 `onEnter/onExit` 视为成功，空 `onUpdate` 视为立即完成；空条件回调表示该类型转换无条件成立。
- 动作完成但没有正常转换满足条件时保留在当前完成节点；动作或 MotionManager 报错时停车并进入内部错误节点。
- `C1~C255` 是单拍事件。条件未接受时立即丢弃，不在以后补触发；`C0` 始终停车并复位到起始状态。
- 当前 25H 使用 KEY1 按下沿启动，不使用 `C1`。新题目应在自己的头文件顶部定义按键掩码或任务信号、距离、速度、角度和传感器掩码，不把题目参数放回 Mission。
- 新增运动形式时，模块使用 `MotionXxx_` 前缀并提供 `Init/Start/Update/Stop/IsBusy/IsFinished/GetState/GetError`；所有可调参数放在对应 `.h` 开头。随后在 MotionManager 增加模式、启动包装、Update 和 Stop 分支，Mission 不直接调用底层 Update。

### 3.7 `Nav` 目标角转向库

`Nav` 由 MotionManager 统一初始化和更新。Mission 回调使用 `MotionManager_TurnTo/TurnBy()`，角度仍直接使用 `Heading_GetYaw()` 的连续累计值，不做 ±180° 归一化。

Nav 只有一种车轮动作：左右轮等速反向，车体围绕两轮中点附近转动。

```c
#include "Application/Control/MotionManager.h"

/* 绝对角：指向连续航向角 90°。 */
(void)MotionManager_TurnTo(90.0f, 80.0f);

/* 相对角：从当前方向再转 +90°。 */
(void)MotionManager_TurnBy(90.0f, 80.0f);
```

- `To` 接口输入连续累计绝对角；例如当前为 370°，输入 90° 会按直接误差回到 90°，不会自动选择 ±180° 最短路径。
- `By` 接口输入相对转角；正负方向由 `NAV_ROTATION_COMMAND_SIGN` 与实车安装共同决定，可输入大于 360° 的多圈角度。
- 首次测试使用 60~80 mm/s。若启动后角度误差持续增大，只翻转 `NAV_ROTATION_COMMAND_SIGN`。
- Nav 到角后先把轮速斜坡降到零，再要求连续 `NAV_SETTLE_TICKS` 个周期处于允许误差内，避免单次采样抖动误判完成。

当前 25H 在 KEY1 启动巡线时记录连续航向作为 0°基准。每次左侧双灰度触发并完成 150 mm 直行后，都把下一绝对目标减去 90°，再调用 `MotionManager_TurnTo()`；因此目标序列为 `startYaw-90°`、`startYaw-180°`、`startYaw-270°`……，不是在进入 TURN 时临时调用相对角接口。

## 4. 工程文件类型与职责

| 文件或目录 | 类型 | 职责 |
|---|---|---|
| `main.c` | C 源文件 | 仅调用 App 初始化、Mission 初始化、硬件中断启用以及 App/Mission 主循环 |
| `main.syscfg` | TI SysConfig | 时钟树、GPIO、UART、I2C、PWM、SysTick 和 PinMux 的唯一配置源 |
| `.project`、`.cproject`、`.settings/` | CCS 工程元数据 | 工程名、TI Arm Clang 选项、SDK/SysConfig 依赖和 IDE 设置 |
| `targetConfigs/*.ccxml` | CCS 目标配置 | MSPM0G3507 调试连接配置 |
| `Application/Comms/` | 应用层 C 模块 | 蓝牙调试命令；K230 二进制帧、CRC8、握手和目标解析 |
| `Application/Core/` | 应用运行层 C 模块 | 固定硬件初始化、零漂、100 Hz 后台服务、按键和蓝牙事件采集 |
| `Application/Mission/` | 通用任务执行层 C 模块 | 校验并执行题目层提供的静态状态图、生命周期回调、有序条件转换和打断处理 |
| `Accomplish/` | 具体题目实现 C 模块 | 位于工程根目录；每道题独立保存用户参数、状态编号、回调、转换表和静态状态图；当前启用 `25H.c/.h`，并保留 `25E.c/.h` |
| `Application/Control/` | 运动控制层 C 模块 | MotionManager、通用 PID、公共双轮速度闭环、直线、巡线和目标角转向 |
| `Application/Debug/` | 应用层 C 模块 | OLED 调试页面编排与 10 Hz 刷新 |
| `Application/Servo/` | 舵机硬件模块 | TIMA0 双通道 PWM、角度限位和脉宽换算 |
| `Application/State/` | 状态层 C 模块 | Z 轴航向角解算、编码器里程与速度状态 |
| `Hardware/Board/` | 板级驱动 | 按键、LED、蜂鸣器 |
| `Hardware/Comms/` | 通信驱动 | UART1 蓝牙和 UART2 K230 的中断接收环形缓冲区与发送接口 |
| `Hardware/Display/` | 显示驱动与数据 | OLED I2C 驱动、帧缓冲、字模和图像数据 |
| `Hardware/Motor/` | 电机驱动 | TIMG8 PWM、TB6612 方向控制、编码器正交解码 |
| `Hardware/Sensors/` | 传感器驱动 | 五路灰度 GPIO、MPU6050 软件 I2C |
| `System/` | 系统基础模块 | 阻塞延时、100 Hz SysTick 计数和全局硬件中断开关 |
| `Debug/`、`Release/` | 生成目录 | 目标文件、依赖文件、链接文件和固件输出；不手工修改 |
| `.gitignore` | Git 配置 | 排除构建产物 |
| `README.md` | 工程索引 | 时钟、Pin 口、文件职责、公共接口和公共参数 |
| `状态机.md` | Markdown 教程 | 说明如何为新题目创建 Accomplish 参数文件、状态图、主程序依赖和验证流程 |

### 4.1 源文件快速定位

| 源文件 / 头文件 | 文件职责 |
|---|---|
| `Application/Comms/BluetoothDebug.c/.h` | 解析 `C/L/R/U/O/D` 命令，保存单槽任务事件，并限制自动运动期间的开环电机调试 |
| `Application/Core/App.c/.h` | 封装系统初始化和每拍固定更新，向 Mission 提供 dt、按键边沿和蓝牙信号 |
| `Application/Mission/Mission.c/.h` | 定义状态图公共类型，校验题目状态图并执行每拍最多一次的状态转换 |
| `Accomplish/25E.c/.h` | 保存 25E 参数和状态图：每轮绝对目标在上一目标上增加 180°；当前未由 main 加载 |
| `Accomplish/25H.c/.h` | 保存 25H 参数和状态图：KEY1 启动巡线，左侧双黑线后直行 150 mm，绝对左转目标每轮减少 90°并循环 |
| `Application/Comms/K230Link.c/.h` | 解析 `AA 55` 二进制帧和 CRC8，执行 READY/READY_ACK 双向握手，保存最新 TARGET |
| `Application/Control/PID.c/.h` | 通用 PID 初始化、调参、复位和单步计算 |
| `Application/Control/MotionStraight.c/.h` | 头文件顶部保存直线参数；源文件实现距离规划、5/6 末段减速、可选终点速度、MPU6050 航向 PD 和软停车状态机 |
| `Application/Control/MotionWheel.c/.h` | 头文件顶部保存公共轮速参数；源文件实现 MotionStraight、MotionLine 与 Nav 共用的双轮速度 PI、前馈、差速修正合成和 PWM 限幅 |
| `Application/Control/MotionLine.c/.h` | 头文件顶部保存巡线参数；源文件实现五路灰度离散权重差速、连续丢线确认、丢线正常完成和状态管理；巡线层不使用 PID |
| `Application/Control/MotionManager.c/.h` | 统一包装直线、巡线和转向；自动停止旧模式并只更新当前模式 |
| `Application/Control/Nav.c/.h` | 头文件顶部保存转向参数；源文件实现连续航向目标、双轮等速反向转向和到角稳定判定 |
| `Application/Debug/DebugDisplay.c/.h` | 组织启动零漂提示、基础状态和 MotionLine 运行状态的 OLED 八行调试数据 |
| `Application/Servo/Servo.c/.h` | 将舵机角度换算为 TIMA0 比较值，并执行纵向/横向限位 |
| `Application/State/Heading.c/.h` | MPU6050 Z 轴零漂标定、角速度积分和尺度标定 |
| `Application/State/Odometry.c/.h` | 读取编码器增量，累计左右路程并计算 mm/s 速度 |
| `Hardware/Board/Beep.c/.h` | 蜂鸣器与 LED2 的非阻塞提示状态机 |
| `Hardware/Board/Key.c/.h` | 四个低有效按键的非阻塞状态读取 |
| `Hardware/Board/LED.c/.h` | LED1、LED2 的开、关、翻转接口 |
| `Hardware/Comms/Serial.c/.h` | UART1/UART2 RX 中断、独立环形缓冲区和阻塞发送 |
| `Hardware/Display/OLED.c/.h` | OLED I2C 传输、128×64 帧缓冲、文本和图形绘制 |
| `Hardware/Display/OLED_Data.c/.h` | ASCII/中文字模和公共位图常量 |
| `Hardware/Motor/Encoder.c/.h` | GPIOA 中断中的左右编码器四倍频正交解码 |
| `Hardware/Motor/Motor.c/.h` | 左右物理电机到 TB6612 A/B 通道的映射、方向和制动 |
| `Hardware/Motor/PWM.c/.h` | TIMG8 双通道占空比到比较值的换算 |
| `Hardware/Sensors/Graydetect.c/.h` | 五路灰度状态位图、通道读取和加权偏差 |
| `Hardware/Sensors/MPU6050.c/.h` | PA10/PA11 软件 I2C、MPU6050 配置和原始数据读取 |
| `System/Delay.c/.h` | 基于 32 MHz CPUCLK 的 us/ms/s 阻塞延时 |
| `System/Tick.c/.h` | SysTick ISR 累计与主循环原子取出 100 Hz 节拍 |
| `System/Interrupt.c/.h` | 在 App 和 Mission 初始化完成后统一开启或关闭全局硬件中断 |

## 5. 公共函数接口

以下只列出头文件公开声明。`.c` 文件内的 `static` 函数和变量属于文件内部实现，不作为跨模块接口使用。

### 5.1 `Application/Comms/BluetoothDebug.h`

```c
void BluetoothDebug_Init(void);
void BluetoothDebug_Update(uint8_t elapsedTicks,
                           uint8_t manualMotorEnabled);
uint8_t BluetoothDebug_PopSignal(uint8_t *signal);
int16_t BluetoothDebug_GetLeftCommand(void);
int16_t BluetoothDebug_GetRightCommand(void);
```

### 5.1.1 `Application/Comms/K230Link.h`

```c
typedef struct
{
    uint8_t valid;
    int16_t offsetX;
    int16_t offsetY;
    uint8_t sequence;
} K230Link_Target_t;

void K230Link_Init(void);
void K230Link_Update(uint8_t elapsedTicks);
uint8_t K230Link_IsReady(void);
uint8_t K230Link_GetTarget(K230Link_Target_t *target);
```

### 5.1.2 `Application/Core/App.h`

```c
typedef struct
{
    uint8_t elapsedTicks;
    float dt;
    uint8_t pressedKeys;
    uint8_t pressedEdges;
    uint8_t hasBluetoothSignal;
    uint8_t bluetoothSignal;
} App_UpdateContext_t;

void App_Init(void);
uint8_t App_Update(App_UpdateContext_t *context);
```

### 5.2 `Application/Control/PID.h`

```c
typedef struct
{
    float Kp, Ki, Kd;
    float integral;
    float prevError;
    float outMax;
    float integralMax;
} PID_t;

void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float outMax, float integralMax);
void PID_SetTunings(PID_t *pid, float kp, float ki, float kd);
void PID_Reset(PID_t *pid);
float PID_Update(PID_t *pid, float setpoint, float measure, float dt);
```

### 5.3 `Application/Control/MotionStraight.h`

```c
MotionStraight_Result_t MotionStraight_Init(void);
MotionStraight_Result_t MotionStraight_Start(
    float distanceMM, float speedMMps, float endSpeedMMps);
MotionStraight_Result_t MotionStraight_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
MotionStraight_Result_t MotionStraight_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
void MotionStraight_Update(float dt);
void MotionStraight_Stop(void);
uint8_t MotionStraight_IsConfigured(void);
uint8_t MotionStraight_IsBusy(void);
uint8_t MotionStraight_IsFinished(void);
MotionStraight_State_t MotionStraight_GetState(void);
MotionStraight_Error_t MotionStraight_GetError(void);
float MotionStraight_GetRemainingDistanceMM(void);
```

`MotionStraight_State_t` 包含空闲、运行、终点低速持续、完成和错误状态。终点速度为正数时进入 `MOTION_STRAIGHT_STATE_CONTINUING` 并继续占用电机，此时 `MotionStraight_IsFinished()` 和 `MotionStraight_IsBusy()` 都返回 1，调用 `MotionStraight_Stop()` 后才释放；终点速度为零时进入完成状态。`MotionStraight_Error_t` 区分 MPU 掉线、里程换算无效、更新周期非法和公共轮速层错误；`MotionStraight_Result_t` 返回启动、忙、参数、配置和传感器检查结果。

### 5.4 `Application/Control/MotionWheel.h`

```c
MotionWheel_Result_t MotionWheel_Init(void);
MotionWheel_Result_t MotionWheel_Update(
    const MotionWheel_Command_t *command, float dt);
void MotionWheel_Reset(void);
void MotionWheel_Stop(void);
uint8_t MotionWheel_IsConfigured(void);
float MotionWheel_GetMaximumCommandPWM(void);
float MotionWheel_GetLeftCommandPWM(void);
float MotionWheel_GetRightCommandPWM(void);
```

`MotionWheel_Command_t` 包含左右轮目标速度 `targetSpeedLMMps/targetSpeedRMMps` 和上层控制器提供的附加修正 `trimLPWM/trimRPWM`。MotionWheel 是电机闭环的唯一公共写入层，上层模式不得并行调用。

### 5.5 `Application/Control/MotionLine.h`

```c
MotionLine_Result_t MotionLine_Init(void);
MotionLine_Result_t MotionLine_Start(float speedMMps);
void MotionLine_Update(float dt);
void MotionLine_Stop(void);
uint8_t MotionLine_IsConfigured(void);
uint8_t MotionLine_IsBusy(void);
uint8_t MotionLine_IsFinished(void);
MotionLine_State_t MotionLine_GetState(void);
MotionLine_Error_t MotionLine_GetError(void);
float MotionLine_GetLineError(void);
```

`MotionLine_GetLineError()` 当前返回最近一次有效灰度位图得到的离散权重，范围为 `-6~+6`，它不再是 PID 输入误差。

### 5.6 `Application/Control/Nav.h`

```c
Nav_Result_t Nav_Init(void);
Nav_Result_t Nav_StartTo(float targetYawDeg, float speedMMps);
Nav_Result_t Nav_StartBy(float deltaYawDeg, float speedMMps);
void Nav_Update(float dt);
void Nav_Stop(void);
uint8_t Nav_IsConfigured(void);
uint8_t Nav_IsBusy(void);
uint8_t Nav_IsFinished(void);
Nav_State_t Nav_GetState(void);
Nav_Error_t Nav_GetError(void);
float Nav_GetTargetYawDeg(void);
float Nav_GetAngleErrorDeg(void);
```

`Nav_StartTo()` 接受连续绝对航向角，`Nav_StartBy()` 接受相对转角，速度参数单位为 mm/s；两者都固定使用双轮等速反向转向。

### 5.6.1 `Application/Control/MotionManager.h`

```c
MotionManager_Result_t MotionManager_Init(void);
MotionManager_Result_t MotionManager_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
MotionManager_Result_t MotionManager_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
MotionManager_Result_t MotionManager_StartLine(float speedMMps);
MotionManager_Result_t MotionManager_TurnTo(
    float targetYawDeg, float speedMMps);
MotionManager_Result_t MotionManager_TurnBy(
    float deltaYawDeg, float speedMMps);
void MotionManager_Update(float dt);
void MotionManager_Stop(void);
uint8_t MotionManager_IsConfigured(void);
uint8_t MotionManager_IsBusy(void);
uint8_t MotionManager_IsFinished(void);
MotionManager_Mode_t MotionManager_GetMode(void);
MotionManager_Error_t MotionManager_GetError(void);
```

### 5.6.2 `Application/Mission/Mission.h`

```c
typedef Mission_CallbackResult_t (*Mission_EnterCallback_t)(void);
typedef Mission_ActionStatus_t (*Mission_UpdateCallback_t)(float dt);
typedef void (*Mission_ExitCallback_t)(Mission_ExitReason_t reason);
typedef uint8_t (*Mission_ConditionCallback_t)(
    const Mission_Runtime_t *runtime,
    const App_UpdateContext_t *updateContext);

typedef struct
{
    Mission_ConditionCallback_t condition;
    uint16_t targetState;
    Mission_TransitionType_t type;
} Mission_Transition_t;

typedef struct
{
    Mission_EnterCallback_t onEnter;
    Mission_UpdateCallback_t onUpdate;
    Mission_ExitCallback_t onExit;
    const Mission_Transition_t *transitions;
    uint8_t transitionCount;
    uint8_t interruptible;
} Mission_StateDefinition_t;

typedef struct
{
    const Mission_StateDefinition_t *states;
    uint16_t stateCount;
    uint16_t startState;
    uint16_t errorState;
} Mission_GraphDefinition_t;

void Mission_Init(const Mission_GraphDefinition_t *graph);
void Mission_Update(const App_UpdateContext_t *updateContext);
void Mission_Stop(void);
Mission_Status_t Mission_GetStatus(void);
const Mission_Runtime_t *Mission_GetRuntime(void);
Mission_ActionStatus_t Mission_GetMotionActionStatus(void);
uint8_t Mission_ContextHasBluetoothSignal(
    const App_UpdateContext_t *updateContext, uint8_t signal);
```

### 5.6.3 `Accomplish/25E.h`

```c
const Mission_GraphDefinition_t *Accomplish25E_GetMissionGraph(void);
```

该函数只返回静态只读状态图，不进行硬件初始化。切换题目时由 `main.c` 选择对应 Accomplish 头文件和状态图函数。

### 5.6.4 `Accomplish/25H.h`

```c
const Mission_GraphDefinition_t *Accomplish25H_GetMissionGraph(void);
```

该函数返回 25H 静态只读状态图。当前 `main.c` 调用该接口；题目参数全部位于 `Accomplish/25H.h` 开头。

### 5.7 `Application/Debug/DebugDisplay.h`

```c
void DebugDisplay_Init(void);
void DebugDisplay_ShowHeadingCalibration(uint8_t mpuReady);
void DebugDisplay_Update(uint8_t elapsedTicks);
```

### 5.8 `Application/Servo/Servo.h`

```c
void Servo_Init(void);
void Servo_SetVerticalAngle(uint16_t angle);
void Servo_SetHorizontalAngle(uint16_t angle);
uint16_t Servo_GetVerticalAngle(void);
uint16_t Servo_GetHorizontalAngle(void);
void Servo_Reset(void);
```

### 5.9 `Application/State/Heading.h`

```c
void Heading_Init(void);
void Heading_Calibrate(void);
void Heading_Update(float dt);
uint8_t Heading_IsReady(void);
float Heading_GetYaw(void);
void Heading_SetYaw(float yaw);
void Heading_ScaleCalibStart(void);
float Heading_ScaleCalibFinish(uint16_t turns);
void Heading_ScaleCalibCancel(void);
float Heading_GetCalibAngle(void);
float Heading_GetScale(void);
void Heading_SetScale(float scale);
```

### 5.10 `Application/State/Odometry.h`

```c
void Odometry_Init(void);
void Odometry_Update(uint8_t ticks);
void Odometry_Reset(void);
float Odometry_GetDistanceMM(void);
float Odometry_GetDistanceLMM(void);
float Odometry_GetDistanceRMM(void);
float Odometry_GetSpeedL(void);
float Odometry_GetSpeedR(void);
```

### 5.11 `Hardware/Board/Beep.h`

```c
void Beep_Init(void);
void Beep_On(void);
void Beep_Off(void);
void Beep_Notify(uint8_t times);
void Beep_Long(void);
void Beep_Tick(void);
```

### 5.12 `Hardware/Board/Key.h`

```c
void Key_Init(void);
uint8_t Key_GetPressedMask(void);
uint8_t Key_GetNum(void);
```

`Key_GetPressedMask()` 的 bit0~bit3 对应 KEY1~KEY4；`Key_GetNum()` 返回当前第一个按下的键号，未按下返回 0。两个读取接口均不阻塞。

### 5.13 `Hardware/Board/LED.h`

```c
void LED_Init(void);
void LED1_ON(void);
void LED1_OFF(void);
void LED1_Turn(void);
void LED2_ON(void);
void LED2_OFF(void);
void LED2_Turn(void);
void LED_RGB_ON(void);
void LED_RGB_OFF(void);
```

### 5.14 `Hardware/Comms/Serial.h`

```c
void Serial1_Init(void);
uint32_t Serial1_Available(void);
uint8_t Serial1_ReadByte(uint8_t *byte);
void Serial1_SendByte(uint8_t byte);
void Serial1_SendArray(const uint8_t *array, uint16_t length);
void Serial1_SendString(const char *string);
void Serial1_Printf(const char *format, ...);
void Serial2_Init(void);
uint32_t Serial2_Available(void);
uint8_t Serial2_ReadByte(uint8_t *byte);
void Serial2_SendByte(uint8_t byte);
void Serial2_SendArray(const uint8_t *array, uint16_t length);
```

### 5.15 `Hardware/Display/OLED.h`

```c
void OLED_Init(void);
uint8_t OLED_IsReady(void);
void OLED_Update(void);
void OLED_UpdateArea(int16_t x, int16_t y, uint8_t width, uint8_t height);
void OLED_Clear(void);
void OLED_ClearArea(int16_t x, int16_t y, uint8_t width, uint8_t height);
void OLED_Reverse(void);
void OLED_ReverseArea(int16_t x, int16_t y, uint8_t width, uint8_t height);
void OLED_ShowChar(int16_t x, int16_t y, char value, uint8_t fontSize);
void OLED_ShowString(int16_t x, int16_t y, const char *string, uint8_t fontSize);
void OLED_ShowNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize);
void OLED_ShowSignedNum(int16_t x, int16_t y, int32_t number, uint8_t length, uint8_t fontSize);
void OLED_ShowHexNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize);
void OLED_ShowBinNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize);
void OLED_ShowFloatNum(int16_t x, int16_t y, double number,
                       uint8_t intLength, uint8_t fraLength, uint8_t fontSize);
void OLED_ShowImage(int16_t x, int16_t y, uint8_t width, uint8_t height,
                    const uint8_t *image);
void OLED_Printf(int16_t x, int16_t y, uint8_t fontSize, const char *format, ...);
void OLED_DrawPoint(int16_t x, int16_t y);
uint8_t OLED_GetPoint(int16_t x, int16_t y);
void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void OLED_DrawRectangle(int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t filled);
void OLED_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint8_t filled);
void OLED_DrawCircle(int16_t x, int16_t y, uint8_t radius, uint8_t filled);
void OLED_DrawEllipse(int16_t x, int16_t y, uint8_t a, uint8_t b, uint8_t filled);
void OLED_DrawArc(int16_t x, int16_t y, uint8_t radius,
                  int16_t startAngle, int16_t endAngle, uint8_t filled);
```

### 5.16 `Hardware/Motor/Encoder.h`

```c
void Encoder_Init(void);
int16_t Encoder_Get(uint8_t n);
```

`Encoder_Get(1)` 读取并清零左编码器增量，`Encoder_Get(2)` 读取并清零右编码器增量。

### 5.17 `Hardware/Motor/Motor.h`

```c
void Motor_Init(void);
void Motor_SetLeftPWM(int16_t PWM);
void Motor_SetRightPWM(int16_t PWM);
void Motor_SetPWM(int16_t LeftPWM, int16_t RightPWM);
void Motor_StopAll(void);
void Motor_Brake(void);
void Motor_Run(int16_t leftSpeed, int16_t rightSpeed);
void Motor_Forward(int16_t speed);
void Motor_Backward(int16_t speed);
void Motor_TurnLeft(int16_t speed);
void Motor_TurnRight(int16_t speed);
void Motor_SpinLeft(int16_t speed);
void Motor_SpinRight(int16_t speed);
void Motor_Stop(void);
```

### 5.18 `Hardware/Motor/PWM.h`

```c
void PWM_Init(void);
void PWM_SetCompareA(uint16_t Compare);
void PWM_SetCompareB(uint16_t Compare);
```

### 5.19 `Hardware/Sensors/Graydetect.h`

```c
void Graydetect_Init(void);
uint8_t Graydetect_GetState(void);
uint8_t Graydetect_GetBit(uint8_t index);
float Graydetect_GetError(uint8_t side);
uint8_t Graydetect_OnLine(uint8_t side);
```

### 5.20 `Hardware/Sensors/MPU6050.h`

```c
void MPU6050_Init(void);
uint8_t MPU6050_IsReady(void);
uint8_t MPU6050_GetID(void);
void MPU6050_GetData(int16_t *ax, int16_t *ay, int16_t *az,
                     int16_t *gx, int16_t *gy, int16_t *gz);
int16_t MPU6050_GetGyroZ(void);
```

### 5.21 `System/Delay.h`

```c
void Delay_us(uint32_t us);
void Delay_ms(uint32_t ms);
void Delay_s(uint32_t s);
```

### 5.22 `System/Tick.h`

```c
void Tick_Init(void);
uint8_t Tick_Poll(void);
uint8_t Tick_PollCount(void);
```

### 5.22.1 `System/Interrupt.h`

```c
void Interrupt_Enable(void);
void Interrupt_Disable(void);
```

## 6. 公共参数和公共数据

| 所在头文件 | 名称 | 当前值/类型 | 含义 |
|---|---|---:|---|
| `BluetoothDebug.h` | `BLUETOOTH_COMMAND_IDLE_TICKS` | `3U` | 无结束符命令的 30 ms 空闲判定 |
| `BluetoothDebug.h` | `BLUETOOTH_TASK_SIGNAL_MAX` | `255U` | C 任务信号最大编号；普通信号不排队 |
| `K230Link.h` | `K230_LINK_FRAME_MAGIC_0/1` | `0xAAU` / `0x55U` | K230 帧头 |
| `K230Link.h` | `K230_LINK_FRAME_VERSION` | `0x01U` | 当前通信协议版本 |
| `K230Link.h` | `K230_LINK_MAX_PAYLOAD_LENGTH` | `32U` | 允许接收的最大 PAYLOAD 长度 |
| `K230Link.h` | `K230_LINK_READY_RETRY_TICKS` | `10U` | 100 Hz 下每 100 ms 重发 READY |
| `K230Link.h` | `K230_LINK_MESSAGE_READY/READY_ACK/TARGET` | `0x01U` / `0x02U` / `0x10U` | 消息类型编号 |
| `DebugDisplay.h` | `DEBUG_DISPLAY_REFRESH_TICKS` | `10U` | OLED 10 Hz 刷新间隔 |
| `MotionStraight.h` | `MOTION_STRAIGHT_*` | 见 6.2 | 航向 PD、直线速度规划、减速起点比例和距离允许误差 |
| `MotionWheel.h` | `MOTION_WHEEL_*` | 见 6.1 | MotionStraight、MotionLine 与 Nav 共用的速度 PI、前馈和 PWM 限幅 |
| `MotionLine.h` | `MOTION_LINE_*` | 见 6.3 | 灰度权重、最大速度调整比例、巡线速度上限和丢线确认节拍 |
| `Accomplish/25E.h` | `ACCOMPLISH_25E_*` | 见 6.5 | 25E 启动按键、直线距离与速度、入线确认、巡线速度和转向参数 |
| `Accomplish/25H.h` | `ACCOMPLISH_25H_*` | 见 6.6 | 25H 启动按键、左侧标志掩码、巡线、150 mm 直行和绝对左转参数 |
| `Nav.h` | `NAV_*` | 见 6.4 | 双轮转向的加减速、低速区、到角误差和稳定判定 |
| `Servo.h` | `SERVO_PHYSICAL_RANGE_DEG` | `270U` | 脉宽换算对应的舵机物理量程 |
| `Servo.h` | `SERVO_MIN_PULSE_US` / `SERVO_MAX_PULSE_US` | `500U` / `2500U` | 舵机最小/最大高电平脉宽 |
| `Servo.h` | `SERVO_FRAME_US` | `20000U` | 50 Hz 舵机帧周期 |
| `Servo.h` | `SERVO_VERTICAL_MIN_ANGLE` / `MAX` / `DEFAULT` | `0U` / `270U` / `135U` | 纵向轴限位与上电角度 |
| `Servo.h` | `SERVO_HORIZONTAL_MIN_ANGLE` / `MAX` / `DEFAULT` | `0U` / `270U` / `135U` | 横向轴限位与上电角度 |
| `Heading.h` | `HEADING_CALIBRATION_SAMPLES` | `400U` | 开机零漂采样数 |
| `Heading.h` | `HEADING_CALIBRATION_INTERVAL_MS` | `2U` | 零漂采样间隔 |
| `Odometry.h` | `Odometry_CountsPerMM` | `float`，初值 `6.23f` | 每毫米编码器计数，必须按实车标定 |
| `Serial.h` | `SERIAL1_RX_BUFFER_SIZE` | `1024U` | UART1 环形接收缓冲区容量 |
| `Serial.h` | `SERIAL2_RX_BUFFER_SIZE` | `256U` | UART2 K230 环形接收缓冲区容量 |
| `Serial.h` | `Serial1_RxFlag` | `volatile uint8_t` | UART1 存在未读数据标志 |
| `PWM.h` | `PWM_MAX_DUTY` | `1000U` | 电机 PWM 指令绝对值上限 |
| `Graydetect.h` | `GRAY_SIDE_ALL/LEFT/RIGHT` | `0/1/2` | 灰度误差计算的通道范围 |
| `OLED.h` | `OLED_8X16` / `OLED_6X8` | `8U` / `6U` | 字体尺寸选择 |
| `OLED.h` | `OLED_UNFILLED` / `OLED_FILLED` | `0U` / `1U` | 图形空心/实心选择 |
| `OLED_Data.h` | `OLED_F8x16`、`OLED_F6x8`、`OLED_CF16x16`、`Diode` | `const` 字模/位图数组 | OLED 公共显示数据 |
| `OLED_Data.h` | `ChineseCell_t` / `OLED_CHARSET_UTF8` | 字模结构 / 字符集宏 | 中文字模索引与 16×16 数据格式 |
| `Tick.h` | `TICK_HZ` / `TICK_DT` | `100U` / `0.01f` | 系统节拍频率与秒单位周期 |

### 6.1 `MotionWheel.h` 参数

以下宏位于 `Application/Control/MotionWheel.h` 开头，由直线、巡线和 Nav 共用：

| 宏 | 单位 | 作用 |
|---|---:|---|
| `MOTION_WHEEL_KP` | PWM/(mm/s) | 左右轮各自的速度比例增益 |
| `MOTION_WHEEL_KI` | PWM/mm | 左右轮各自的速度积分增益 |
| `MOTION_WHEEL_INTEGRAL_LIMIT` | mm | 速度积分绝对值限幅；`KI>0` 时必须大于 0 |
| `MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS` | PWM/(mm/s) | 目标速度到 PWM 的线性前馈斜率 |
| `MOTION_WHEEL_STATIC_FRICTION_PWM` | PWM | 克服静摩擦所需的符号前馈 |
| `MOTION_WHEEL_MAX_COMMAND_PWM` | PWM | 每个车轮最终 PWM 绝对值上限，不得超过 `PWM_MAX_DUTY` |

当前值：

| 宏 | 当前值 |
|---|---:|
| `MOTION_WHEEL_KP` / `MOTION_WHEEL_KI` / `MOTION_WHEEL_INTEGRAL_LIMIT` | `1.0f` / `0.0f` / `0.0f` |
| `MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS` / `MOTION_WHEEL_STATIC_FRICTION_PWM` | `2.0f` / `0.0f` |
| `MOTION_WHEEL_MAX_COMMAND_PWM` | `1000.0f` |

### 6.2 `MotionStraight.h` 参数

以下宏位于 `Application/Control/MotionStraight.h` 开头：

| 宏 | 单位 | 作用 |
|---|---:|---|
| `MOTION_STRAIGHT_HEADING_KP` | PWM/° | 航向误差比例增益 |
| `MOTION_STRAIGHT_HEADING_KD` | PWM/(°/s) | 航向误差微分增益 |
| `MOTION_STRAIGHT_HEADING_LIMIT_PWM` | PWM | 航向差速修正绝对值上限，必须大于 0 |
| `MOTION_STRAIGHT_CORRECTION_SIGN` | `1` 或 `-1` | 航向差速方向；偏差被放大时翻转符号 |
| `MOTION_STRAIGHT_MAX_SPEED_MMPS` | mm/s | 允许请求的最大直线速度，超出时自动限幅 |
| `MOTION_STRAIGHT_ACCELERATION_MMPS2` | mm/s² | 目标速度上升斜率 |
| `MOTION_STRAIGHT_DECELERATION_MMPS2` | mm/s² | 允许使用的最大减速度；不足以满足末段目标时会提前开始减速 |
| `MOTION_STRAIGHT_DECELERATION_START_RATIO` | 0~1 比例 | 首选减速起点占全程的比例；当前 `5/6` 表示最后 `1/6` 为减速段 |
| `MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM` | mm | 判定到达目标距离的允许误差；到点后继续完成速度斜坡 |

当前实车测试值：

| 宏 | 当前值 |
|---|---:|
| `MOTION_STRAIGHT_HEADING_KP` / `MOTION_STRAIGHT_HEADING_KD` | `6.0f` / `0.4f` |
| `MOTION_STRAIGHT_HEADING_LIMIT_PWM` / `MOTION_STRAIGHT_CORRECTION_SIGN` | `700.0f` / `-1` |
| `MOTION_STRAIGHT_MAX_SPEED_MMPS` | `1000.0f` |
| `MOTION_STRAIGHT_ACCELERATION_MMPS2` / `MOTION_STRAIGHT_DECELERATION_MMPS2` | `300.0f` / `250.0f` |
| `MOTION_STRAIGHT_DECELERATION_START_RATIO` / `MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM` | `5.0f / 6.0f` / `5.0f` |

### 6.3 `MotionLine.h` 参数

以下宏位于 `Application/Control/MotionLine.h` 开头。当前 25H 通过 MotionManager 启动巡线，连续丢线确认后把巡线标记为正常完成并返回等待：

| 宏 | 单位 | 当前值 | 作用 |
|---|---:|---:|---|
| `MOTION_LINE_OUTER_WEIGHT` | 无 | `6` | 左右最外侧灰度权重的绝对值，对应最大修正力度 |
| `MOTION_LINE_INNER_WEIGHT` | 无 | `3` | 左右内侧灰度权重的绝对值，对应最大修正力度的一半 |
| `MOTION_LINE_MAX_ADJUST_RATIO` | 比例 | `0.2f` | 权重达到正负 6 时，一侧减去、另一侧增加的巡线速度比例 |
| `MOTION_LINE_MAX_SPEED_MMPS` | mm/s | `1000.0f` | 巡线请求软件上限；应结合公共轮速前馈和最终 PWM 上限设置 |
| `MOTION_LINE_LOST_CONFIRM_TICKS` | 100 Hz 节拍 | `50U` | 连续五路全白达到 50 次后确认丢线，当前约为 500 ms |

### 6.4 `Nav.h` 参数

以下宏位于 `Application/Control/Nav.h` 开头，当前仍是低速测试值：

| 宏 | 单位 | 当前值 | 作用 |
|---|---:|---:|---|
| `NAV_MAX_TURN_SPEED_MMPS` | mm/s | `200.0f` | Nav 接口允许请求的每侧轮最大速度；首次测试不要直接使用上限 |
| `NAV_MIN_TURN_SPEED_MMPS` | mm/s | `40.0f` | 接近目标角时的每侧轮最低速度；转不动则增大，冲角明显则减小 |
| `NAV_SLOWDOWN_ANGLE_DEG` | ° | `45.0f` | 剩余角进入低速区的阈值；冲角时增大该值 |
| `NAV_ACCELERATION_MMPS2` | mm/s² | `150.0f` | 转向轮速上升斜率；越小起转越柔和 |
| `NAV_DECELERATION_MMPS2` | mm/s² | `600.0f` | 转向轮速下降斜率；冲角时可增大，停车突兀时减小 |
| `NAV_ANGLE_TOLERANCE_DEG` | ° | `2.0f` | 到角允许误差；太小可能在目标附近反复修正 |
| `NAV_SETTLE_TICKS` | 100 Hz 周期 | `3U` | 连续稳定 30 ms 后判定完成 |
| `NAV_ROTATION_COMMAND_SIGN` | `1` 或 `-1` | `1` | 角度与双轮指令方向映射；误差持续增大时翻转 |

### 6.5 `Accomplish/25E.h` 参数

| 宏 | 单位 | 当前值 | 作用 |
|---|---:|---:|---|
| `ACCOMPLISH_25E_START_KEY_MASK` | 按键位图 | `0x01U` | KEY1 的 bit0 掩码；等待状态检测按下沿后启动 25E |
| `ACCOMPLISH_25E_STRAIGHT_DISTANCE_MM` | mm | `2000U` | 每轮直线寻找黑线的最大距离 |
| `ACCOMPLISH_25E_STRAIGHT_SPEED_MMPS` | mm/s | `300.0f` | 直线巡航速度 |
| `ACCOMPLISH_25E_STRAIGHT_END_SPEED_MMPS` | mm/s | `0.0f` | 走满最大距离仍未找到线时的终点速度；当前停车 |
| `ACCOMPLISH_25E_LINE_SPEED_MMPS` | mm/s | `200.0f` | 巡线速度 |
| `ACCOMPLISH_25E_LINE_DETECT_CONFIRM_TICKS` | 100 Hz 节拍 | `3U` | 直线阶段连续检测黑线 30 ms 后才进入巡线 |
| `ACCOMPLISH_25E_TURN_TARGET_OFFSET_DEG` | ° | `180.0f` | 每轮在上一绝对目标上增加的角度；目标依次为启动航向加 180°、360°…… |
| `ACCOMPLISH_25E_TURN_SPEED_MMPS` | mm/s | `80.0f` | 180° 转向的每侧轮速度请求 |

### 6.6 `Accomplish/25H.h` 参数

| 宏 | 单位 | 当前值 | 作用 |
|---|---:|---:|---|
| `ACCOMPLISH_25H_START_KEY_MASK` | 按键位图 | `0x01U` | KEY1 的 bit0 掩码；等待状态检测按下沿后启动 25H |
| `ACCOMPLISH_25H_LEFT_MARKER_MASK` | 灰度位图 | `0x03U` | bit0 和 bit1 必须同时为 1 才触发左侧标志 |
| `ACCOMPLISH_25H_LINE_SPEED_MMPS` | mm/s | `200.0f` | 正常巡线速度 |
| `ACCOMPLISH_25H_FORWARD_DISTANCE_MM` | mm | `150U` | 检测到左侧标志后继续向前直行的距离 |
| `ACCOMPLISH_25H_FORWARD_SPEED_MMPS` | mm/s | `200.0f` | 150 mm 定距直行速度 |
| `ACCOMPLISH_25H_FORWARD_END_SPEED_MMPS` | mm/s | `0.0f` | 定距完成后的终点速度；当前减速至零再转向 |
| `ACCOMPLISH_25H_TURN_STEP_DEG` | ° | `-90.0f` | 每轮绝对目标减少的角度；目标依次为启动航向减 90°、180°、270°…… |
| `ACCOMPLISH_25H_TURN_SPEED_MMPS` | mm/s | `80.0f` | Nav 原地转向的每侧轮速度请求 |

`PID_t` 的 `Kp/Ki/Kd`、`integral`、`prevError`、`outMax` 和 `integralMax` 为 PID 实例的公共状态与参数。除上述公开声明外，其余 `static` 数据和源文件内宏均为模块内部实现。
