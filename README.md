# MSPM0G3507 天猛星小车工程索引

> 对项目进行任何修改时，只要涉及硬件连接、Pin 口、外设实例、时钟树、波特率、定时器、主循环调用关系或程序功能，就必须同时修改本文的 Pin 口表、时钟表和对应程序说明。禁止只修改代码而不更新本文档。

本文只记录工程文件类型、文件职责、公共函数和公共参数。函数声明以当前头文件为准；`static` 函数和变量属于文件内部实现，不作为跨模块接口记录。`main.syscfg` 是 PinMux、时钟和外设实例的唯一配置源，`Debug/`、`Release/` 和 `ti_msp_dl_config.*` 均为生成内容。

## 1. 时钟与外设占用

| 资源 | 当前配置 | 占用模块 | 作用 |
|---|---:|---|---|
| CPUCLK / SYSCLK | 32 MHz | 全工程 | SysConfig 默认时钟源；延时、总线外设和 SysTick 的基准时钟 |
| SysTick | 32 MHz / 320000 = 100 Hz | `System/Tick`、`Application/Core/App`、`Graydetect_Update`、`MotionManager`、`Mission`、`Accomplish/25H`、`Accomplish/26H` | 10 ms 系统节拍；`App_Update()` 每拍采样一次六路红外 I2C 状态并生成 dt；26H 用整数 Tick 计时并驱动单圈巡线状态机 |
| TIMG8 | 32 MHz，周期 1600 = 20 kHz | `Hardware/Motor/PWM`、`MotionWheel`、`MotionManager`、`Accomplish/25H`、`Accomplish/26H` | MotionManager 经 MotionWheel 输出双轮 PWM；26H 以平滑巡线速度曲线完成 A 点单圈 |
| TIMG7 | 32 MHz / 32 = 1 MHz，周期 20000 = 50 Hz | `Application/Servo` | PA26/PA27 分别输出横向/纵向普通舵机 PWM；`D`/`O` 命令更新比较值 |
| TIMA0 CCP0 | BUSCLK / 8 = 4 MHz，重复计数器，PB8 | `Hardware/Motor/Stepper` | MS42CG ST 脉冲；当前测试入口已启用 |
| TIMG12 CCP0 | 捕获组合模式，PB13 | `Hardware/Motor/Stepper` | MT6816 PWM 单圈绝对角捕获；当前测试入口已启用 |
| I2C0 | BUSCLK 32 MHz，SCL 400 kHz | `Hardware/Display/OLED` | OLED 控制器通信 |
| UART0 | BUSCLK 32 MHz，115200 baud，8N1，RX + DMA TX 中断，IRQ 优先级 3 | `Hardware/Comms/Serial`、`Application/Comms/K230Link` | PA10/PA11 接 K230；逻辑接口为 `Serial3`，TX 使用 DMA CH2；RX 异常积压时熔断以保护行车实时性 |
| UART1 | 当前 SysConfig 未实例化 | `Serial2` 停用桩、`Application/Comms/CarLink` 软件层 | PB6/PB7 未占用；DMA CH1 空闲，CarLink 当前没有物理链路 |
| UART2 | BUSCLK 32 MHz，115200 baud，8N1，RX + DMA TX 中断 | `Hardware/Comms/Serial`、`Application/Comms/BluetoothDebug`、`App`、`Mission` | PA21/PA22 接 DAPLink，作为 PC/网页链路；逻辑接口为 `Serial1`，TX 使用 DMA CH0 |
| GPIO 软件 I2C（PB2/PB3） | CPU 延时产生时序 | `Hardware/Sensors/MPU6050`、`Application/State/Heading`、`Accomplish/25H` | MPU6050 的 SCL/SDA；不占用 I2C 外设实例 |
| GPIO 软件 I2C（PA25/PA14） | 约 100 kHz、开漏式时序 | `Hardware/Sensors/Graydetect`、`Application/Control/MotionLine`、`Application/Debug/DebugDisplay` | 六路红外模块的 SDA/SCL；读取地址 `0x5C`、状态寄存器 `0x05` |
| GPIOA GROUP1 IRQ | A/B 相双边沿，共享唯一入口 | `Hardware/Motor/Encoder`、`MotionWheel` | PA15/PA16/PA17/PA24 解码左右轮编码器；PA13/PA29 为已预留的步进 AB 反馈，只有启用步进后才打开其中断 |

## 2. Pin 口占用

| Pin | 方向 / 复用 | 占用对象 | 程序说明 |
|---|---|---|---|
| PA10 | UART0 TX | K230 RX | MCU 向 K230 发送握手与拍照请求；逻辑接口 `Serial3`，115200 |
| PA11 | UART0 RX、上拉 | K230 TX | 接收 K230 帧；未连接时保持 UART 空闲高电平，逻辑接口 `Serial3`，115200 |
| PA12 | GPIO 输出 | 右电机 AIN2 | TB6612 A 通道方向 |
| PA13 | GPIO 输入、上拉 | MS42CG AB-A | 步进反馈 A，双边沿中断 |
| PA14 | GPIO 输入 / 软件开漏 | 六路红外 SCL | 软件 I2C 时钟；仅拉低或释放，绝不主动输出高电平 |
| PA15 | GPIO 输入、上拉、双边沿中断 | 右编码器 A | 右轮正交反馈 |
| PA16 | GPIO 输入、上拉、双边沿中断 | 右编码器 B | 右轮正交反馈 |
| PA17 | GPIO 输入、上拉、双边沿中断 | 左编码器 A | 左轮正交反馈 |
| PA19 | SWDIO | 下载调试 | 不作为普通 GPIO 使用 |
| PA20 | SWCLK | 下载调试 | 不作为普通 GPIO 使用 |
| PA21 | UART2 TX | DAPLink RX | MCU 向 PC/网页发送回应和遥测；逻辑接口 `Serial1`，115200 |
| PA22 | UART2 RX、上拉 | DAPLink TX | 接收 PC/网页命令；逻辑接口 `Serial1`，115200 |
| PA24 | GPIO 输入、上拉、双边沿中断 | 左编码器 B | GPIOA GROUP1 IRQ；`MotionWheel` 左轮反馈 |
| PA25 | GPIO 输入 / 软件开漏 | 六路红外 SDA | 软件 I2C 数据；仅拉低或释放，绝不主动输出高电平 |
| PA26 | TIMG7 CCP0 | 横向舵机 PWM | 50 Hz；`D<number>` 更新角度 |
| PA27 | TIMG7 CCP1 | 纵向舵机 PWM | 50 Hz；`O<number>` 更新角度 |
| PA28 | I2C0 SDA | OLED | 400 kHz |
| PA29 | GPIO 输入、上拉 | MS42CG AB-B | 步进反馈 B，双边沿中断 |
| PA30 | GPIO 输入、上拉 | KEY1 | 低电平按下，按键位图 bit0；在 26H 等待/完成/错误页按下时启动或重试单圈 |
| PA31 | I2C0 SCL | OLED | 400 kHz |
| PB0 | GPIO 输出 | 左电机 BIN1 | TB6612 B 通道方向；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB1 | GPIO 输出 | 左电机 BIN2 | TB6612 B 通道方向；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB2 | 开漏式 GPIO | MPU6050 SCL | 软件 I2C 时钟 |
| PB3 | 开漏式 GPIO | MPU6050 SDA | 软件 I2C 数据 |
| PB5 | 未分配 | 旧灰度 CH7 | 旧八路灰度已停用，Pin 已从 `main.syscfg` 释放 |
| PB6 | 未分配 | - | 预留；不再是步进 DIR |
| PB7 | 未分配 | - | 预留；不再是步进 EN |
| PB8 | TIMA0 CCP0 输出 | MS42CG ST | 步进脉冲输出，3200 ST/rev |
| PB9 | GPIO 输出 | MS42CG DIR | 角度增大为上升，角度减小为下降 |
| PB10 | GPIO 输入、上拉 | KEY4 | 低电平按下，按键位图 bit3；第一次停止 O 点闭环并保持步进位置，第二次采样并锁定任意钢球目标位置，第三次启动 30 秒巡线 |
| PB11 | GPIO 输入、上拉 | KEY2 | 低电平按下，按键位图 bit1；第一次停止 O 点闭环并保持步进倾角，第二次启动要求3摆球扫描及计时；与KEY1同时按下为物理急停 |
| PB12 | GPIO 输出 | MS42CG EN | 高电平使能 |
| PB13 | TIMG12 CCP0 捕获 | MS42CG 绝对角 PWM | MT6816 单圈绝对角 |
| PB14 | GPIO 输入、上拉 | KEY3 | 低电平按下，按键位图 bit2；启动 30 秒定时巡线，不执行 KEY1 的终点减速和终点停车逻辑 |
| PB15 | TIMG8 CCP0 | 右电机 PWM | TB6612 A 通道，20 kHz；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB16 | TIMG8 CCP1 | 左电机 PWM | TB6612 B 通道，20 kHz；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB17 | GPIO 输出 | 蜂鸣器 | 低电平有效 |
| PB23 | GPIO 输出 | LED1 | 高电平点亮 |
| PB25 | GPIO 输出 | 右电机 AIN1 | TB6612 A 通道方向 |
| PB26 | 未分配 | 旧灰度 CH1 | 旧八路灰度已停用，Pin 已从 `main.syscfg` 释放 |
| PB27 | GPIO 输出 | LED2 | 高电平点亮 |

### 2.1 硬件连接检查

- `main.syscfg` 中没有重复分配的 Pin；UART0、UART2、I2C0、TIMA0、TIMG7、TIMG8、TIMG12、两组软件 I2C 和 SWD 互不冲突。
- 旧八路灰度 GPIO 已废弃；其中 PA13、PA29、PB8、PB9、PB12、PB13 已改为 MS42CG 的新专用资源，不能再作为传感器输入使用。
- 六路红外接线固定为：模块 `+5V -> +5V`、`GND -> GND`、`SDA -> PA25`、`SCL -> PA14`。教程线序对应红、黑、蓝、绿。
- 模块的 SDA/SCL 空闲高电平必须是 **3.3 V**；若模块板载上拉到 5 V，必须先加双向 I2C 电平转换器，不能直接接入 PA25/PA14。
- 模块地址为 `0x5C`，每拍读取寄存器 `0x05` 的低 6 位：bit0=CH1，…，bit5=CH6；`1` 表示已学习的目标颜色。先按教程完成背景长按学习、目标短按学习。
- **安装方向**：实车俯视（探头朝前）时 CH1 在左、CH6 在右，对应 `GRAYDETECT_CHANNEL1_IS_RIGHT = 0`。翻面安装只需把该宏改回 `1`，`MotionLine.c` 里两张权重表都已备好，不要去改权重符号。方向接反会让巡线修正朝错误一侧、车直接冲出赛道，上电首测务必先确认：手动把车偏到线左侧，右轮应加速。
- DAPLink 接 PA21/PA22（UART2），承载 PC/网页命令、回应和遥测；K230 迁移到 PA10/PA11（UART0、DMA CH2）。
- 原 UART1/HC05 配置已移除，`Serial2` 保留为停用桩；MS42CG 使用 PB8/PB9/PB12、PA13/PA29、PB13。步进驱动和 MT6816 反馈均已启用。
- F32C/Gimbal 仍停用；PA26/PA27 舵机 PWM 继续由 TIMG7 提供。

## 3. 当前程序说明

`main.c` 是当前唯一运行入口，不再通过 `MAIN_STEPPER_TEST_MODE` 或 `Application/Core/Main26H.c` 切换主流程。上电前需手动把摆杆调到平衡位置；烧录启动后步进不会自动转到固定角度，连续取得3帧有效 MT6816 PWM 后，`BeamActuator` 自动把此刻实测绝对角保存为本次上电的水平零偏。零偏捕获完成且视觉就绪后，无按键状态才默认启动要求3的 O 点保持。实体 `KEY2` 第一次解除 O 点闭环并保持当前倾角，第二次启动摆球扫描；网页 `C3` 仍可直接切换到指定目标保持。

六路红外资源已经恢复为 `PA25=SDA、PA14=SCL`，但只有完整 26H 入口中的 `App_Init()` 会初始化并采样它们。

当前 `main.c` 入口启用 PC/网页链路、K230Link、六路红外、MPU6050、舵机、底盘和步进摆杆。不按按键时，程序等待 K230 钢球位置有效，且步进已使能、就绪、PWM 反馈有效并结束当前运动后，默认启动 O 点 `0.0 mm` 保持；启动不再要求实测绝对角落在水平设定角的固定容差内。`KEY1` 保留为 H 题要求 2 的单圈巡线启动键，`KEY2` 通过两次按键依次解除 O 点闭环和启动要求3摆球扫描，`KEY3` 启动不识别终点的 30 秒定时巡线，`KEY4` 通过三次按键依次完成解除 O 点闭环、任意位置锁定和同款定时巡线，`KEY1+KEY2` 同时按下保持原有物理急停逻辑。

### 3.2 100 Hz 主循环与 26H 单圈巡线

完整26H模式加载`Accomplish/26H.c`独立控制器，实现H题要求2。KEY1在READY、完成或错误后按下时清零累计Tick并启动平滑巡线。正常巡线的左右差速由六路红外权重 PID 生成；里程计路程大于 `ACCOMPLISH_26H_FINISH_MARKER_ARM_DISTANCE_MM`（默认 1700 mm）后，若六路灰度中至少 3 路同时检测到黑线并连续维持 `ACCOMPLISH_26H_MARKER_CONFIRM_TICKS`（默认 2）个 100 Hz 节拍，立即进入 `MotionManager_StartBrake()` 主动刹车并在静止确认后冻结计时。**KEY1+KEY2 同时按下**为物理急停。

KEY3 由 `main.c` 直接分发给 `Application/Control/TimedLineRun`，不进入 `Accomplish/26H` 总任务。它复用同一套六路红外 `MotionLine` 巡线与 O 点摆球保持，默认以 `100 mm/s²` 加速到 `400 mm/s`，运行 30 秒后请求软停。KEY3 控制器不读取终点横线、不切换终点慢速，也不按圈长或最大里程停车。`k3acc`、`k3cru` 和 `k3dur` 可通过 K 参数或网页“KEY3 定时巡线”阶段调整，并在下一次 KEY3 启动时快照。

KEY4 同样由 `main.c` 分发并复用 `BallSequence` 与 `TimedLineRun`。第一次按 KEY4 只停止默认 O 点保持，不会重新初始化步进或额外回中：停止前保存当前已下发倾角，解除旧闭环后继续保持该执行器状态。第二次按 KEY4 才开始目标采样；`BallTargetCapture` 只统计此后序号不同的 K230 钢球帧，默认连续 8 帧落在当前平均值 ±5 mm 内后，把平均位置锁定为指定目标并自动开始保持。待现有 `BallBalance` 稳定判据满足后，第三次按 KEY4 才会以该指定位置为目标启动与 KEY3 完全相同的 30 秒巡线。采样帧数和容差集中在 `Application/Control/BallTargetCapture.h`。

当前 KEY1 测试入口的停车触发点由 `K42`（`h2arm`，单位 mm）控制，初值为 1700 mm；该距离以前即使检测到 3 路以上黑线也不会触发终点。`K30`（`h2off`）仍作为保留的停车偏移参数参与启动快照，但当前终点条件满足后直接主动刹车，不再用它延后巡线软停。

`App_Init()` 继续初始化并校准 MPU6050，`App_Update()` 继续更新 Heading 和里程，并在此后采样一次六路红外状态；保留的航向运动和调试命令仍可使用。OLED不再显示MPU校准、巡线、按键、编码器或钢球页面，只显示上电运行时间。

### 3.2.1 26H 摆杆滚球（要求 3）

上电后的无按键默认状态使用现有 `BallSequence_Start(0.0f)` 和 `BallBalance` PID 闭环把钢球保持在 O 点；视觉或步进尚未就绪时保持等待，不会提前驱动。第一次按 `KEY2` 只停止当前保持任务并保持步进已下发倾角，第二次按下才启动 -50 mm 到 +50 mm 扫描及要求3计时；`C3` 独立地启动或重定向到网页给定位置，`C4` 停止正在运行的摆球任务并回中。`C4` 或 KEY1+KEY2/C0 急停会同时取消本次上电默认启动等待，不会停车后自行重新启动。

分四层，每层只做一件事：`BallSensor` 把 K230 的 `BALL_POSITION` 换算成以 O 为原点的毫米位置并估计滚动速度；`BallBalance` 用 PID 算出摆杆倾角，暂不加入车体加速度前馈；`BeamActuator` 把倾角换算成步进的绝对角度；`Application/Control/BallSequence` 只管启动、持续更新、停止和错误保护。K230 的 `-50.00~+50.00`覆盖250 mm水管，因此端点对应`-125~+125 mm`。

三处关键保护已在实现中处理：目标位置先经过轨迹发生器；钢球速度按`BALL_POSITION`帧间实际时间估计，同一序号重复读取时不做差分；视觉失效后立即标记数据陈旧，闭环超过保护时间会回中报错。

摆杆由步进电机驱动，MT6816 PWM和AB反馈均启用。`238.0°`是水平绝对角，软限位为`106.0°~309.0°`。控制器正倾角与机构电机角方向相反：球位为正且需要回到O点时，执行器会增大电机绝对角并抬高正位置一侧。上层 PID 和执行器换算层不再额外限制倾角，最终只由 `Stepper_TrackToSteps()`/`TrackToAngle()` 的绝对角软限位约束；106°和309°只是软件允许边界，必须在机构确认不会碰撞后才能手动测试极限。

```c
#include "Accomplish/26H.h"
#include "Application/Control/BallSequence.h"
#include "Application/Control/BeamActuator.h"
#include "Application/Core/App.h"
#include "Application/Debug/DebugDisplay.h"
#include "Application/Debug/Telemetry.h"
#include "System/Interrupt.h"

int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Accomplish26H_Init();
    BallSequence_Init();
    Interrupt_Enable();

    for (;;)
    {
        if (App_Update(&updateContext) != 0U)
        {
            Accomplish26H_Update(&updateContext);
            /* KEY2 两阶段启动扫描，C3 启动目标保持；C4 停止 BallSequence。 */
            BallSequence_Update(updateContext.dt);
            Telemetry_Update(updateContext.elapsedTicks, updateContext.pressedKeys);
            BeamActuator_Update(updateContext.dt);
            DebugDisplay_Update(updateContext.elapsedTicks);
        }
    }
}
```

当前入口的`App_Init()`初始化整车、OLED、MPU6050、左右轮编码器、串口、K230Link、六路红外和步进接口。`Stepper_Init()`连续取得3帧有效 PWM 后建立绝对角基准，但不执行启动运动；`BeamActuator`随后把该次上电的实测绝对角写入 `BeamActuator_TuneZeroOffsetDeg`，作为控制器倾角 `0°` 对应的水平位置。

```text
Heading -> Odometry -> 六路红外 I2C -> Stepper -> 按键边沿 -> CarLink -> K230Link
        -> BallSensor（须在 K230Link 之后）-> BluetoothDebug
        -> C0 全局停车（同时停摆杆闭环）-> MotionManager -> K230 ACK -> Beep
        -> Accomplish26H_Update -> KEY2两阶段扫描/C3目标保持 -> BallSequence_Update
        -> BeamActuator_Update -> OLED时间局部刷新
```

`BallSensor_Update()` 必须排在 `K230Link_Update()` 之后，钢球位置来自本拍刚解析的 `BALL_POSITION(0x14)` 帧；`BeamActuator_Update()` 必须排在任务层之后，让本拍算出的倾角当拍下发给步进。

保留的 Mission 使用静态状态图，不使用 `malloc`。每个状态含 `onEnter/onUpdate/onExit`、有序转换表和 `interruptible`。动作运行时只检查打断转换；动作完成后只检查正常转换；每拍最多转换一次。被打断状态调用退出回调并停车，不保存或恢复原进度。当前 `main.c` 不加载 Mission。

`Accomplish/25H.c` 静态状态图继续保留，但当前不加载。它的 KEY1 巡线、150 mm 直行和连续绝对左转行为没有改动。

OLED每100个系统节拍刷新一次，即1 Hz，只局部更新第0行时间数值区域：

| 显示 | 含义 |
|---|---|
| `T:00000s` | 上电后运行秒数，只用于确认主循环仍在跑 |

固定标签和空白区域不会在每拍重新发送；刷新使用`OLED_UpdateArea()`，不再调用全屏`OLED_Update()`。

K230Link 当前通过逻辑接口 `Serial3`、物理 UART0 的 PA10/PA11 运行；未接模块时维持离线，以下为实际使用的帧格式：

```text
Heading -> Odometry -> 六路红外 -> Stepper -> Key -> K230Link -> BallSensor
        -> BluetoothDebug -> C0 全局停止 -> MotionManager -> Beep
        -> Accomplish26H_Update -> BallSequence -> BeamActuator -> OLED局部刷新
```

- `VER=0x01`；`TYPE` 为 `READY=0x01`、`READY_ACK=0x02`、`TARGET=0x10`、`CAPTURE=0x20`、`CAPTURE_ACK=0x21`。
- `SEQ` 为 8 位帧序号，`LEN` 最大为 32。
- CRC 使用 CRC-8/ATM，多项式 `0x07`、初始值 0，校验范围为 `VER` 到 `PAYLOAD`。
- TARGET 的 PAYLOAD 固定为 `valid:u8 + offsetX:int16_LE + offsetY:int16_LE`，共 5 字节。
- CAPTURE（MCU → K230）的 PAYLOAD 为 `count:u8`，共 1 字节；`count` 为连拍张数，范围 `1~20`。
- CAPTURE_ACK（K230 → MCU）的 PAYLOAD 为 `ok:u8 + index:uint16_LE`，共 3 字节；`ok=1` 表示成功，`index` 为存入 TF 卡的起始文件序号。
- K230 `uart_io.py` 测试入口可在握手后持续发送 `valid=1、offsetX=123、offsetY=-45`。

### 3.3 调试串口任务与命令协议

本节的命令走逻辑接口 `Serial1`，物理链路是 **UART2 的 PA21/PA22 DAPLink 串口**（115200 8N1）；PB6/PB7 当前未被新的步进映射占用。源码模块名 `BluetoothDebug` 是历史名称；凡提到「蓝牙命令」处，均指本节这套 PC/网页命令协议。

命令不区分大小写。推荐以 `\r` 或 `\n` 结束；也支持空格、逗号、分号作为分隔符。没有结束符时，接收空闲 3 个系统节拍（30 ms）后执行。每条命令都会返回 `OK ...` 或 `ERR ...`。

`C` 命令进入单槽任务事件邮箱；App 每个有效节拍最多提供一个任务信号。普通信号不排队，同一拍只保留最后收到的一条；`C0` 具有最高优先级，待处理时不会被普通信号覆盖，并立即停车。当前 `C3` 直接启动要求 3 的目标保持，与实体 `KEY2` 的两阶段摆球扫描流程相互独立；`C4` 停止摆球闭环并让摆杆回中。除 `C0/C3/C4` 外的非零信号继续保留给其他任务。

| 命令 | 作用 | 输入范围与限位 | 示例 |
|---|---|---|---|
| `C0` | 全局急停，停止底盘与摆球并冻结 26H 计时 | 始终有效且优先级最高 | `C0` |
| `C3` | 直接启动要求 3 的 PID 目标保持任务，当前默认目标 O 点 `0.0 mm`；不经过实体 `KEY2` 的两阶段扫描流程 | 钢球视觉必须就绪、底盘与摆球任务必须空闲；成功回 `OK BALL START` | `C3` |
| `C4` | 结束要求 3，停止闭环并让摆杆回中；不是全局急停 | 成功回 `OK BALL STOP` | `C4` |
| `C<number>`（其他非零值） | 发送保留的 Mission 单次任务信号 | `1~255`，当前任务未定义的编号不消费 | `C1` |
| `L<number>` | 只更新左轮 PWM，右轮保持上次指令 | `-1000~1000`，超限自动夹紧 | `L10` |
| `R<number>` | 只更新右轮 PWM，左轮保持上次指令 | `-1000~1000`，超限自动夹紧 | `R10` |
| `U<number>` | 左右轮使用相同 PWM | `-1000~1000`；正数前进，负数后退 | `U100` |
| `O<number>` | 纵向舵机移动到指定角度 | 当前限位 `0°~270°` | `O10` |
| `D<number>` | 横向舵机移动到指定角度 | 当前限位 `0°~270°` | `D10` |
| `G<number>` | 设置二进制遥测输出频率；`G0` 关闭输出 | `0~100` Hz（硬上限）；实际安全上限由当前字段掩码动态决定，超限返回 `ERR RANGE MAX=<当前安全上限>` | `G20` |
| `M<number>` | 设置遥测**通道掩码**（32 位，见 3.3.2），改变时发一帧二进制 SCHEMA | `1~131071`（`0x1FFFF`），`0` 返回 `ERR RANGE`；成功回报新频率 `OK M=<mask> G=<hz>` | `M81920`（bpos+sang） |
| `V<number>` | 设置调试巡航速度 | `20~800` mm/s，默认 200 | `V200` |
| `F<number>` | 前进定距，终点速度 0 | `1~10000` mm，忙时 `ERR BUSY` | `F300` |
| `B<number>` | 后退定距，终点速度 0 | `1~10000` mm，忙时 `ERR BUSY` | `B300` |
| `T<number>` | 相对转角，带符号 | `-3600~3600` 度，忙时 `ERR BUSY` | `T90` |
| `A<number>` | 绝对连续航向角 | `-3600~3600` 度，忙时 `ERR BUSY` | `A90` |
| `Z<number>` | `Z1` 只把当前朝向定为 0° 基准；`Z2` 静止重采 MPU6050 Z 轴零漂并把航向归零 | 仅接受 1/2，忙时 `ERR BUSY`；`Z2` 需保持车辆静止约 0.8 秒，离线返回 `ERR Z OFFLINE`，正在 E/Y 尺度标定时返回 `ERR Z CALIBRATING` | `Z2` |
| `P<number>` | 请求 K230 连拍并存 TF 卡 | `1~20`，链路未就绪返回 `ERR CAP NOLINK` | `P1` |
| `W<number>` | 闭环恒速模式：双轮同目标速度、无规划斜坡、无航向修正，是轮速 PI 的标准阶跃激励；运动中重复发 `W` 直接改目标（不复位 PID，可链式阶跃）；`W0` 停止并释放电机 | `-800~800` mm/s；其他模式忙时 `ERR BUSY`，`W0` 只停恒速模式 | `W300` |
| `N<number>` | 直接启动巡线（不经 Mission 状态图）；`N0` 停止 | `20~800` mm/s；其他模式忙时 `ERR BUSY`，`N0` 只停巡线模式；丢线后自动完成 | `N200` |
| `K?` / `K<id>?` / `K<id>=<float>` | 运行时读写控制参数（见 3.3.1 参数表）：列表 / 读单个 / 写入（支持小数与负号，写入立即生效） | id `1~62`；越界返回 `ERR K RANGE MIN=<min> MAX=<max>`，格式错误返回 `ERR K FORMAT` | `K28=0.1` |
| `E<number>` | 陀螺仪尺度标定：`E1` 开始（清零标定角），`E0` 取消 | 仅 `0/1`；运动中 `ERR BUSY`，MPU 离线 `ERR CAL OFFLINE` | `E1` |
| `Y<number>` | 原地转 n 整圈回到起始朝向后结束标定，解算并应用尺度因子 | `1~20` 圈；未在标定中返回 `ERR CAL IDLE`，积分角过小返回 `ERR CAL SMALL`；成功回 `OK Y SCALE=<新因子> RAW=<积分角>` | `Y3` |
| `Q` | 查询遥测能力；**上位机据此自适应频率，不再各存一份阈值** | 无参数（裸 `Q` 即可）；回 `OK Q MAX=<上限Hz> MASK=<掩码> RATE=<当前Hz>` | `Q` |

> **板载捕获已移除。** 原先的 `X` 板载捕获功能连同它的 24 KB RAM 缓冲一并删除；当前 `X<mm>` 专用于设置要求 3 小球目标，不再代表板载捕获。`Q` 回应中原有的 `CAPST`/`CAPN`/`CAPMAX` 三个字段同时撤销；`car_debug.html` 的 Q 解析器本就把这三项写成可选组，无需改动。

**`ERR MOTION 3` 的含义。** MotionManager 错误码 `3` 是 `SENSOR_NOT_READY`。若 `B/F/T/A` 都返回 `ERR MOTION 3`，同时 `Z2` 返回 `ERR Z OFFLINE`，说明动作命令没有真正启动，根因是 Heading 层未就绪，通常是 MPU6050 离线或初始化失败。此时 `yaw/navE` 遥测会作为无效字段发送，网页显示为空值，不再把默认 `0°` 当成有效航向；优先检查 MPU6050 供电、GND、PB2/PB3 软件 I2C 线序、上拉/开漏连接和地址 `0x68`。

**`T` 与 `A` 的参考系差异。** `T` 是相对转角，从当前航向再偏转指定度数，支持带符号输入（`T-90` 反转 90°）。`A` 是绝对连续航向角，目标为 `Heading_GetYaw()` 的累计值空间中的绝对角度。两者底层均使用 `Nav`，**`Nav` 不做 ±180° 最短路径优化**：当前连续航向为 370° 时，`A90` 会计算误差 `90 - 370 = -280°`，选择倒转 280° 而不是顺转 80° 走最短路径。若想总是最短路径到达某个方向，应在上位机计算出当前航向与目标方向之间的最短差值后，改用 `T` 命令发送相对角。`Z1` 只把当前朝向重置为 0°，不会更新零漂；车辆静止时可用 `Z2` 重新采样零漂并归零，再用 `A` 命令指向绝对角度。

`L/R/U` 只在 MotionManager 空闲时执行，自动运动期间返回 `ERR BUSY`。数字仍是开环 PWM，不是 mm/s；左右轮编码器数据仍可通过遥测读取，OLED不再显示这些数据。

**`G` 命令的上限仍由当前掩码动态计算。** 发送已经改为 DMA，不再阻塞控制环；限流现在用于守住 115200 8N1 的串口总带宽。当前 17 个通道全开时 SAMPLE 帧为 79 字节，100 Hz 约 7.9 KB/s，仍在 70% 带宽预算内。网页会按调参阶段选字段并通过 `Q` 读取固件给出的真实上限。

### 3.3.0 二进制 DMA 遥测架构

> **本节描述当前架构。** 遥测数据已从 ASCII CSV 改为二进制帧 + DMA 非阻塞发送。
> 线上的帧定义以 `Application/Debug/TelemFrame.h` 和本节为准；下文“遥测 CSV 格式”等 ASCII 描述是**历史架构**，已被本节取代。

**核心变化。** `Serial1`（当前落在物理 UART2）的发送从逐字节阻塞（`transmitDataBlocking`）改为 **DMA 搬运 + 环形缓冲**：主循环只往缓冲写、从不等硬件，DMA 完成中断推进下一段。发送不再占用主循环，遥测带宽预算从 20% 放宽到 70%。数据从 ASCII CSV（一个 float 占 11 字节）改为**二进制帧**（float32 只占 4 字节，压缩 2.5×）。两者叠加，单侧实时流上限从阻塞 ASCII 的 57 Hz 提到约 **450 Hz** 理论值，实际夹在 100 Hz 硬上限——**远超 100 Hz 控制环，实时流本身即无损**。

**帧格式**（与 K230Link 同范式，`0xAA` 起头与 ASCII 命令回应可靠共存于同一串口）：

```text
0xAA 0x55 | VER | TYPE | SEQ | LEN | PAYLOAD | CRC8
```

- `0xAA` 是非 ASCII 可打印字符，绝不出现在文本回应里；网页见 `0xAA` 进二进制解析，否则累积成 ASCII 文本行。
- `CRC8` 为 CRC-8/ATM（多项式 `0x07`，初值 0），覆盖 `VER` 到 `PAYLOAD`。
- 帧类型：`SCHEMA=0x30`（掩码变化时先发，含列名+单位码）、`SAMPLE=0x31`（ms + 各通道 float32）。`0x32`~`0x34` 曾是板载捕获的 `CAP_META`/`CAP_SAMPLE`/`CAP_END`，模块删除后这三个类型号保留不复用，避免旧版网页把新帧误认成捕获数据。

**命令仍走 ASCII。** `K/W/N/M/G/F/B/T/A/Z/Q/E/Y/P/C/X/L/R/U/O/D` 及其 `OK`/`ERR` 回应保持文本——低频、要人读、任何串口助手可直接调试。要求 3 中 `X<mm>` 只设置小球目标并返回 `OK X=<mm>`，不会启动任务或驱动车辆；`C3` 才启动要求 3，运行中再次发送 `X<mm>` 只重定向 PID 目标。只有高频遥测数据二进制化。

**单通道：实时流。** `M` 设掩码、`G` 设频率，`SCHEMA` + `SAMPLE` 两种帧，边跑边发（DMA 不阻塞），可达 100 Hz 与控制环同频，容量无限。监视、遥控采数据、调参全部够用。

**板载捕获（通道 B）已删除。** 它在第一次架构（ASCII 双通道）里引入，唯一理由是"实时流 30 Hz 低于控制环 100 Hz"。二进制 + DMA 之后实时流本身就是 100 Hz 无损，捕获只剩两个边缘场景（录得比串口实时率更久、绝对零丢包），却要独占 **24 KB RAM——32 KB SRAM 的 75%**。

这不是"占地方"而已：`.stack` 被 SysConfig 硬编码为 512 字节且位于 SRAM 顶部向下生长，紧邻其下就是 `.data`。捕获缓冲把栈可用余量压到 1587 字节，而实测最深调用链约 1900 字节，栈越界改写了 `.data` 里的 `Heading::s_scale`、`Odometry_CountsPerMM` 和各运动模块的增益，表现为整车卡死（OLED 航向角固定、`LD/RD/LV/RV` 恒为 0、灰度却正常刷新）。删除后 SRAM 占用从 96.7% 降到 21.7%，栈可用余量 26175 字节。

### 3.3.2 通道掩码（`M` 命令）

32 位通道掩码定义见 `TELEMETRY_CH_*`，由 `M<mask>` 选择实时流通道。

| 位 | 通道 | 单位 | 来源 |
|---:|---|---|---|
| `0x001` | `TL` | mm/s | 左轮目标速度 |
| `0x002` | `LV` | mm/s | 左轮实测速度 |
| `0x004` | `PL` | pwm | 左轮输出 PWM |
| `0x008` | `TR` | mm/s | 右轮目标速度 |
| `0x010` | `RV` | mm/s | 右轮实测速度 |
| `0x020` | `PR` | pwm | 右轮输出 PWM |
| `0x040` | `yaw` | 度 | 连续累计航向角；Heading 离线时该字段无效/空值 |
| `0x080` | `navE` | 度 | 转向角误差；Heading 离线时该字段无效/空值 |
| `0x100` | `lerr` | — | 巡线权重误差 |
| `0x200` | `gray` | 位图 | 兼容字段名；低 6 位为六路红外 CH1~CH6 状态 |
| `0x400` | `LD` | mm | 左轮累计路程 |
| `0x800` | `RD` | mm | 右轮累计路程 |
| `0x1000` | `vx` | — | K230 最近视觉带偏差，车道偏右为正 |
| `0x2000` | `vad` | mm/s | 视觉差速修正量，正值表示右转 |
| `0x4000` | `bpos` | mm | 要求 3 钢球实测位置；视觉未就绪时为无效值 |
| `0x8000` | `bref` | mm | 要求 3 当前目标位置，与 `bpos` 同一控制拍采样 |
| `0x10000` | `sang` | deg | 步进 MT6816 PWM 单圈绝对角度；PWM 未就绪时为无效值 |

`TELEMETRY_CH_ALL = 0x1FFFF`。位序即帧列序，一经发布不得重排。二进制 SAMPLE 帧字节数 = 7（帧开销）+ 4（ms）+ 通道数×4；全 17 通道 79 字节，100 Hz 时约 7.9 KB/s。当前摆球调试默认使用 `M81920`（`0x4000|0x10000`），只发送 `bpos/sang`，可稳定跑到 100 Hz。

### 3.3.1 运行时参数表（K 命令）

所有控制参数的 `#define` 使用点已改为同名运行时变量：上电取头文件默认值，可用 `K<id>=<float>` 在线修改并立即生效（PID 增益经 apply 钩子写入 PID 实例），**掉电不保存**。调好后把数值写回对应 `.h` 的 `#define` 固化；网页参数面板也可把整组参数存进浏览器，下次上电一键回写。

| id | 名 | 对应默认值宏 / 变量 | 单位 | 范围 |
|---:|---|---|---|---|
| 1 | `wkp` | 双轮 Kp 批量兼容入口（读取返回左右均值） | PWM/(mm/s) | 0~50 |
| 2 | `wki` | 双轮 Ki 批量兼容入口（读取返回左右均值） | PWM/mm | 0~50 |
| 3 | `wil` | 双轮积分限批量兼容入口（读取返回左右均值） | mm | 0~1000 |
| 4 | `wff` | 双轮前馈斜率批量兼容入口（读取返回左右均值） | PWM/(mm/s) | 0~10 |
| 5 | `wsf` | 双轮静摩擦批量兼容入口（读取返回左右均值） | PWM | 0~500 |
| 6 | `skp` | `MOTION_STRAIGHT_HEADING_KP` | PWM/° | 0~100 |
| 7 | `skd` | `MOTION_STRAIGHT_HEADING_KD` | PWM/(°/s) | 0~50 |
| 8 | `sac` | `MOTION_STRAIGHT_ACCELERATION_MMPS2` | mm/s² | 10~5000 |
| 9 | `lra` | `MotionLine_TuneKpMMpsPerWeight` | mm/s 每 权重 | 0~200 |
| 10 | `lkd` | `MotionLine_TuneKdMMpsPerWeight` | mm/s 每 权重/s | 0~200 |
| 11 | `nvx` | `NAV_MAX_TURN_SPEED_MMPS` | mm/s | 10~500 |
| 12 | `nvn` | `NAV_MIN_TURN_SPEED_MMPS` | mm/s | 1~500 |
| 13 | `nsa` | `NAV_SLOWDOWN_ANGLE_DEG` | ° | 5~180 |
| 14 | `ntl` | `NAV_ANGLE_TOLERANCE_DEG` | ° | 0.5~20 |
| 15 | `gsc` | 陀螺仪尺度因子（`Heading_Get/SetScale`，默认 1.0） | 比例 | 0.5~2 |
| 16 | `cpm` | `Odometry_CountsPerMM`（默认 6.44086） | 计数/mm | 0.5~50 |
| 17 | `lwkp` | `MotionWheel_TuneLeftKp` | PWM/(mm/s) | 0~50 |
| 18 | `lwki` | `MotionWheel_TuneLeftKi` | PWM/mm | 0~50 |
| 19 | `lwil` | `MotionWheel_TuneLeftIntegralLimit` | mm | 0~1000 |
| 20 | `lwff` | `MotionWheel_TuneLeftFeedforwardPWMPerMMps` | PWM/(mm/s) | 0~10 |
| 21 | `lwsf` | `MotionWheel_TuneLeftStaticFrictionPWM` | PWM | 0~500 |
| 22 | `rwkp` | `MotionWheel_TuneRightKp` | PWM/(mm/s) | 0~50 |
| 23 | `rwki` | `MotionWheel_TuneRightKi` | PWM/mm | 0~50 |
| 24 | `rwil` | `MotionWheel_TuneRightIntegralLimit` | mm | 0~1000 |
| 25 | `rwff` | `MotionWheel_TuneRightFeedforwardPWMPerMMps` | PWM/(mm/s) | 0~10 |
| 26 | `rwsf` | `MotionWheel_TuneRightStaticFrictionPWM` | PWM | 0~500 |
| 27 | `vkp` | `MotionLane_TuneKp` | 比例 | 0~5 |
| 28 | `vkd` | `MotionLane_TuneKdYaw` | 比例 | 0~10 |
| 29 | `vra` | `MotionLane_TuneMaxAdjustRatio` | 比例 | 0.05~1 |
| 30 | `h2off` | `Accomplish26H_TuneFinishRolloutMM` | mm | 0~300 |
| 31 | `bkp` | `BallBalance_TunePositionKpPerS` | 1/s | 0~10 |
| 32 | `bkd` | `BallBalance_TuneVelocityKpDegPerMMps` | 度/(mm/s) | 0~1 |
| 33 | `bki` | `BallBalance_TuneVelocityKiDegPerMM` | 度/mm | 0~0.2 |
| 34 | `bhl` | `BallSensor_TuneHalfLengthMM` | mm | 50~200 |
| 35 | `bgr` | `BeamActuator_TuneGearRatio` | 倍 | 0.1~50 |
| 36 | `bzo` | `BeamActuator_TuneZeroOffsetDeg`（上电自动捕获，可继续微调） | 度 | 0~360 |
| 37 | `h2cru` | `Accomplish26H_TuneCruiseSpeedMMps` | mm/s | 20~2000 |
| 38 | `h2fin` | `Accomplish26H_TuneFinishCrawlSpeedMMps` | mm/s | 10~2000 |
| 39 | `h2clr` | `Accomplish26H_TuneStartClearDistanceMM` | mm | 0~1000 |
| 40 | `h2lap` | `Accomplish26H_TuneNominalLapDistanceMM` | mm | 1000~20000 |
| 41 | `h2app` | `Accomplish26H_TuneFinishApproachDistanceMM` | mm | 0~5000 |
| 42 | `h2arm` | `Accomplish26H_TuneFinishMarkerArmDistanceMM` | mm | 0~20000 |
| 43 | `h2max` | `Accomplish26H_TuneMaxLapDistanceMM` | mm | 1000~25000 |
| 44 | `lacc` | `MotionLine_TuneAccelerationMMps2` | mm/s² | 10~5000 |
| 45 | `ldec` | `MotionLine_TuneDecelerationMMps2` | mm/s² | 10~5000 |
| 46 | `lcra` | `MotionLine_TuneKpMMpsPerWeight`（兼容别名） | mm/s 每 权重 | 0~200 |
| 47 | `lckd` | `MotionLine_TuneKdMMpsPerWeight`（兼容别名） | mm/s 每 权重/s | 0~200 |
| 48 | `lcv` | 保留的弯道速度兼容参数 | mm/s | 20~1000 |
| 49 | `lch` | 保留的弯道保持距离兼容参数 | mm | 100~5000 |
| 50 | `bvm` | `BallBalance_TuneMaxVelocityMMps` | mm/s | 10~500 |
| 51 | `bff` | `BallBalance_TuneFeedforwardDegPerMMps2` | 度/(mm/s²) | 0~0.1 |
| 52 | `bft` | `BallBalance_TuneFeedforwardSpeedThresholdMMps` | mm/s | 0~500 |
| 53 | `lki` | `MotionLine_TuneKiMMpsPerWeight` | mm/s 每 权重·s | 0~50 |
| 54 | `k3acc` | `TimedLineRun_TuneAccelerationMMps2` | mm/s² | 10~5000 |
| 55 | `k3cru` | `TimedLineRun_TuneCruiseSpeedMMps` | mm/s | 20~2000 |
| 56 | `k3dur` | `TimedLineRun_TuneDurationSeconds` | s | 1~60 |
| 57 | `k2kp` | `BallSequence_TunePositionKpPerS` | 1/s | 0~10 |
| 58 | `k2kd` | `BallSequence_TuneVelocityKpDegPerMMps` | 度/(mm/s) | 0~1 |
| 59 | `k2ki` | `BallSequence_TuneVelocityKiDegPerMM` | 度/mm | 0~0.2 |
| 60 | `k2pkp` | `BallSequence_TunePositivePositionKpPerS` | 1/s | 0~10 |
| 61 | `k2pkd` | `BallSequence_TunePositiveVelocityKpDegPerMMps` | 度/(mm/s) | 0~1 |
| 62 | `k2pki` | `BallSequence_TunePositiveVelocityKiDegPerMM` | 度/mm | 0~0.2 |

id 一经发布不得重排，新增参数只能在尾部追加。K1~K5 为旧上位机保留：写入会同时覆盖左右轮，读取返回两侧当前值的平均数；新调参应使用 K17~K26 分别设置左右轮。基础前馈公式为 `PWMbase = speed×ff + sign(speed)×sf`，再叠加该轮 PI 与上层 trim，最终夹到 ±1000；这是底层轮速环标定，不属于巡线层额外前馈。要求 2、KEY3 和巡线 PID 参数都在对应任务下次启动时快照，当前运行中写入不会切换本圈参数。巡线层只保留统一 PID，不再有弯道低速上限、压线降速、差速变化率限幅或弯道保持距离。`ldec` 沿用 MotionLine 统一减速度，同时负责终点降速和最终软停，不增加额外状态机配置。`gsc` 由 `E1`→原地转 N 圈→`Y<n>` 标定流程自动写入；`cpm` 建议用网页里程标定向导。

K31~K36 是要求 3 摆球的标定量。上电前先手动把摆杆放到平衡位置，固件会自动捕获 `bzo`；如仍有机械水平误差，可在网页读取自动值后小幅微调。其余参数仍需实车标定：先确认 `bgr`，再用钢球放在 0/±25/±50 mm 处校准 `bhl`，然后调 `bkp` 让钢球能回目标，调 `bkd` 压住过冲和往复。固件中的 `bki` 初值为 0.20；实车首次整定建议先通过网页设为 0，只有确认 P/D 已经调顺且仍存在稳定偏差时再小量加入。

K57~K62 是实体 KEY2 扫描独占的两套串级控制参数。阶段一 O→-50 mm 使用 `k2kp/k2kd/k2ki`，阶段二 -50→+50 mm 使用 `k2pkp/k2pkd/k2pki`；每套均依次对应位置外环 P、速度内环 P、速度内环 I。KEY2 启动时会同时快照两套参数，因此当前扫描中修改只对下一次 KEY2 生效，也不会影响默认 O 点保持、C3、KEY3 或 KEY4。网页“要求3摆球”中可分别进入两个 KEY2 PID 阶段调整。

**遥测 CSV 格式（⚠️ 历史架构，已被 3.3.0 的二进制帧取代）。** 以下 ASCII CSV 描述对应第一次架构；当前固件发二进制 SCHEMA/SAMPLE 帧，通道定义见 3.3.2。保留本段仅为理解演进历史。每次字段掩码改变时输出一行表头 `H,...`，随后每隔 `1000/G` ms 输出一行数据：

| 列名 | 对应掩码位 | 列数 | 内容 |
|---|---|---|---|
| `ms` | 固定输出 | 1 | 自上电以来的系统节拍累计毫秒数 |
| `yaw` | `TELEMETRY_FIELD_YAW` = `0x01` | 1 | Z 轴连续累计航向角（度，`%.2f`），不做 ±180° 归一化 |
| `gray`,`keys` | `TELEMETRY_FIELD_SENSOR` = `0x02` | 2 | `gray` 是兼容字段名，低 6 位为六路红外 CH1~CH6 状态，**十六进制两位**；`keys` 是按键位图，十进制 |
| `LD`,`RD` | `TELEMETRY_FIELD_DISTANCE` = `0x04` | 2 | 左右轮累计路程 mm（`%.1f`） |
| `LV`,`RV` | `TELEMETRY_FIELD_SPEED` = `0x08` | 2 | 左右轮实测速度 mm/s（`%.1f`） |
| `mode` | `TELEMETRY_FIELD_MODE` = `0x10` | 1 | 运动模式文本：`IDLE`/`LINE`/`STRAIGHT`/`TURN`/`BRAKE`/`SPEED`/`ERROR`；模式保留但动作已完成时输出 `DONE`（供上位机自动判定一次调参试验结束） |
| `k230` | `TELEMETRY_FIELD_K230` = `0x20` | 1 | 最近一次 K230 TARGET，**冒号分隔的复合值** `valid:offsetX:offsetY`（如 `1:-123:-45`）；无数据时为 `0:0:0` |
| `TL`,`TR` | `TELEMETRY_FIELD_TARGET` = `0x40` | 2 | MotionWheel 本拍左右目标轮速 mm/s（`%.1f`），空闲时为 0；与 `LV`/`RV` 画在同刻度即为"目标 vs 实测" |
| `PL`,`PR` | `TELEMETRY_FIELD_PWM` = `0x80` | 2 | 双轮最终输出 PWM（`%.0f`，±1000），可观察输出是否饱和 |
| `navT`,`navE` | `TELEMETRY_FIELD_NAV` = `0x100` | 2 | Nav 目标航向角与当前角误差（度，`%.2f`） |
| `lerr` | `TELEMETRY_FIELD_LINE` = `0x200` | 1 | 巡线离散权重误差 `-6~+6`（`%.1f`） |
| `LV` | `TELEMETRY_FIELD_SPEED_L` = `0x400` | 1 | **单侧**：只输出左轮实测速度 |
| `RV` | `TELEMETRY_FIELD_SPEED_R` = `0x800` | 1 | **单侧**：只输出右轮实测速度 |
| `TL` | `TELEMETRY_FIELD_TARGET_L` = `0x1000` | 1 | **单侧**：只输出左轮目标速度 |
| `TR` | `TELEMETRY_FIELD_TARGET_R` = `0x2000` | 1 | **单侧**：只输出右轮目标速度 |
| `PL` | `TELEMETRY_FIELD_PWM_L` = `0x4000` | 1 | **单侧**：只输出左轮 PWM |
| `PR` | `TELEMETRY_FIELD_PWM_R` = `0x8000` | 1 | **单侧**：只输出右轮 PWM |

注意 `gray` 是十六进制而其余数值列是十进制，`k230` 是一列而非两列——解析方按表头列名逐列取值即可，不要假设「一个掩码位对应一列」。

**单侧字段与成对字段的取舍。** `0x08`/`0x40`/`0x80` 是成对字段（LV+RV、TL+TR、PL+PR 各输出两列），`0x400` 之后的六个是单侧字段（各输出一列）。调单个轮子时用单侧：左轮 `M20496`（= `0x1000|0x400|0x4000|0x10`，即 TL+LV+PL+mode）行长约 40 字节，安全上限约 57 Hz；换成成对字段行长 75 字节，上限只有 30 Hz。**两者可以并存但不要同时置位同一数据**——那样同一列会输出两次，解析方会按表头当作两个不同列处理。

掩码 `TELEMETRY_FIELD_ALL = 0xFFFF`（十进制 65535）开启全部 16 个字段。例：`M7`（掩码 0x07 = yaw+sensor+distance）输出的表头是 `H,ms,yaw,gray,keys,LD,RD`，共 6 列；`M216`（0xD8 = speed+mode+target+pwm）是双轮轮速调参子集。

**不必手算掩码。** 网页的调参试验会按「任务→对象→阶段」自动选好最优掩码，并用 `Q` 命令向固件问出真实频率上限后自动设频；手动勾选字段时，上限也会在勾选瞬间实时显示。

当前舵机限位仅对应源码中 270° 舵机的电气量程。实车连杆若存在更小的机械行程，通电调试前必须先收紧 `SERVO_VERTICAL_*_ANGLE` 和 `SERVO_HORIZONTAL_*_ANGLE`，并同步修改本文的 Pin 口表、协议表和公共参数表。

### 3.4 `MotionStraight` 直线行驶控制库

`MotionStraight` 已完成本轮实车调试，当前由 MotionManager 统一初始化和按模式更新。Mission 状态回调通过 `MotionManager_StartForward/StartBackward()` 使用它，不再直接修改主循环。

控制结构分为三层：

1. 距离层读取左右编码器相对路程的平均值，优先在全程 `5/6` 处进入末段减速，并连续过渡到本次调用指定的终点速度。
2. 航向层锁定启动瞬间的 MPU6050 连续累计偏航角，使用 PD 生成左右轮差速 PWM 修正，不做 ±180° 归一化。
3. 公共 `MotionWheel` 分别对左右轮编码器实测速度执行 PI，叠加速度前馈、静摩擦补偿和航向修正，再统一限幅输出。

调用顺序：

```text
等待 KEY1
  -> 200 mm/s 巡线
  -> 灰度 bit0、bit1 同时检测到黑线
  -> 向前直行 150 mm，终点速度 0 mm/s
  -> 转到下一连续绝对角目标：启动航向 - 90°、-180°、-270°……
  -> 重新巡线并循环
```

- `MotionManager_StartForward/StartBackward()` 的 `distanceMM` 都填写正整数，方向由函数名决定。
- `speedMMps` 必须为正数；超过 `MOTION_STRAIGHT_MAX_SPEED_MMPS` 时自动限幅。
- `endSpeedMMps` 是非负速度大小且不能高于 `speedMMps`。设为 `0` 时平滑降速后释放电机；设为正数时到达目标后继续按该速度闭环前进，直到状态转换、`C0` 或 `MotionManager_Stop()` 停止当前运动。
- 常用流程使用 `MotionStraight_StartForward()` 或 `MotionStraight_StartBackward()`；距离参数填写正整数，巡航速度和终点速度直接填写 mm/s 浮点值，不使用枚举。
- `MotionStraight_Start()` 保留带符号距离的底层调用方式，终点速度方向自动跟随距离方向。
- 默认在全程 `5/6` 处开始减速；减速开始时根据当前速度、终点速度和剩余规划距离计算本次固定减速度，使目标速度在“目标距离减距离容差”处达到。
- 终点速度为零时，直线模块把双轮速度目标固定保持为 `0 mm/s` 达到 `MOTION_STRAIGHT_ZERO_SPEED_HOLD_SECONDS`，随后调用 `Motor_StopAll()` 释放电机；不再依据低速编码器速度确认停车。需要研究满力主动刹车时才使用 `MotionManager_StartBrake()`，不得在 Accomplish 文件中直接调用 `Motor_Brake()`。
- MPU6050 掉线、里程换算无效或更新周期非法时立即停止并进入错误状态。
- `MotionStraight` 直接使用 `Odometry` 已更新的左右路程和速度，不能再次读取 `Encoder_Get()`，否则会提前清空编码器增量。
- `MotionStraight` 运行期间不要调用 `Heading_SetYaw()` 重置角度，否则会改变本次直线行驶的航向基准。

实车调参顺序（下列参数均可通过 `K` 命令在线修改立即生效，见 3.3.1；推荐用网页「调参试验」跑 `W` 阶跃/`F` 定距看目标 vs 实测曲线，调好后再把数值写回头文件固化）：

1. 用网页 `W` 阶跃分别观察 `TL/LV/PL` 与 `TR/RV/PR`，先独立调整左轮 `lwff/lwsf`、右轮 `rwff/rwsf`，使中速稳态时 PI 修正较小且 PWM 不饱和；K4/K5 仅用于需要同时覆盖两轮的兼容场景。
2. 先令 `lwki=rwki=0`，分别增加 `lwkp/rwkp` 到两侧响应足够快且不持续振荡；随后先设置 `lwil/rwil`，再少量加入 `lwki/rwki` 消除各自稳态误差。
3. 低速测试航向 `kp`；若偏差被放大，将 `correctionSign` 从 `1` 改为 `-1` 或反向。随后少量增加 `kd` 抑制摆动。
4. 最后调整加速度、最大减速度、减速起点比例、每次任务的终点速度和距离允许误差。

OLED默认只显示上电运行时间；K230钢球物理位置和步进 PWM 绝对角度通过串口遥测或网页查看。巡线、按键、编码器和Gimbal状态继续通过串口遥测或网页查看。

| 蓝牙命令 | 作用 | 当前限制 |
|---|---|---|
| `C0` | 全局停车 | 始终有效，同时冻结 26H 计时并失能 Gimbal |
| `C3` | 直接启动要求 3 目标保持，不经过实体 `KEY2` 的两阶段扫描流程 | 视觉、底盘和摆球任务必须就绪/空闲 |
| `C4` | 停止要求 3 并让摆杆回中 | 仅停止摆球，不替代 `C0` 全局急停 |
| 其他非零 `C` 信号 | Mission 单次事件 | 当前任务未定义的编号不消费 |
| `L<number>` | 设置左轮开环 PWM | MotionManager 空闲时有效，范围 `-1000~1000` |
| `R<number>` | 设置右轮开环 PWM | MotionManager 空闲时有效，范围 `-1000~1000` |
| `U<number>` | 设置双轮相同开环 PWM | MotionManager 空闲时有效，范围 `-1000~1000` |
| `O<number>` | 设置纵向舵机角度 | PA27 输出 50 Hz PWM，范围 `0°~270°` |
| `D<number>` | 设置横向舵机角度 | PA26 输出 50 Hz PWM，范围 `0°~270°` |

## 4. 工程文件类型与职责

### 4.1 Application

| 文件 / 目录 | 类型 | 职责 |
|---|---|---|
| `Application/Core/App.c/.h` | 应用运行层 | 完整整车初始化、固定更新链和 Mission 上下文 |
| `Application/Core/TestApp.c/.h` | 可选测试运行层 | 跳过 OLED、MPU6050、六路红外和里程的快速测试入口；当前未使用 |
| `Application/Comms/BluetoothDebug.c/.h` | 应用通信层 | 解析 `C/L/R/U/O/D` 命令并提供任务事件 |
| `Application/Comms/K230Link.c/.h` | 应用通信层 | 通过逻辑 `Serial3`、物理 UART0 运行 K230 帧、CRC8、握手、拍照 ACK 和目标解析；每拍 RX 解析有固定预算 |
| `Application/Control/MotionManager.c/.h` | 统一运动调度 | 保证同一时刻只有直线、巡线、转向或刹车之一控制双轮 |
| `Application/Control/MotionStraight.c/.h` | 直线控制 | 定距速度规划、连续航向保持和可选终点速度 |
| `Application/Control/MotionLine.c/.h` | 巡线控制 | 六路红外 CH1~CH6 的离散权重差速和连续丢线确认 |
| `Application/Control/BallSensor.c/.h` | 钢球位置观测 | K230 千分比换算成毫米球位、按帧间隔估计滚动速度、视觉新鲜度 |
| `Application/Control/BallBalance.c/.h` | 摆杆滚球控制 | 目标位置 PID 闭环，输出摆杆倾角；当前不加前馈 |
| `Application/Control/BeamActuator.c/.h` | 摆杆倾角执行 | 倾角到步进角度的换算，独占传动比、零点、软限位和限斜率 |
| `Application/Control/MotionWheel.c/.h` | 公共轮速控制 | 双轮 PI、前馈、差速修正和 PWM 限幅 |
| `Application/Control/Nav.c/.h` | 转向控制 | 双轮反向旋转到连续绝对角或相对角 |
| `Application/Control/PID.c/.h` | 通用控制器 | 位置式 PID 计算、复位和调参 |
| `Application/Debug/DebugDisplay.c/.h` | 显示编排 | OLED运行秒数的单行局部刷新 |
| `Application/Gimbal/Gimbal.c/.h` | 云台应用层 | 管理 X/Y 地址、T 型多圈位置目标、反馈和到位状态 |
| `Application/Servo/Servo.c/.h` | 舵机控制 | TIMG7 双路 50 Hz PWM、角度限位与脉宽换算 |
| `Application/State/Heading.c/.h` | 航向状态 | MPU6050 零漂、连续偏航积分和尺度标定 |
| `Application/State/Odometry.c/.h` | 里程状态 | 编码器增量到双轮路程与速度的换算 |

### 4.2 Hardware 与 System

| 文件 / 目录 | 类型 | 职责 |
|---|---|---|
| `Hardware/Board/` | 板级驱动 | 按键、LED 和蜂鸣器 |
| `Hardware/Comms/Serial.c/.h` | UART 驱动 | `Serial1` 使用 UART2/CH0，`Serial3` 使用 UART0/CH2；`Serial2` 在当前配置中为无硬件停用桩 |
| `Hardware/Display/OLED.c/.h` | OLED 驱动 | I2C0 显存、文本、数字、图像和图形绘制 |
| `Hardware/Display/OLED_Data.c/.h` | 字模数据 | ASCII、中文字模和图像数据 |
| `Hardware/Motor/Motor.c/.h` | 有刷电机驱动 | TB6612 方向、PWM、释放和主动刹车 |
| `Hardware/Motor/PWM.c/.h` | PWM 驱动 | TIMG8 双通道占空比换算 |
| `Hardware/Motor/Encoder.c/.h` | 编码器驱动 | 在共享 GPIOA GROUP1 入口中解码左右轮与步进 AB |
| `Hardware/Motor/Stepper.c/.h` | 步进电机接口 | PB8/PB9/PB12驱动，PA13/PA29/PB13反馈，带绝对角基准、限位和梯形运动 |
| `Hardware/Motor/F32C.c/.h` | 无刷电机协议 | F32C 命令编码、校验和反馈解码 |
| `Hardware/Sensors/Graydetect.c/.h` | 六路红外驱动 | 软件 I2C 读取 `0x5C:0x05`，缓存 CH1~CH6 状态和区域误差；历史文件名/API 保持兼容 |
| `Hardware/Sensors/MPU6050.c/.h` | IMU 驱动 | 软件 I2C 初始化和原始数据读取 |
| `System/Delay.c/.h` | 系统基础 | 微秒、毫秒和秒级阻塞延时 |
| `System/Tick.c/.h` | 系统基础 | 100 Hz 累计节拍 |
| `System/Interrupt.c/.h` | 系统基础 | 全局中断统一开关 |

### 4.3 Mission 与 Accomplish

| 文件 / 目录 | 类型 | 职责 |
|---|---|---|
| `Application/Mission/Mission.c/.h` | 通用任务执行层 | 校验并执行静态状态图、回调和有序转换 |
| `Accomplish/25E.c/.h` | 题目状态图 | 25E 参数、状态、回调和转换表 |
| `Accomplish/26H.c/.h` | 当前题目控制器 | KEY1 启动单圈巡线、A 点终点软停、100 Hz 整数计时和组合急停冻结 |
| `Application/Control/TimedLineRun.c/.h` | KEY3/KEY4 定时巡线控制器 | 独立快照加速度、巡航速度与运行时长；30 秒后软停，不含终点逻辑 |
| `Application/Control/BallTargetCapture.c/.h` | KEY4 指定位置采样 | 按独立 K230 帧进行稳定性确认并输出平均目标位置，不重复实现摆球闭环 |
| `Application/Control/BallSequence.c/.h` | 要求 3 摆球任务 | KEY2 第二次按下启动扫描，C3直接启动目标位置保持，C4停止与视觉失效保护 |
| `Accomplish/25H.c/.h` | 保留题目状态图 | KEY1 启动的巡线、150 mm 直行和连续绝对左转循环 |
| `Accomplish/Brushless_Motor_Test.c/.h` | 可选测试状态图 | F32C 双轴多圈位置循环测试；当前未加载 |
| `状态机.md` | 使用说明 | 新建 Accomplish 状态图的编写流程 |

### 4.4 工程入口与配置

| 文件 | 类型 | 职责 |
|---|---|---|
| `main.c` | C 源文件 | 当前唯一运行入口；无按键时默认保持钢球在 O 点，KEY1启动26H总任务、KEY2两阶段启动摆球扫描、C3直接启动目标保持、KEY3启动定时巡线、KEY4三阶段启动任意位置巡线、KEY1+KEY2/C0急停 |
| `Application/Core/Main26H.c/.h` | C 源文件 / 头文件 | 旧完整26H包装入口，保留在工程中但当前不由 `main.c` 调用 |
| `main.syscfg` | TI SysConfig | 时钟、PinMux、GPIO、UART、I2C、PWM 和 SysTick 配置 |
| `.project`、`.cproject`、`.settings/` | CCS 元数据 | 工程、编译器、SDK 和 IDE 配置 |
| `targetConfigs/*.ccxml` | CCS 目标配置 | MSPM0G3507 调试连接 |

## 5. Application 公共接口与参数

### 5.1 Core、通信与显示

```c
void App_Init(void);                              /* 初始化完整整车，保持全局中断关闭。 */
uint8_t App_Update(App_UpdateContext_t *context); /* 有新 Tick 时更新整车并填写 Mission 上下文。 */

void TestApp_Init(void);                          /* 初始化可选快速测试通道。 */
uint8_t TestApp_Update(App_UpdateContext_t *context); /* 更新测试通道并填写同类型上下文。 */

- `To` 接口输入连续累计绝对角；例如当前为 370°，输入 90° 会按直接误差回到 90°，不会自动选择 ±180° 最短路径。
- `By` 接口输入相对转角；正负方向由 `NAV_ROTATION_COMMAND_SIGN` 与实车安装共同决定，可输入大于 360° 的多圈角度。
- 首次测试使用 60~80 mm/s。若启动后角度误差持续增大，只翻转 `NAV_ROTATION_COMMAND_SIGN`。
- Nav 到角后先把轮速斜坡降到零，再要求连续 `NAV_SETTLE_TICKS` 个周期处于允许误差内，避免单次采样抖动误判完成。

保留的 25H 在 KEY1 启动巡线时记录连续航向作为 0°基准。每次左侧双灰度触发后均先完成 150 mm 直行和固定时长的零速目标保持，再把下一绝对目标减去 90°并调用 `MotionManager_TurnTo()`；因此目标序列为 `startYaw-90°`、`startYaw-180°`、`startYaw-270°`……，不是在进入 TURN 时临时调用相对角接口。

## 4. 工程文件类型与职责

| 文件或目录 | 类型 | 职责 |
|---|---|---|
| `main.c` | C 源文件 | 当前唯一入口；无按键时默认保持钢球在 O 点，并直接分发 KEY1、KEY2、KEY3 与 C0/C3/C4 |
| `main.syscfg` | TI SysConfig | 时钟树、GPIO、UART、I2C、PWM、SysTick 和 PinMux 的唯一配置源 |
| `car_debug.html` | 单文件调试网页 | 浏览器端上位机：Web Serial 连接无线串口，发送第 3.3 节命令、解析遥测 CSV、实时画曲线并导出。无外部依赖，不参与固件编译 |
| `tests/*.mjs` | Node 测试脚本 | 无依赖，`node tests/<文件>` 直接跑。覆盖遥测解析、页面启动、固件-网页协议契约与教程源码一致性；清单见 4.2 节末 |
| `tutorial/sync-sources.mjs` | Node 工具脚本 | 按 `data-source`/`data-lines` 把教程内联源码重新对齐到当前源文件；源码位移时按内容自动重定位行号 |
| `.project`、`.cproject`、`.settings/` | CCS 工程元数据 | 工程名、TI Arm Clang 选项、SDK/SysConfig 依赖和 IDE 设置 |
| `targetConfigs/*.ccxml` | CCS 目标配置 | MSPM0G3507 调试连接配置 |
| `Application/Comms/` | 应用层 C 模块 | 蓝牙调试命令；K230 二进制帧、CRC8、握手和目标解析 |
| `Application/Core/` | 应用运行层 C 模块 | 固定硬件初始化、零漂、100 Hz 后台服务、按键和蓝牙事件采集 |
| `Application/Mission/` | 通用任务执行层 C 模块 | 校验并执行题目层提供的静态状态图、生命周期回调、有序条件转换和打断处理 |
| `Accomplish/` | 具体题目实现 C 模块 | 位于工程根目录；当前启用独立计时控制器 `26H.c/.h`，并保留 `25E.c/.h`、`25H.c/.h` 与刹车测试 `Test.c/.h` |
| `Application/Control/` | 运动控制层 C 模块 | MotionManager、通用 PID、公共双轮速度闭环、直线、巡线、目标角转向和短暂主动刹车 |
| `Application/Debug/` | 应用层 C 模块 | OLED 调试页面编排、CSV 遥测输出与运行时调参注册表 |
| `Application/Servo/` | 舵机硬件模块 | PA26/PA27 上的 TIMG7 双路 50 Hz PWM、角度限位和脉宽换算 |
| `Application/State/` | 状态层 C 模块 | Z 轴航向角解算、编码器里程与速度状态 |
| `Hardware/Board/` | 板级驱动 | 按键、LED、蜂鸣器 |
| `Hardware/Comms/` | 通信驱动 | `Serial1`/UART2 DAPLink 与 `Serial3`/UART0 K230 的中断接收、环形缓冲和 DMA 发送；`Serial2`/UART1 当前停用 |
| `Hardware/Display/` | 显示驱动与数据 | OLED I2C 驱动、帧缓冲、字模和图像数据 |
| `Hardware/Motor/` | 电机驱动 | TIMG8/TB6612 双直流电机、左右轮编码器，以及等待新引脚的步进接口 |
| `Hardware/Sensors/` | 传感器驱动 | PA25/PA14 软件 I2C 六路红外、PB2/PB3 软件 I2C MPU6050 |
| `System/` | 系统基础模块 | 阻塞延时、100 Hz SysTick 计数和全局硬件中断开关 |
| `Debug/`、`Release/` | 生成目录 | 目标文件、依赖文件、链接文件和固件输出；不手工修改 |
| `.gitignore` | Git 配置 | 排除构建产物 |
| `README.md` | 工程索引 | 时钟、Pin 口、文件职责、公共接口和公共参数 |
| `状态机.md` | Markdown 教程 | 说明如何为新题目创建 Accomplish 参数文件、状态图、主程序依赖和验证流程 |

### 4.1 源文件快速定位

| 源文件 / 头文件 | 文件职责 |
|---|---|
| `Application/Comms/BluetoothDebug.c/.h` | 解析 `C/L/R/U/O/D/G/M/V/F/B/T/A/Z/P/W/N/K/E/Y` 命令，保存单槽任务事件，限制自动运动期间的开环电机调试；`K` 命令带文本参数缓冲转交 Param 模块 |
| `Application/Core/App.c/.h` | 封装系统初始化和每拍固定更新，向当前控制器或保留的 Mission 提供 dt、按键边沿和蓝牙信号 |
| `Application/Mission/Mission.c/.h` | 定义状态图公共类型，校验题目状态图并执行每拍最多一次的状态转换 |
| `Accomplish/25E.c/.h` | 保存 25E 参数和状态图：每轮绝对目标在上一目标上增加 180°；当前未由 main 加载 |
| `Accomplish/26H.c/.h` | 完整 26H 模式加载；保存单圈巡线状态、终点判定、软停确认和 100 Hz 累计 Tick |
| `Application/Control/BallSequence.c/.h` | 完整 26H 模式加载；要求 3 的目标位置保持状态、手动停止和视觉失效保护，只驱动摆杆不碰底盘 |
| `Accomplish/25H.c/.h` | 保留 25H 参数和状态图：KEY1 启动巡线，左侧双黑线后直行 150 mm、固定时长零速保持，绝对左转目标每轮减少 90°并循环；当前未由 main 加载 |
| `Accomplish/Test.c/.h` | 独立刹车测试状态图：KEY2 启动定距直行、短暂刹车并返回等待；需要测试时才在 main.c 临时加载 |
| `Application/Comms/K230Link.c/.h` | 解析 `AA 55` 二进制帧和 CRC8，执行 READY/READY_ACK 双向握手，保存最新 TARGET、LANE 和 BALL_POSITION |
| `Application/Control/PID.c/.h` | 通用 PID 初始化、调参、复位和单步计算 |
| `Application/Control/MotionStraight.c/.h` | 头文件顶部保存直线参数；源文件实现距离规划、5/6 末段减速、可选终点速度、MPU6050 航向 PD 和软停车状态机 |
| `Application/Control/MotionWheel.c/.h` | 头文件顶部保存公共轮速参数；源文件实现 MotionStraight、MotionLine 与 Nav 共用的双轮速度 PI、前馈、差速修正合成和 PWM 限幅 |
| `Application/Control/MotionLine.c/.h` | 头文件顶部保存巡线参数；源文件实现六路红外离散权重差速、连续丢线确认、丢线正常完成和状态管理；巡线层不使用 PID |
| `Application/Control/MotionManager.c/.h` | 统一包装直线、巡线、转向和短暂刹车；自动停止旧模式并只更新当前模式 |
| `Application/Control/Nav.c/.h` | 头文件顶部保存转向参数；源文件实现连续航向目标、双轮等速反向转向和到角稳定判定 |
| `Application/Debug/DebugDisplay.c/.h` | 组织运行秒数的OLED单行局部刷新 |
| `Application/Debug/Telemetry.c/.h` | 表驱动组装二进制 SCHEMA/SAMPLE 帧，按频率和 32 位通道掩码经 `Serial1`/UART2（DMA）输出；`bpos/sang` 用于当前摆球网页观察 |
| `Application/Debug/TelemFrame.c/.h` | 二进制帧编码：CRC-8/ATM、帧构建、float32 小端打包；与 K230Link 同 CRC |
| `Application/Debug/Param.c/.h` | 运行时调参注册表：K 命令后端，33 个参数的读写、范围校验、左右轮独立参数与要求 3 摆球标定量 |
| `Application/Servo/Servo.c/.h` | 纵向/横向角度限位并换算为 500~2500 us 脉宽，写入 TIMG7 CCP1/CCP0 |
| `Application/State/Heading.c/.h` | MPU6050 Z 轴零漂标定、角速度积分和尺度标定 |
| `Application/State/Odometry.c/.h` | 读取编码器增量，累计左右路程并计算 mm/s 速度 |
| `Hardware/Board/Beep.c/.h` | 蜂鸣器与 LED2 的非阻塞提示状态机 |
| `Hardware/Board/Key.c/.h` | 四个低有效按键的非阻塞状态读取 |
| `Hardware/Board/LED.c/.h` | LED1、LED2 的开、关、翻转接口 |
| `Hardware/Comms/Serial.c/.h` | `Serial1` 通过 UART2/CH0 连接 DAPLink，`Serial3` 通过 UART0/CH2 连接 K230，TX 均为 DMA 非阻塞发送；`Serial2` 当前没有物理 UART 实例 |
| `Hardware/Display/OLED.c/.h` | OLED I2C 传输、128×64 帧缓冲、文本和图形绘制 |
| `Hardware/Display/OLED_Data.c/.h` | ASCII/中文字模和公共位图常量 |
| `Hardware/Motor/Encoder.c/.h` | 唯一 `GROUP1_IRQHandler` 内完成左右轮与步进AB四倍频正交解码 |
| `Hardware/Motor/EncoderStepper.h` | 步进共享编码器 ISR 桥接头文件 |
| `Hardware/Motor/Stepper.c/.h` | 公开单轴步进API；驱动、PWM绝对角和AB反馈均已启用 |
| `Hardware/Motor/Motor.c/.h` | 左右物理电机到 TB6612 A/B 通道的映射、方向和制动 |
| `Hardware/Motor/PWM.c/.h` | TIMG8 双通道占空比到比较值的换算 |
| `Hardware/Sensors/Graydetect.c/.h` | 六路红外 CH1~CH6 状态位图、I2C 读取和加权偏差；文件名保留兼容 |
| `Hardware/Sensors/MPU6050.c/.h` | PB2/PB3 软件 I2C、MPU6050 配置和原始数据读取 |
| `System/Delay.c/.h` | 基于 32 MHz CPUCLK 的 us/ms/s 阻塞延时 |
| `System/Tick.c/.h` | SysTick ISR 累计与主循环原子取出 100 Hz 节拍 |
| `System/Interrupt.c/.h` | 在 App 和 Mission 初始化完成后统一开启或关闭全局硬件中断 |

### 4.2 `car_debug.html` 调试网页

**打开方式。** 双击 `car_debug.html` 即可，`file://` 下 Web Serial 可用（该协议属于安全上下文），不需要架本地服务器。只支持 **Chrome / Edge** 等 Chromium 内核浏览器，Firefox 与 Safari 没有实现 Web Serial API。点「连接串口」后在弹出的设备列表里选无线 DAPLink 对应的串口，波特率固定 115200 8N1。

**与固件的约定。** 网页只依赖第 3.3 节那套 ASCII 命令和第 5.7 节的遥测 CSV，不假设任何固件内部实现。因此改动固件时只要保持这两个契约不变，网页无需同步修改；反过来，一旦新增命令或调整 CSV 字段，必须同步更新第 3.3 / 5.7 节，网页按表头列名解析，不硬编码列位置。

页面服务**两个目的**：遥控采集数据（供后续视觉/决策任务使用）和闭环调参。分为八块：连接与急停、运动控制（含 `W` 恒速与 `N` 巡线）、**遥控采集**、参数面板、标定向导、调参试验、遥测与曲线、原始终端。

**工作流一：调参试验。**

1. 用「任务→对象→阶段」导航选定要调什么（如 轮速 → 左轮 → P），点「开始试验」；
2. 连接和阶段切换时，网页提前准备本阶段的**最优掩码**（调左轮只传 TL/LV/PL）并按固件 `Q` 回报的真实频率上限设频；
3. 点「开始试验」后，网页先清空本次终端与曲线、记录参数快照，然后立即发激励命令，不再等待遥测准备链；
4. 指标卡明确标注数据来源：使用 `实时流 xx Hz`，低于 100 Hz 控制环时峰值可能漏采、仅供参考；
5. 每条记录可设为基准（虚线叠加对比）、导出 CSV，或一键复制 **AI 调参包**（含控制结构、参数、激励、列说明、自动指标和完整数据），直接粘给 AI 要下一组参数；
6. 在阶段参数区就地微调（`−`/`+` 按钮或直接输入，写入立即生效），再跑下一次对比。

**工作流二：遥控采集。** 启用键盘驾驶后用 <kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> 驾驶（W/S 走恒速命令，A/D 走相对转角），点「开始录制」整段记录遥测流；录制中按 <kbd>1</kbd>~<kbd>5</kbd> 打场景标记（直道/弯道/路口/障碍/异常），标记写入 CSV 的 `mark` 列。停止后可导出整段 CSV，或**按标记分段导出**——每个场景一个文件，便于后续分类使用。<kbd>Esc</kbd> 全局急停。

参数面板连接后自动 `K?` 拉取全表，悬浮显示范围与上电默认值，支持存到浏览器 localStorage、下次上电一键回写（固件参数掉电不保存）。标定向导包装陀螺尺度 `E1`/`Y`/`E0` 流程和里程 `cpm` 换算写入。

几个实现上的取舍：

- **频率上限由固件说了算，网页不各存一份阈值。** 改掩码后网页主动发 `Q` 查询，用固件回报的 `MAX` 自动设频；勾选字段的瞬间另有一份本地复算用于即时显示（公式与 `Telemetry.c` 相同），两者不一致就说明固件与网页失配。
- **指标必须标注来源。** 直启流程优先让车先动，因此默认用实时流指标；实时流受串口带宽限制低于 100 Hz 控制环，阶跃峰值会落在采样点之间，界面必须让用户看见这次结论是基于哪种数据得出的。
- **页面里的捕获解析分支已成死代码。** 固件的板载捕获模块已删除（见 3.3.0），`CH,`/`C,` 前缀与 `CAP_*` 帧类型不会再出现；`session.captureArmed` 恒为 `false`。清理页面时可一并删除，保留也不影响运行。
- **舵机滑块节流 100 ms。** 拖动会逐像素触发事件，不节流会瞬间发出几十条命令占满串口，把遥测流挤垮。
- **曲线按量纲分组共享刻度。** `LV/RV/TL/TR` 同为 mm/s、`PL/PR` 同为 PWM、`yaw/navT/navE` 同为度、`LD/RD` 同为 mm，各组内共用一套 min/max，目标与实测才能直接对比；不在分组表里的列仍各自独立归一化。
- **`ms` 和 `mode` 不进曲线选择器。** 前者是横轴，画出来是单调直线；后者是文本。两者仍会写进导出的 CSV。
- **重绘按帧合并。** 每收一行就重绘会卡死页面，所以只在有新数据时排一个渲染帧；暂停按钮只冻结画面，数据仍照常累积。
- **列数与表头不符的行直接丢弃。** 掉电重连和掩码切换瞬间会出现半截行，宁可丢也不猜。

**测试。** 全部测试位于 `tests/`，直接 `node tests/<文件>` 运行，无需依赖：

| 测试 | 覆盖什么 |
|---|---|
| `test_csv_parse.mjs` | 从 `car_debug.html` 抽取解析器与纯函数源码运行——测的是页面里的真代码而非副本，避免两边漂移。含 CSV 解析、阶跃指标、频率估算、标记分段 |
| `test_page_boot.mjs` | 用最小 DOM 桩**真正执行整段页面脚本**。`node --check` 只查语法，查不出 `let/const` 的初始化顺序错误（函数声明提升而变量不提升），那类错误只有打开页面才暴露；本测试把「打开就白屏」挡在提交前，并交叉校验 HTML 的 `id` 与脚本引用是否对得上 |
| `test_wheel_tuning_contract.mjs` | 左右轮独立调参的参数表兼容性与网页联动 |
| `test_heading_resample_contract.mjs` | `Z1`/`Z2` 的语义区分与错误分支 |
| `test_tutorial.mjs` | 教程内联源码块与真实源文件逐字一致（防止改了源码而教程还在讲旧代码） |

**教程源码同步。** 教程用 `data-source` + `data-lines` 把源码逐字内联，源文件一改行号就偏。改完固件后跑 `node tutorial/sync-sources.mjs` 自动按内容重新定位并覆盖（`--dry` 只看不改），再跑 `node tutorial/build.mjs` 重建页面。内容大改到无法自动定位的块会明确列出，需人工确认并补讲解。

## 5. 公共函数接口

以下只列出头文件公开声明。`.c` 文件内的 `static` 函数和变量属于文件内部实现，不作为跨模块接口使用。

### 5.1 `Application/Comms/BluetoothDebug.h`

```c
void BluetoothDebug_Init(void);
void BluetoothDebug_Update(uint8_t elapsedTicks,
                           uint8_t manualMotorEnabled); /* 解析命令；参数决定是否允许 L/R/U。 */
uint8_t BluetoothDebug_PopSignal(uint8_t *signal); /* 取出一个 C0~C255 单次事件。 */
int16_t BluetoothDebug_GetLeftCommand(void);      /* 返回最近一次左轮开环 PWM。 */
int16_t BluetoothDebug_GetRightCommand(void);     /* 返回最近一次右轮开环 PWM。 */

void K230Link_Init(void);                         /* 初始化 K230 帧解析与握手状态。 */
void K230Link_Update(uint8_t elapsedTicks);        /* 按节拍推进握手并解析接收帧。 */
uint8_t K230Link_IsReady(void);                    /* 返回双方握手是否完成。 */
uint8_t K230Link_GetTarget(K230Link_Target_t *target); /* 读取最新目标数据。 */

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
uint8_t K230Link_RequestCapture(uint8_t count);
uint8_t K230Link_IsCapturePending(void);
uint8_t K230Link_PopCaptureAck(uint8_t *ok, uint16_t *index);
```

| 头文件 | 公共参数 | 当前值 | 作用 |
|---|---|---:|---|
| `BluetoothDebug.h` | `BLUETOOTH_COMMAND_IDLE_TICKS` | `3U` | 无结束符命令空闲 30 ms 后执行 |
| `BluetoothDebug.h` | `BLUETOOTH_TASK_SIGNAL_MAX` | `255U` | Mission 信号上限 |
| `K230Link.h` | `K230_LINK_FRAME_MAGIC_0`、`K230_LINK_FRAME_MAGIC_1` | `0xAA/0x55` | 帧头 |
| `K230Link.h` | `K230_LINK_FRAME_VERSION` | `0x01` | 协议版本 |
| `K230Link.h` | `K230_LINK_MAX_PAYLOAD_LENGTH` | `32U` | 最大载荷长度 |
| `K230Link.h` | `K230_LINK_READY_RETRY_TICKS` | `10U` | READY 重发周期，100 ms |
| `K230Link.h` | `K230_LINK_RX_BUDGET_BYTES` | `128U` | 每个 10 ms 控制拍最多解析的 K230 RX 字节数 |
| `K230Link.h` | `K230_LINK_MESSAGE_READY`、`K230_LINK_MESSAGE_READY_ACK`、`K230_LINK_MESSAGE_TARGET` | `0x01/0x02/0x10` | 消息类型 |
| `DebugDisplay.h` | `DEBUG_DISPLAY_REFRESH_TICKS` | `100U` | OLED 刷新周期，1 s |

### 5.2 MotionManager

```c
MotionManager_Result_t MotionManager_Init(void); /* 初始化全部运动模块。 */
MotionManager_Result_t MotionManager_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps); /* 定距前进。 */
MotionManager_Result_t MotionManager_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps); /* 定距后退。 */
MotionManager_Result_t MotionManager_StartLine(float speedMMps); /* 持续巡线。 */
MotionManager_Result_t MotionManager_TurnTo(
    float targetYawDeg, float speedMMps);          /* 转到连续绝对角。 */
MotionManager_Result_t MotionManager_TurnBy(
    float deltaYawDeg, float speedMMps);           /* 按相对角转动。 */
MotionManager_Result_t MotionManager_StartBrake(void); /* 启动短时主动刹车模式。 */
void MotionManager_Update(float dt);               /* 只更新当前运动模式。 */
void MotionManager_Stop(void);                     /* 停止并释放当前模式。 */
uint8_t MotionManager_IsConfigured(void);          /* 返回初始化状态。 */
uint8_t MotionManager_IsBusy(void);                /* 返回当前动作是否运行。 */
uint8_t MotionManager_IsFinished(void);            /* 返回当前动作是否完成。 */
MotionManager_Mode_t MotionManager_GetMode(void);  /* 返回当前运动模式。 */
MotionManager_Error_t MotionManager_GetError(void);/* 返回统一错误码。 */
```

| 参数 | 当前值 | 作用 |
|---|---:|---|
| `MOTION_MANAGER_BRAKE_RELEASE_SECONDS` | `0.01f` | 主动刹车前的 PWM 释放时间 |
| `MOTION_MANAGER_BRAKE_HOLD_SECONDS` | `0.05f` | `Motor_Brake()` 保持时间；不会由终点速度 0 自动调用 |

### 5.3 直线、巡线、转向与轮速

```c
extern float MotionStraight_TuneHeadingKp;
extern float MotionStraight_TuneHeadingKd;
extern float MotionStraight_TuneAccelerationMMps2;

MotionStraight_Result_t MotionStraight_Init(void);
MotionStraight_Result_t MotionStraight_Start(
    float distanceMM, float speedMMps, float endSpeedMMps); /* 带符号距离的底层入口。 */
MotionStraight_Result_t MotionStraight_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps); /* 前进包装。 */
MotionStraight_Result_t MotionStraight_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps);
void MotionStraight_Update(float dt);
void MotionStraight_Stop(void);
void MotionStraight_ApplyHeadingTunings(void);
uint8_t MotionStraight_IsConfigured(void);
uint8_t MotionStraight_IsBusy(void);
uint8_t MotionStraight_IsFinished(void);
MotionStraight_State_t MotionStraight_GetState(void);
MotionStraight_Error_t MotionStraight_GetError(void);
float MotionStraight_GetRemainingDistanceMM(void);
```

`MotionStraight_Tune*` 是运行时可调参数（上电取 `#define` 默认值，经 K 命令读写）；Kp/Kd 写入后需调用 `MotionStraight_ApplyHeadingTunings()` 进入航向 PID，加速度直接生效。

`MotionStraight_State_t` 包含空闲、运行、终点低速持续、完成和错误状态。终点速度为正数时进入 `MOTION_STRAIGHT_STATE_CONTINUING` 并继续占用电机，此时 `MotionStraight_IsFinished()` 和 `MotionStraight_IsBusy()` 都返回 1，调用 `MotionStraight_Stop()` 后才释放；终点速度为零时进入完成状态。`MotionStraight_Error_t` 区分 MPU 掉线、里程换算无效、更新周期非法和公共轮速层错误；`MotionStraight_Result_t` 返回启动、忙、参数、配置和传感器检查结果。

### 5.4 `Application/Control/MotionWheel.h`

```c
extern float MotionWheel_TuneKp;
extern float MotionWheel_TuneKi;
extern float MotionWheel_TuneIntegralLimit;
extern float MotionWheel_TuneFeedforwardPWMPerMMps;
extern float MotionWheel_TuneStaticFrictionPWM;

MotionWheel_Result_t MotionWheel_Init(void);
MotionWheel_Result_t MotionWheel_Update(
    const MotionWheel_Command_t *command, float dt);
void MotionWheel_Reset(void);
void MotionWheel_Stop(void);
void MotionWheel_ApplyPidTunings(void);
uint8_t MotionWheel_IsConfigured(void);
float MotionWheel_GetMaximumCommandPWM(void);
float MotionWheel_GetLeftCommandPWM(void);
float MotionWheel_GetRightCommandPWM(void);
float MotionWheel_GetTargetSpeedL(void);
float MotionWheel_GetTargetSpeedR(void);
```

`MotionWheel_Command_t` 包含左右轮目标速度 `targetSpeedLMMps/targetSpeedRMMps` 和上层控制器提供的附加修正 `trimLPWM/trimRPWM`。MotionWheel 是电机闭环的唯一公共写入层，上层模式不得并行调用。`MotionWheel_Tune*` 是运行时可调参数；Kp/Ki/积分限幅写入后需调用 `MotionWheel_ApplyPidTunings()` 进入左右轮 PID，前馈与静摩擦每拍直接读取。`GetTargetSpeedL/R` 返回最近一拍上层提交的目标轮速（空闲时为 0），供遥测 `TL`/`TR` 列输出。

### 5.5 `Application/Control/MotionLine.h`

```c
extern float MotionLine_TuneKpMMpsPerWeight;
extern float MotionLine_TuneKiMMpsPerWeight;
extern float MotionLine_TuneKdMMpsPerWeight;
extern float MotionLine_TuneAccelerationMMps2;
extern float MotionLine_TuneDecelerationMMps2;

MotionLine_Result_t MotionLine_Init(void);
MotionLine_Result_t MotionLine_Start(float speedMMps);
MotionLine_Result_t MotionLine_SetSpeed(float speedMMps);
MotionLine_Result_t MotionLine_RequestStop(void);
void MotionLine_Update(float dt);
void MotionLine_Stop(void);
uint8_t MotionLine_IsConfigured(void);
uint8_t MotionLine_IsBusy(void);
uint8_t MotionLine_IsFinished(void);
MotionLine_State_t MotionLine_GetState(void);
MotionLine_Error_t MotionLine_GetError(void);
float MotionLine_GetLineError(void);
float MotionLine_GetProfileSpeedMMps(void);
float MotionLine_GetProfileAccelerationMMps2(void);
```

`MotionLine_GetLineError()` 返回六路红外加权平均并低通后的 PID 输入误差，范围约为 `-6~+6`。`MotionLine_Start()` 一次性快照 `lra/lki/lkd/lacc/ldec`：运行中继续写 K 只影响下一次启动。巡线层不再区分直道和弯道，不再按压线程度降低基准速度，也不再使用上层前馈或差速变化率限幅；左右轮目标速度为 `profileSpeed ± PID(lerr)`，最终 PWM 只由公共轮速层限制。

### 5.6 `Application/Control/Nav.h`

```c
extern float Nav_TuneMaxTurnSpeedMMps;
extern float Nav_TuneMinTurnSpeedMMps;
extern float Nav_TuneSlowdownAngleDeg;
extern float Nav_TuneAngleToleranceDeg;

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

`Nav_StartTo()` 接受连续绝对航向角，`Nav_StartBy()` 接受相对转角，速度参数单位为 mm/s；两者都固定使用双轮等速反向转向。四个 `Tune` 变量运行时可调、每拍直接读取、写入即生效，对应原 `NAV_MAX_TURN_SPEED_MMPS`/`NAV_MIN_TURN_SPEED_MMPS`/`NAV_SLOWDOWN_ANGLE_DEG`/`NAV_ANGLE_TOLERANCE_DEG`。

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
MotionManager_Result_t MotionManager_StartBrake(void);
MotionManager_Result_t MotionManager_StartSpeed(float speedMMps);
void MotionManager_Update(float dt);
void MotionManager_Stop(void);
uint8_t MotionManager_IsConfigured(void);
uint8_t MotionManager_IsBusy(void);
uint8_t MotionManager_IsFinished(void);
MotionManager_Mode_t MotionManager_GetMode(void);
MotionManager_Error_t MotionManager_GetError(void);
```

## 7. Mission 与 Accomplish 公共接口和参数

`MotionManager_StartSpeed()` 是 `W` 命令的后端（`MOTION_MANAGER_MODE_SPEED`）：双轮同目标速度、无规划斜坡、无航向修正，范围 ±`MOTION_MANAGER_SPEED_MAX_MMPS`（1000 mm/s）。已处于 SPEED 模式时再次调用只更新目标、不复位 PID（链式阶跃）；该模式没有完成条件，`IsBusy()` 恒为 1，只能被 `MotionManager_Stop()` 或 `C0` 停止。仅用于调参激励，Mission 状态图不应使用。

### 5.6.2 `Application/Mission/Mission.h`

```c
typedef struct
{
    Mission_EnterCallback_t onEnter;          /* 进入状态时调用一次。 */
    Mission_UpdateCallback_t onUpdate;        /* 每个 Mission 节拍调用。 */
    Mission_ExitCallback_t onExit;            /* 完成、打断、停止或错误时调用一次。 */
    const Mission_Transition_t *transitions;  /* 按声明顺序检查的转换表。 */
    uint8_t transitionCount;                  /* 转换数量。 */
    uint8_t interruptible;                    /* 是否允许普通打断转换。 */
} Mission_StateDefinition_t;

typedef struct
{
    const Mission_StateDefinition_t *states;  /* 静态状态数组。 */
    uint16_t stateCount;                      /* 状态数量。 */
    uint16_t startState;                      /* 起始状态编号。 */
    uint16_t errorState;                      /* 错误状态编号。 */
} Mission_GraphDefinition_t;

void Mission_Init(const Mission_GraphDefinition_t *graph); /* 校验并进入起始状态。 */
void Mission_Update(const App_UpdateContext_t *updateContext); /* 更新当前状态并执行最多一次转换。 */
void Mission_Stop(void);                            /* 停止动作并回到起始状态。 */
Mission_Status_t Mission_GetStatus(void);           /* 返回任务执行状态。 */
const Mission_Runtime_t *Mission_GetRuntime(void);  /* 返回只读运行上下文。 */
Mission_ActionStatus_t Mission_GetMotionActionStatus(void); /* 把 MotionManager 状态映射为动作状态。 */
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

该函数返回保留的 25H 静态只读状态图。当前 `main.c` 不调用该接口；题目参数全部位于 `Accomplish/25H.h` 开头。

### 5.6.5 `Accomplish/26H.h`

```c
void Accomplish26H_Init(void);
void Accomplish26H_Update(const App_UpdateContext_t *context);
uint8_t Accomplish26H_IsTiming(void);
uint32_t Accomplish26H_GetElapsedTicks(void);
Accomplish26H_State_t Accomplish26H_GetState(void);
Accomplish26H_Error_t Accomplish26H_GetError(void);
```

这些接口实现当前独立单圈控制器，不经过 Mission；`main.c`调用初始化和更新接口，累计Tick通过遥测读取。正常状态依次为READY、离开起点、巡线、终点主动刹车和低速确认；I2C离线、丢线/控制错误、超出最大圈长、急停或静止超时会进入ERROR。

### 5.6.6 `Accomplish/Test.h`

```c
const Mission_GraphDefinition_t *AccomplishTest_GetMissionGraph(void);
```

该函数返回独立的刹车测试状态图。测试时才在 `main.c` 临时加载；KEY2 会执行“定距直行 -> 短暂刹车 -> 等待”，不影响当前 26H 单圈流程。

### 5.7 `Application/Debug/DebugDisplay.h`

```c
void DebugDisplay_Init(void);
void DebugDisplay_Update(uint8_t elapsedTicks);
```

### 5.7.1 `Application/Debug/Telemetry.h`

```c
#define TELEMETRY_DEFAULT_RATE_HZ    50U
#define TELEMETRY_RATE_HARD_LIMIT_HZ 100U

#define TELEMETRY_CH_TL     0x0001U
#define TELEMETRY_CH_LV     0x0002U
#define TELEMETRY_CH_PL     0x0004U
#define TELEMETRY_CH_TR     0x0008U
#define TELEMETRY_CH_RV     0x0010U
#define TELEMETRY_CH_PR     0x0020U
#define TELEMETRY_CH_YAW    0x0040U
#define TELEMETRY_CH_NAVE   0x0080U
#define TELEMETRY_CH_LERR   0x0100U
#define TELEMETRY_CH_GRAY   0x0200U
#define TELEMETRY_CH_LD     0x0400U
#define TELEMETRY_CH_RD     0x0800U
#define TELEMETRY_CH_VX     0x1000U
#define TELEMETRY_CH_VAD    0x2000U
#define TELEMETRY_CH_BPOS   0x4000U
#define TELEMETRY_CH_BREF   0x8000U
#define TELEMETRY_CH_SANG   0x10000UL
#define TELEMETRY_CH_ALL    0x1FFFFUL

void Telemetry_Init(void);
void Telemetry_Update(uint8_t elapsedTicks, uint8_t pressedKeys);
uint8_t Telemetry_SetRateHz(uint8_t rateHz);
uint8_t Telemetry_SetFieldMask(uint32_t mask);
uint8_t Telemetry_GetRateHz(void);
uint16_t Telemetry_GetFieldMask(void);
uint8_t Telemetry_GetMaxRateHz(void);
```

**为什么仍保留动态频率上限：** DMA 发送不会再阻塞主循环，但物理串口仍只有 115200 8N1。固件按当前二进制 SAMPLE 帧长度计算带宽，并在 100 Hz 控制频率处硬限幅；全 17 通道为 79 字节/帧，100 Hz 约 7.9 KB/s。要求 3 默认只开 `bpos/sang` 时帧更短，网页会直接请求 100 Hz。

**带宽常量跟随 SysConfig，不写死。** 计算上限用的两个常量定义在 `Telemetry.c` 内部而非头文件里，其中每秒字节数直接由波特率推导：

```c
/* 8N1 每字节含起始位和停止位共 10 个位时。 */
#define TELEMETRY_UART_BYTES_PER_SECOND  ((uint32_t)BLUETOOTH_UART_BAUD_RATE / 10U)
#define TELEMETRY_BANDWIDTH_PERCENT      70U
```

这样修改 UART 波特率时限流会自动跟随；若实际带宽降低，`Q` 回报的最大频率和 `G` 的范围校验也会同步收紧。

`Telemetry_Init()` 会用 `Telemetry_GetMaxRateHz()` 对默认频率再夹一次；默认频率、掩码或波特率变化后仍不会超发。

### 5.7.2 `Application/Debug/Param.h`

```c
void Param_HandleCommand(const char *args);
```

K 命令后端：`args` 为去掉 `K` 前缀的文本参数（`?` 列表 / `<id>?` 读 / `<id>=<float>` 写），直接经 UART1 回应。参数注册表见 3.3.1；表序即协议 id，一经发布不得重排，只能尾部追加。

### 5.7.3 `Application/Debug/Capture.h`（已删除）

板载高速捕获模块连同 `X` 命令、`CAP_META`/`CAP_SAMPLE`/`CAP_END` 帧类型和 24 KB RAM 缓冲已整体移除，原因见 3.3.0：二进制 + DMA 之后实时流本身即 100 Hz 无损，而那 24 KB 把栈可用余量压到 1587 字节，导致栈越界改写 `.data` 中的整车参数。

需要它的历史实现时，在 `fix/ram-stack-overflow` 分支删除该模块的那个提交之前取。

**这里有一条对以后仍然适用的教训：** 任何新增的大型静态缓冲，都要同时核对 `Debug/ArcLineTest.map` 里 `.data` 末尾到 `.stack` 之间还剩多少空隙——那才是栈真正能用的空间。`--stack_size` 由 SysConfig 按器件硬编码生成到 `device_linker.cmd`（本器件为 512），既不在 `main.syscfg` 里、改了也会被下次构建覆盖，指望它报错是靠不住的。

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
uint8_t Heading_IsScaleCalibActive(void);
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

### 5.11 `Hardware/Motor/Stepper.h`

```c
void Stepper_Init(void);
void Stepper_Update(uint8_t elapsedTicks);
Stepper_Result_t Stepper_Enable(bool enable);
Stepper_Result_t Stepper_MoveBySteps(
    int32_t steps, const Stepper_Profile_t *profile);
Stepper_Result_t Stepper_MoveToSteps(
    int32_t target, const Stepper_Profile_t *profile);
Stepper_Result_t Stepper_MoveByAngle(
    float degrees, const Stepper_Profile_t *profile);
Stepper_Result_t Stepper_MoveToAngle(
    float degrees, const Stepper_Profile_t *profile);
void Stepper_Stop(void);
void Stepper_EmergencyStop(void);
Stepper_Result_t Stepper_SetCurrentPosition(float degrees);
bool Stepper_IsBusy(void);
void Stepper_GetStatus(Stepper_Status_t *status);
```

`Stepper_Init()`在当前`App_Init()`中调用。资源固定为 ST=PB8、DIR=PB9、EN=PB12、AB=PA13/PA29、绝对 PWM=PB13；PWM 连续3帧有效后建立绝对角基准。当前 `STEPPER_AUTO_START_ENABLED=0`，上电不自动运动，实测角由 `BeamActuator` 捕获为水平零偏。

转换数组从前到后就是优先级。动作运行时只检查打断转换，动作完成后只检查正常转换；每个系统节拍最多转换一次。`C0` 不受 `interruptible` 限制，始终停止并复位任务。

### 7.2 Accomplish 入口

```c
const Mission_GraphDefinition_t *Accomplish25E_GetMissionGraph(void); /* 返回 25E 静态状态图。 */
const Mission_GraphDefinition_t *Accomplish25H_GetMissionGraph(void); /* 返回保留的 25H 静态状态图。 */
void Accomplish26H_Init(void); /* 初始化当前 26H 单圈控制器。 */
void Accomplish26H_Update(const App_UpdateContext_t *context); /* 更新巡线、终点软停与计时。 */
const Mission_GraphDefinition_t *BrushlessMotorTest_GetMissionGraph(void); /* 返回 F32C 测试状态图。 */
```

| 头文件 | 公共参数 | 当前值 | 作用 |
|---|---|---:|---|
| `BluetoothDebug.h` | `BLUETOOTH_COMMAND_IDLE_TICKS` | `3U` | 无结束符命令的 30 ms 空闲判定 |
| `BluetoothDebug.h` | `BLUETOOTH_TASK_SIGNAL_MAX` | `255U` | C 任务信号最大编号；普通信号不排队 |
| `K230Link.h` | `K230_LINK_FRAME_MAGIC_0/1` | `0xAAU` / `0x55U` | K230 帧头 |
| `K230Link.h` | `K230_LINK_FRAME_VERSION` | `0x01U` | 当前通信协议版本 |
| `K230Link.h` | `K230_LINK_MAX_PAYLOAD_LENGTH` | `32U` | 允许接收的最大 PAYLOAD 长度 |
| `K230Link.h` | `K230_LINK_READY_RETRY_TICKS` | `10U` | 100 Hz 下每 100 ms 重发 READY |
| `K230Link.h` | `K230_LINK_RX_BUDGET_BYTES` | `128U` | 每拍 K230 接收解析预算，防止异常输入独占主循环 |
| `K230Link.h` | `K230_LINK_MESSAGE_READY/READY_ACK/TARGET/LANE/BALL_POSITION` | `0x01U` / `0x02U` / `0x10U` / `0x13U` / `0x14U` | 消息类型编号 |
| `K230Link.h` | `K230_LINK_BALL_POSITION_MIN/MAX/INVALID` | `-5000` / `+5000` / `-32768` | 水管全长 250 mm 映射到 `-50.00~+50.00` 后的 100 倍定点范围和丢球哨兵 |
| `DebugDisplay.h` | `DEBUG_DISPLAY_REFRESH_TICKS` | `100U` | OLED 1 Hz 刷新间隔 |
| `MotionStraight.h` | `MOTION_STRAIGHT_*` | 见 6.2 | 航向 PD、直线速度规划、减速起点比例和距离允许误差 |
| `MotionManager.h` | `MOTION_MANAGER_BRAKE_*` / `MOTION_MANAGER_SPEED_MAX_MMPS` | 见 6.2.1 | 定距软停后的 PWM 释放与短暂主动刹车时间；W 恒速调试模式速度上限 |
| `MotionWheel.h` | `MOTION_WHEEL_*` | 见 6.1 | MotionStraight、MotionLine 与 Nav 共用的速度 PI、前馈和 PWM 限幅 |
| `MotionLine.h` | `MOTION_LINE_*` | 见 6.3 | 六路权重滤波、统一 PID、加减速度和丢线确认节拍 |
| `Accomplish/25E.h` | `ACCOMPLISH_25E_*` | 见 6.5 | 25E 启动按键、直线距离与速度、入线确认、巡线速度和转向参数 |
| `Accomplish/25H.h` | `ACCOMPLISH_25H_*` | 见 6.6 | 25H 启动按键、左侧标志掩码、巡线、150 mm 直行和绝对左转参数 |
| `Accomplish/26H.h` | `ACCOMPLISH_26H_*` | 见头文件 | 单圈速度、A 点识别、软停偏移、静止确认与 KEY1+KEY2 急停参数 |
| `Accomplish/Test.h` | `ACCOMPLISH_TEST_*` | 见 6.7 | KEY2 启动的定距软停与短刹测试参数 |
| `Nav.h` | `NAV_*` | 见 6.4 | 双轮转向的加减速、低速区、到角误差和稳定判定 |
| `Servo.h` | `SERVO_PHYSICAL_RANGE_DEG` | `270U` | 脉宽换算对应的舵机物理量程 |
| `Servo.h` | `SERVO_MIN_PULSE_US` / `SERVO_MAX_PULSE_US` | `500U` / `2500U` | 舵机最小/最大高电平脉宽 |
| `Servo.h` | `SERVO_FRAME_US` | `20000U` | 50 Hz 舵机帧周期 |
| `Servo.h` | `SERVO_VERTICAL_MIN_ANGLE` / `MAX` / `DEFAULT` | `0U` / `270U` / `135U` | 纵向轴限位与上电角度 |
| `Servo.h` | `SERVO_HORIZONTAL_MIN_ANGLE` / `MAX` / `DEFAULT` | `0U` / `270U` / `135U` | 横向轴限位与上电角度 |
| `Heading.h` | `HEADING_CALIBRATION_SAMPLES` | `400U` | 开机零漂采样数 |
| `Heading.h` | `HEADING_CALIBRATION_INTERVAL_MS` | `2U` | 零漂采样间隔 |
| `Odometry.h` | `Odometry_CountsPerMM` | `float`，初值 `6.44086f` | 1:28 减速比、65 mm 轮胎的每毫米编码器计数初值，必须按实车标定 |
| `Stepper.h` | `STEPPER_ENABLED` / `STEPPER_FEEDBACK_ENABLED` | `1U` / `1U` | 启用步进驱动、PWM绝对角和AB反馈 |
| `Stepper.h` | `STEPPER_INITIAL/MIN/MAX_ANGLE_DEG` | `238.0°` / `106.0°` / `309.0°` | 水平绝对角和机构软件运动边界 |
| `Stepper.h` | `STEPPER_STEPS_PER_REVOLUTION` 等参数 | 3200 ST/rev、4096 AB count/rev | ST=PB8、DIR=PB9、EN=PB12、AB=PA13/PA29、绝对PWM=PB13 |
| `Serial.h` | `SERIAL1_RX_BUFFER_SIZE` | `1024U` | `Serial1`/UART2 DAPLink 环形接收缓冲区容量 |
| `Serial.h` | `SERIAL2_RX_BUFFER_SIZE` | `256U` | `Serial2` 软件接口保留容量；当前无 UART1 硬件实例 |
| `Serial.h` | `SERIAL3_RX_BUFFER_SIZE` / `SERIAL3_TX_BUFFER_SIZE` | `256U` / `256U` | `Serial3`/UART0 K230 接收与 DMA 发送缓冲区容量 |
| `Serial.h` | `Serial1_RxFlag` | `volatile uint8_t` | PC/网页链路存在未读数据标志 |
| `PWM.h` | `PWM_MAX_DUTY` | `1000U` | 电机 PWM 指令绝对值上限 |
| `Graydetect.h` | `GRAYDETECT_ENABLED` | `1U` | 完整26H入口启用PA25/PA14六路红外软件I2C |
| `Graydetect.h` | `GRAY_CHANNEL_COUNT` / `GRAYDETECT_I2C_ADDRESS` / `GRAYDETECT_STATE_REGISTER` | `6U` / `0x5C` / `0x05` | 六路红外通道数、I2C 地址和状态寄存器 |
| `OLED.h` | `OLED_8X16` / `OLED_6X8` | `8U` / `6U` | 字体尺寸选择 |
| `OLED.h` | `OLED_UNFILLED` / `OLED_FILLED` | `0U` / `1U` | 图形空心/实心选择 |
| `OLED_Data.h` | `OLED_F8x16`、`OLED_F6x8`、`OLED_CF16x16`、`Diode` | `const` 字模/位图数组 | OLED 公共显示数据 |
| `OLED_Data.h` | `ChineseCell_t` / `OLED_CHARSET_UTF8` | 字模结构 / 字符集宏 | 中文字模索引与 16×16 数据格式 |
| `Tick.h` | `TICK_HZ` / `TICK_DT` | `100U` / `0.01f` | 系统节拍频率与秒单位周期 |

### 6.1 `MotionWheel.h` 参数

以下公共宏位于 `Application/Control/MotionWheel.h` 开头，由直线、巡线和 Nav 共用；左右轮默认宏分别以 `MOTION_WHEEL_LEFT_*` / `MOTION_WHEEL_RIGHT_*` 继承这些初始值，实车标定后可分别固化：

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
| `MOTION_WHEEL_KP` / `MOTION_WHEEL_KI` / `MOTION_WHEEL_INTEGRAL_LIMIT` | `0.6f` / `0.0f` / `0.0f` |
| `MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS` / `MOTION_WHEEL_STATIC_FRICTION_PWM` | `0.45181f` / `21.0445f` |
| `MOTION_WHEEL_LEFT_FEEDFORWARD_PWM_PER_MMPS` / `MOTION_WHEEL_LEFT_STATIC_FRICTION_PWM` | `0.44573f` / `19.884f` |
| `MOTION_WHEEL_RIGHT_FEEDFORWARD_PWM_PER_MMPS` / `MOTION_WHEEL_RIGHT_STATIC_FRICTION_PWM` | `0.45788f` / `22.205f` |
| `MOTION_WHEEL_MAX_COMMAND_PWM` | `1000.0f` |

`Odometry_CountsPerMM` 和速度前馈按旧 1:20、48 mm 轮胎标定值乘以 `(28/20) × (48/65) = 1.033846` 得到。该换算假设电机轴编码器分辨率不变；更换电机后应先用 `K16`/网页里程标定复核 `cpm`，再分别重测 `lwff/lwsf` 与 `rwff/rwsf`。Kp、Ki、静摩擦和任务里的 mm/s 目标不能只靠机械比例可靠推导，因此暂不改动。

前 5 个公共宏是左右轮独立运行时变量的兼容参考值。新调参使用 `K17~K21`（lwkp/lwki/lwil/lwff/lwsf）和 `K22~K26`（rwkp/rwki/rwil/rwff/rwsf）；`K1~K5` 保留为同时写两轮的兼容入口。`MAX_COMMAND_PWM` 保持编译期固定。

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
| `MOTION_STRAIGHT_DECELERATION_START_RATIO` | 0~1 比例 | 首选减速起点占全程的比例；当前 `5/6` 表示最后 `1/6` 为减速段 |
| `MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM` | mm | 允许的终点距离误差；速度曲线在“目标距离减该值”处到达终点速度 |
| `MOTION_STRAIGHT_ZERO_SPEED_HOLD_SECONDS` | s | 终点速度为零后，双轮速度 PI 持续跟踪 `0 mm/s` 的固定时长 |

当前实车测试值：

| 宏 | 当前值 |
|---|---:|
| `MOTION_STRAIGHT_HEADING_KP` / `MOTION_STRAIGHT_HEADING_KD` | `6.0f` / `0.4f` |
| `MOTION_STRAIGHT_HEADING_LIMIT_PWM` / `MOTION_STRAIGHT_CORRECTION_SIGN` | `700.0f` / `-1` |
| `MOTION_STRAIGHT_MAX_SPEED_MMPS` | `1000.0f` |
| `MOTION_STRAIGHT_ACCELERATION_MMPS2` | `300.0f` |
| `MOTION_STRAIGHT_DECELERATION_START_RATIO` / `MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM` | `5.0f / 6.0f` / `5.0f` |
| `MOTION_STRAIGHT_ZERO_SPEED_HOLD_SECONDS` | `0.05f` |

`HEADING_KP`/`HEADING_KD`/`ACCELERATION` 是运行时变量 `MotionStraight_Tune*` 的上电默认值，可经 `K6~K8`（skp/skd/sac）在线修改；其余保持编译期固定。

### 6.2.1 `MotionManager.h` 刹车参数

以下宏位于 `Application/Control/MotionManager.h` 开头，只在 Mission 的独立 BRAKE 状态中生效：

| 宏 | 单位 | 当前值 | 作用 |
|---|---:|---:|---|
| `MOTION_MANAGER_BRAKE_RELEASE_SECONDS` | s | `0.01f` | 直线已软停后继续释放 PWM 的最短时间，避免直接从驱动切入制动 |
| `MOTION_MANAGER_BRAKE_HOLD_SECONDS` | s | `0.05f` | 调用 `Motor_Brake()` 的保持时间；增大可减少滑行，但过大可能顿挫、发热 |
| `MOTION_MANAGER_SPEED_MAX_MMPS` | mm/s | `1000.0f` | `W` 恒速调试模式允许的目标轮速上限，与直线请求速度上限一致 |

### 6.3 `MotionLine.h` 参数

以下宏位于 `Application/Control/MotionLine.h` 开头。26H 与保留的 25H 都通过 MotionManager 启动巡线；基准速度只由请求速度和加减速斜坡决定，红外误差统一进入 PID 生成左右轮目标差速，不应循环调用 `MotionManager_StartLine()`：

| 宏 | 单位 | 当前值 | 作用 |
|---|---:|---:|---|
| `MOTION_LINE_OUTER_WEIGHT` | 无 | `6` | 六路最外侧红外权重的绝对值，对应最大修正力度 |
| `MOTION_LINE_INNER_WEIGHT` | 无 | `2.5f` | 六路内侧红外权重的绝对值 |
| `MOTION_LINE_KP/KI/KD_MMPS_PER_WEIGHT` | mm/s | `26 / 0 / 1` | 统一巡线 PID 初值；输出直接作为左右轮目标差速 |
| `MOTION_LINE_ACCELERATION_MMPS2` / `DECELERATION` | mm/s² | `300` / `360` | 起步、终点降速和软停的基准速度斜坡 |
| `MOTION_LINE_WEIGHT_FILTER_ALPHA` | — | `0.25` | 六路加权误差低通系数；循迹不使用 IMU |
| `MOTION_LINE_LOST_CONFIRM_TICKS` | 100 Hz 节拍 | `8U` | 连续所有有效红外通道全白达到 8 次后确认丢线，当前约为 80 ms |

`lra/lki/lkd` 分别写入 `MotionLine_TuneKp/Ki/KdMMpsPerWeight`，由 `MotionLine_Start()` 快照。弯道和直线不再分区；CH2/CH5 只作为普通红外通道参与误差计算，不触发任何低速上限或保持距离。

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

`MAX/MIN_TURN_SPEED`、`SLOWDOWN_ANGLE`、`ANGLE_TOLERANCE` 是运行时变量 `Nav_Tune*` 的上电默认值，可经 `K11~K14`（nvx/nvn/nsa/ntl）在线修改；加减速斜率与符号保持编译期固定。

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
| `ACCOMPLISH_25H_FORWARD_END_SPEED_MMPS` | mm/s | `0.0f` | 定距完成后的终点速度；当前减速至零并固定保持后再转向 |
| `ACCOMPLISH_25H_TURN_STEP_DEG` | ° | `-90.0f` | 每轮绝对目标减少的角度；目标依次为启动航向减 90°、180°、270°…… |
| `ACCOMPLISH_25H_TURN_SPEED_MMPS` | mm/s | `80.0f` | Nav 原地转向的每侧轮速度请求 |

### 6.7 `Accomplish/Test.h` 参数

`Test.c/.h` 只用于上机观察短刹时的滑行量。它已包含在 CCS 工程中；当前 `main.c` 直接运行26H巡线与手动摆球入口，不会自动加载该独立测试图。需要测试短刹时应显式接入 `AccomplishTest_GetMissionGraph()`，完成后恢复对应入口。

| 宏 | 单位 | 当前值 | 作用 |
|---|---:|---:|---|
| `ACCOMPLISH_TEST_START_KEY_MASK` | 按键位图 | `0x02U` | KEY2 的 bit1 掩码；按下沿启动一次测试 |
| `ACCOMPLISH_TEST_BRAKE_DISTANCE_MM` | mm | `300U` | 测试用定距直行距离 |
| `ACCOMPLISH_TEST_BRAKE_SPEED_MMPS` | mm/s | `200.0f` | 测试用巡航速度 |
| `ACCOMPLISH_TEST_BRAKE_END_SPEED_MMPS` | mm/s | `0.0f` | 必须为零，直线完成后才会进入 BRAKE 状态 |

`PID_t` 的 `Kp/Ki/Kd`、`integral`、`prevError`、`outMax` 和 `integralMax` 为 PID 实例的公共状态与参数。除上述公开声明外，其余 `static` 数据和源文件内宏均为模块内部实现。
