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
| SysTick | 32 MHz / 320000 = 100 Hz | `System/Tick`、`main.c`、`MotionLine`、`MotionWheel`；`K230Link`、`MotionStraight`、`Nav` 预留 | 10 ms 系统节拍；当前直接驱动按键检测和灰度巡线更新，不等待 K230 |
| TIMG8 | 32 MHz，周期 1600 = 20 kHz | `Hardware/Motor/PWM`、`MotionWheel`、`MotionLine` | MotionLine 按灰度权重给出左右轮目标速度，再由公共轮速层输出 PWM；CCP0/PB15 右轮，CCP1/PB16 左轮 |
| TIMA0 | BUSCLK / 32 = 1 MHz，周期 20000 = 50 Hz | `Application/Servo` | 双路舵机 PWM；1 个计数等于 1 us |
| I2C0 | BUSCLK 32 MHz，SCL 400 kHz | `Hardware/Display/OLED` | OLED 控制器通信 |
| UART1 | BUSCLK 32 MHz，9600 baud，8N1，RX 中断 | `Hardware/Comms/Serial`、`Application/Comms/BluetoothDebug` | 蓝牙调试命令和应答 |
| UART2 | BUSCLK 32 MHz，115200 baud，8N1，当前不启用 RX NVIC | `main.syscfg` 预留 | K230 硬件配置保留；当前巡线主程序不调用 `K230Link_Init/Update()`，不发送握手帧 |
| GPIO 软件 I2C | CPU 延时产生时序 | `Hardware/Sensors/MPU6050`、`Application/State/Heading` | 当前用于 OLED 连续多圈航向显示；MotionStraight 和 Nav 预留使用该角度闭环；不占用 I2C 外设实例 |
| GPIOA GROUP1 IRQ | A/B 相双边沿 | `Hardware/Motor/Encoder`、`MotionWheel` | 左右编码器软件正交解码，为公共双轮速度 PI 提供速度反馈 |

## 2. Pin 口占用

| Pin | 方向/复用 | 占用对象 | 程序映射与说明 |
|---|---|---|---|
| PA10 | 开漏式 GPIO | MPU6050 SCL | 软件 I2C 时钟；当前用于连续航向显示，MotionStraight/Nav 预留使用 |
| PA11 | 开漏式 GPIO | MPU6050 SDA | 软件 I2C 数据；当前用于连续航向显示，MotionStraight/Nav 预留使用 |
| PA12 | GPIO 输出 | 右电机 AIN2 | TB6612 A 通道方向；MotionLine 右轮方向输出 |
| PA13 | GPIO 输出 | 右电机 AIN1 | TB6612 A 通道方向；MotionLine 右轮方向输出 |
| PA14 | GPIO 输入、上拉 | 灰度 CH3 | `Graydetect` 位图 bit3，右内侧，检测黑线为 1，巡线权重 `+1` |
| PA15 | GPIO 输入、上拉、双边沿中断 | 右编码器 A | GPIOA GROUP1 IRQ；`MotionWheel` 右轮反馈 |
| PA16 | GPIO 输入、上拉、双边沿中断 | 右编码器 B | GPIOA GROUP1 IRQ；`MotionWheel` 右轮反馈 |
| PA17 | GPIO 输入、上拉、双边沿中断 | 左编码器 A | GPIOA GROUP1 IRQ；`MotionWheel` 左轮反馈 |
| PA19 | SWDIO | 下载调试 | 不作为普通 GPIO 使用 |
| PA20 | SWCLK | 下载调试 | 不作为普通 GPIO 使用 |
| PA21 | UART2 TX | K230 RX 预留 | 当前巡线主程序不发送数据；恢复 K230Link 时连接 K230 GPIO4（RX） |
| PA22 | UART2 RX | K230 TX 预留 | 当前巡线主程序不启用接收中断；恢复 K230Link 时连接 K230 GPIO3（TX） |
| PA24 | GPIO 输入、上拉、双边沿中断 | 左编码器 B | GPIOA GROUP1 IRQ；`MotionWheel` 左轮反馈 |
| PA28 | I2C0 SDA | OLED | 400 kHz |
| PA30 | GPIO 输入、上拉 | KEY1 | 低电平按下，按键位图 bit0；调用 `MotionLine_Start()` 开始巡线 |
| PA31 | I2C0 SCL | OLED | 400 kHz |
| PB0 | GPIO 输出 | 左电机 BIN1 | TB6612 B 通道方向；MotionLine 左轮方向输出 |
| PB1 | GPIO 输出 | 左电机 BIN2 | TB6612 B 通道方向；MotionLine 左轮方向输出 |
| PB6 | UART1 TX | 蓝牙 | MCU 发送到蓝牙 RX |
| PB7 | UART1 RX、上拉 | 蓝牙 | 蓝牙 TX 发送到 MCU，RX 中断接收 |
| PB8 | TIMA0 CCP0 | 横向舵机 | `D` 命令，`Servo_SetHorizontalAngle()` |
| PB9 | TIMA0 CCP1 | 纵向舵机 | `O` 命令，`Servo_SetVerticalAngle()` |
| PB10 | GPIO 输入、上拉 | KEY4 | 低电平按下，按键位图 bit3；当前巡线测试不分配运动命令，仅在 OLED 显示 |
| PB11 | GPIO 输入、上拉 | KEY2 | 低电平按下，按键位图 bit1；立即调用 `MotionLine_Stop()` |
| PB14 | GPIO 输入、上拉 | KEY3 | 低电平按下，按键位图 bit2；当前巡线测试不分配运动命令，仅在 OLED 显示 |
| PB15 | TIMG8 CCP0 | 右电机 PWM | TB6612 A 通道，20 kHz；MotionLine 右轮速度闭环和差速修正输出 |
| PB16 | TIMG8 CCP1 | 左电机 PWM | TB6612 B 通道，20 kHz；MotionLine 左轮速度闭环和差速修正输出 |
| PB17 | GPIO 输出 | 蜂鸣器 | 低电平有效 |
| PB18 | GPIO 输入、上拉 | 灰度 CH4 | `Graydetect` 位图 bit4，右最外侧，检测黑线为 1，巡线权重 `+3` |
| PB20 | GPIO 输入、上拉 | 灰度 CH2 | `Graydetect` 位图 bit2，中间，检测黑线为 1，巡线权重 `0` |
| PB23 | GPIO 输出 | LED1 | 高电平点亮 |
| PB24 | GPIO 输入、上拉 | 灰度 CH1 | `Graydetect` 位图 bit1，左内侧，检测黑线为 1，巡线权重 `-1` |
| PB25 | GPIO 输入、上拉 | 灰度 CH0 | `Graydetect` 位图 bit0，左最外侧，检测黑线为 1，巡线权重 `-3` |
| PB27 | GPIO 输出 | LED2 | 高电平点亮；蜂鸣提示同步使用 |

## 3. 当前程序调用关系

### 3.1 启动阶段

`main.c::App_Init()` 按以下顺序运行：

1. `SYSCFG_DL_init()` 应用 `main.syscfg` 生成的时钟、PinMux 和外设配置。
2. 初始化 Tick、LED、蜂鸣器、按键、灰度、电机、舵机、蓝牙 UART1 和编码器里程计；当前不初始化 K230 UART2 帧链路。
3. 开启全局中断并初始化 OLED。
4. 初始化 MPU6050；OLED 显示 `ZERO CALIBRATING...`，要求小车保持静止。
5. `Heading_Calibrate()` 采集 400 次 Z 轴陀螺仪零偏，采样间隔 2 ms；若 MPU6050 不在线，OLED 显示 `OFFLINE`。
6. 清除零漂阶段累计的 Tick 和编码器计数，初始化蓝牙命令解析器。
7. 调用 `MotionLine_Init()`；它使用 `MotionWheel.h` 和 `MotionLine.h` 顶部参数初始化公共双轮速度层与灰度离散权重巡线；参数非法时长鸣一次。

### 3.2 100 Hz 主循环

`Tick_PollCount()` 每次取出累计节拍，随后依次调用：

```text
Heading_Update -> Odometry_Update
               -> 按键边沿检测与 MotionLine_Update
               -> BluetoothDebug_Update -> 状态蜂鸣提示
               -> Beep_Tick -> DebugDisplay_Update
```

当前巡线测试速度集中在 `main.c` 顶部，`MOTION_LINE_TEST_SPEED_MMPS` 当前值为 `100.0 mm/s`。KEY1 调用 `MotionLine_Start()` 开始持续巡线，KEY2 调用 `MotionLine_Stop()` 立即停止；KEY3、KEY4 当前不执行运动命令。

当前 `main.c` 只初始化和更新 `MotionLine`，不初始化、不更新 `MotionStraight` 或 `Nav`，因此三种上层模式不会争用电机。五路连续全白达到 `MOTION_LINE_LOST_CONFIRM_TICKS`、更新周期非法或公共轮速层报错时，MotionLine 停止并进入错误状态，主程序长鸣一次。

当前巡线测试关闭 K230 握手：`main.c` 不调用 `K230Link_Init()`、`K230Link_Update()` 或 `K230Link_IsReady()`，KEY1 不再受 K230 状态限制。K230Link 源码和 UART2 SysConfig 配置保留，后续正式联调时可以重新接入。

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
| 7 | 当前巡线状态：`LINE:IDLE`、`LINE:RUN` 或 `LINE:ERROR` |

K230Link 库保留的统一帧格式如下，当前巡线主程序不调用：

```text
AA 55 | VER | TYPE | SEQ | LEN | PAYLOAD | CRC8
```

- `VER=0x01`；`TYPE` 为 `READY=0x01`、`READY_ACK=0x02` 或 `TARGET=0x10`。
- `SEQ` 为 8 位帧序号，`LEN` 最大为 32。
- CRC 使用 CRC-8/ATM，多项式 `0x07`、初始值 0，校验范围为 `VER` 到 `PAYLOAD`。
- TARGET 的 PAYLOAD 固定为 `valid:u8 + offsetX:int16_LE + offsetY:int16_LE`，共 5 字节。
- 恢复 K230 联调后，K230 `uart_io.py` 测试入口可在握手后持续发送 `valid=1、offsetX=123、offsetY=-45`。

### 3.3 蓝牙调试协议

命令不区分大小写。推荐以 `\r` 或 `\n` 结束；也支持空格、逗号、分号作为分隔符。没有结束符时，接收空闲 3 个系统节拍（30 ms）后执行。每条命令都会返回 `OK ...` 或 `ERR ...`。

MotionLine 上机测试期间关闭蓝牙模块电源，避免 `L/R/U` 与 MotionLine 同时改写电机 PWM。当前 Pin 口表中没有蓝牙电源控制引脚，因此固件不能控制模块断电，需要使用外部电源开关或断开模块供电。需要蓝牙手动调试时再给模块上电，并且不要同时按 KEY1 启动巡线。

| 命令 | 作用 | 输入范围与限位 | 示例 |
|---|---|---|---|
| `L<number>` | 只更新左轮 PWM，右轮保持上次指令 | `-1000~1000`，超限自动夹紧 | `L10` |
| `R<number>` | 只更新右轮 PWM，左轮保持上次指令 | `-1000~1000`，超限自动夹紧 | `R10` |
| `U<number>` | 左右轮使用相同 PWM | `-1000~1000`；正数前进，负数后退 | `U100` |
| `O<number>` | 纵向舵机移动到指定角度 | 当前限位 `0°~270°` | `O10` |
| `D<number>` | 横向舵机移动到指定角度 | 当前限位 `0°~270°` | `D10` |

`L/R/U` 的数字是开环 PWM 指令，不是 mm/s 物理速度。OLED 上的 `LV/RV` 才是由编码器和 `Odometry_CountsPerMM` 换算的实测速度。

当前舵机限位仅对应源码中 270° 舵机的电气量程。实车连杆若存在更小的机械行程，通电调试前必须先收紧 `SERVO_VERTICAL_*_ANGLE` 和 `SERVO_HORIZONTAL_*_ANGLE`，并同步修改本文的 Pin 口表、协议表和公共参数表。

### 3.4 `MotionStraight` 直线行驶控制库

`Application/Control/MotionStraight` 已完成本轮实车调试，但当前 MotionLine 测试版 `main.c` 不初始化、也不调用它。以后切回直线任务时仍按 100 Hz 非阻塞调用。

控制结构分为三层：

1. 距离层读取左右编码器相对路程的平均值，优先在全程 `5/6` 处进入末段减速，并连续过渡到本次调用指定的终点速度。
2. 航向层锁定启动瞬间的 MPU6050 连续累计偏航角，使用 PD 生成左右轮差速 PWM 修正，不做 ±180° 归一化。
3. 公共 `MotionWheel` 分别对左右轮编码器实测速度执行 PI，叠加速度前馈、静摩擦补偿和航向修正，再统一限幅输出。

调用顺序：

```text
上电且 MPU 零漂完成
    -> MotionStraight_Init()
    -> 按 KEY1
    -> MotionStraight_StartForward(距离, 巡航速度, 终点速度)

每个 100 Hz 节拍：
Heading_Update(dt) -> Odometry_Update(ticks) -> MotionStraight_Update(dt)
```

直线运动规划和航向参数位于 `MotionStraight.h` 顶部；双轮速度 PI、前馈和最终 PWM 限幅位于 `MotionWheel.h` 顶部。切回直线主流程时使用以下调用关系：

```c
#include "Application/Control/MotionStraight.h"

/* 初始化阶段：必须在电机、里程计和 MPU6050 零漂标定完成后调用。 */
if (MotionStraight_Init() != MOTION_STRAIGHT_RESULT_OK)
{
    /* 默认参数范围非法。 */
}

/* main.c 顶部由用户填写。 */
#define MOTION_STRAIGHT_TEST_DISTANCE_MM   1200U
#define MOTION_STRAIGHT_TEST_SPEED_MMPS    300.0f
#define MOTION_STRAIGHT_TEST_END_SPEED_MMPS 0.0f

/* KEY1 按下沿：使用顶部参数启动前进。 */
(void)MotionStraight_StartForward(MOTION_STRAIGHT_TEST_DISTANCE_MM,
                                  MOTION_STRAIGHT_TEST_SPEED_MMPS,
                                  MOTION_STRAIGHT_TEST_END_SPEED_MMPS);

/* 100 Hz 主循环：先更新传感器状态，再更新 MotionStraight。 */
Heading_Update(elapsedSeconds);
Odometry_Update(elapsedTicks);
MotionStraight_Update(elapsedSeconds);
```

- `distanceMM > 0` 表示前进，`distanceMM < 0` 表示后退。
- `speedMMps` 必须为正数；超过 `MOTION_STRAIGHT_MAX_SPEED_MMPS` 时自动限幅。
- `endSpeedMMps` 是非负速度大小且不能高于 `speedMMps`。设为 `0` 时平滑降速后释放电机；设为正数时到达目标后继续按该速度闭环前进，直到调用 `MotionStraight_Stop()`。
- 常用流程使用 `MotionStraight_StartForward()` 或 `MotionStraight_StartBackward()`；距离参数填写正整数，巡航速度和终点速度直接填写 mm/s 浮点值，不使用枚举。
- `MotionStraight_Start()` 保留带符号距离的底层调用方式，终点速度方向自动跟随距离方向。
- 默认优先在全程 `5/6` 处开始减速；若 `MOTION_STRAIGHT_DECELERATION_MMPS2` 不足以在最后 `1/6` 内达到终点速度，程序会按运动学制动距离自动提前减速，避免终点速度突变。
- 进入 `distanceToleranceMM` 后只继续完成速度斜坡；终点速度为零时调用 `Motor_StopAll()` 清零 PWM 和方向脚，不再调用 `Motor_Brake()`。
- MPU6050 掉线、里程换算无效或更新周期非法时立即停止并进入错误状态。
- `MotionStraight` 直接使用 `Odometry` 已更新的左右路程和速度，不能再次读取 `Encoder_Get()`，否则会提前清空编码器增量。
- `MotionStraight` 运行期间不要调用 `Heading_SetYaw()` 重置角度，否则会改变本次直线行驶的航向基准。

实车调参顺序：

1. 单独进行蓝牙开环测试时，使用 `U100/U200/...` 记录 OLED 稳态速度，在 `MotionWheel.h` 顶部调整速度前馈、静摩擦 PWM 与最大可靠输出；完成后关闭蓝牙模块再测 `MotionStraight`。
2. 先令 `ki=0`，逐步增加速度 `kp` 到响应足够快且不持续振荡，再少量加入 `ki` 消除稳态误差。
3. 低速测试航向 `kp`；若偏差被放大，将 `correctionSign` 从 `1` 改为 `-1` 或反向。随后少量增加 `kd` 抑制摆动。
4. 最后调整加速度、最大减速度、减速起点比例、每次任务的终点速度和距离允许误差。

### 3.5 `MotionWheel` 与 `MotionLine` 当前巡线流程

统一命名层级如下：

| 层级 | 模块前缀 | 职责 |
|---|---|---|
| 上层运动模式 | `MotionStraight_*` | 编码器距离规划和 MPU6050 航向保持 |
| 上层运动模式 | `MotionLine_*` | 五路灰度巡线和丢线处理 |
| 上层运动模式 | `Nav_*` | MPU6050 目标角闭环与双轮反向转向 |
| 下层公共执行 | `MotionWheel_*` | 双轮速度 PI、前馈、差速合成和电机 PWM 输出 |

`MotionWheel` 是 `MotionStraight`、`MotionLine` 和 `Nav` 共用的唯一双轮速度闭环与电机输出层。三个上层控制器不能同时更新，否则会争用电机。当前主程序只选择 `MotionLine`；以后必须由任务状态机先停止当前模式，再启动另一个模式。

当前 `main.c` 使用以下巡线调用流程：

```c
#include "Application/Control/MotionLine.h"

/* 用户巡线速度放在 main.c 顶部，单位 mm/s。 */
#define MOTION_LINE_TEST_SPEED_MMPS  100.0f

/* 初始化阶段：Graydetect、Motor 和 Odometry 已初始化后调用。 */
if (MotionLine_Init() != MOTION_LINE_RESULT_OK)
{
    /* 默认参数非法。 */
}

/* KEY1 按下沿：开始持续巡线。 */
(void)MotionLine_Start(MOTION_LINE_TEST_SPEED_MMPS);

/* 每个 100 Hz 周期先更新里程，再更新巡线。 */
Odometry_Update(elapsedTicks);
MotionLine_Update(elapsedSeconds);

/* KEY2 按下沿：停止并释放电机输出。 */
MotionLine_Stop();
```

模式切换顺序：

```text
MotionStraight_Stop()
    -> MotionLine_Init()
    -> MotionLine_Start(MOTION_LINE_TEST_SPEED_MMPS)
    -> 每周期 Odometry_Update() -> MotionLine_Update()
    -> MotionLine_Stop()
    -> 需要直线时重新 MotionStraight_Init() -> MotionStraight_StartForward(...)
```

- MotionLine 不使用 PID。五路从左到右的权重为 `-3、-1、0、+1、+3`，检测到黑线时把对应权重相加，并把最终结果限制在 `-3~+3`。
- 权重为 `-3` 时，左轮目标速度为巡线速度的 `0.5` 倍，右轮为 `1.5` 倍；权重为 `+3` 时左右相反。权重为正负 `1` 时，每侧只增减巡线速度的 `1/6`。
- 灰度位为 `1` 表示检测到黑线，五路全白为 `0x00`。连续全白达到 `MOTION_LINE_LOST_CONFIRM_TICKS` 才进入 `MOTION_LINE_ERROR_LINE_LOST`；确认前保持上一拍的左右轮目标速度，任一路恢复为 1 时立即清零丢线计数。
- 五路全黑 `0x1F` 当前按误差 0 继续直行；十字、停止线和任务标志必须在后续任务状态机中根据连续采样单独判断。
- `MotionLine.h` 顶部参数是当前首轮低速测试值，仍需根据实车循迹效果标定。

### 3.6 `Nav` 目标角转向库

`Nav` 当前未接入 `main.c`。以后切回目标角测试时，必须先完成 MPU6050 开机零漂，再以 100 Hz 调用。角度直接使用 `Heading_GetYaw()` 的连续累计值，不做 ±180° 归一化。

Nav 只有一种车轮动作：左右轮等速反向，车体围绕两轮中点附近转动。

```c
#include "Application/Control/Nav.h"

/* 初始化：必须在 Heading 零漂和 MotionWheel 所需硬件完成后调用。 */
if (Nav_Init() != NAV_RESULT_OK)
{
    /* 默认参数非法或公共轮速层初始化失败。 */
}

/* 绝对角：指向连续航向角 90°。 */
(void)Nav_StartTo(90.0f, 80.0f);

/* 相对角：从当前方向再转 +90°。 */
(void)Nav_StartBy(90.0f, 80.0f);

/* 每个 100 Hz 周期必须先更新航向，再更新 Nav。 */
Heading_Update(elapsedSeconds);
Odometry_Update(elapsedTicks);
Nav_Update(elapsedSeconds);

/* 中途取消或离开转向任务。 */
Nav_Stop();
```

- `To` 接口输入连续累计绝对角；例如当前为 370°，输入 90° 会按直接误差回到 90°，不会自动选择 ±180° 最短路径。
- `By` 接口输入相对转角；正负方向由 `NAV_ROTATION_COMMAND_SIGN` 与实车安装共同决定，可输入大于 360° 的多圈角度。
- 首次测试使用 60~80 mm/s。若启动后角度误差持续增大，只翻转 `NAV_ROTATION_COMMAND_SIGN`。
- Nav 到角后先把轮速斜坡降到零，再要求连续 `NAV_SETTLE_TICKS` 个周期处于允许误差内，避免单次采样抖动误判完成。

当前 MotionLine 测试版主程序没有 Nav 按键映射。切回 Nav 测试时可使用以下映射：

| 按键 | 调用 |
|---|---|
| KEY1 | `Nav_StartTo(NAV_TEST_ABSOLUTE_YAW_DEG, NAV_TEST_BASE_SPEED_MMPS)` |
| KEY2 | `Nav_StartBy(NAV_TEST_RELATIVE_YAW_DEG, NAV_TEST_BASE_SPEED_MMPS)` |
| KEY3 | `Nav_StartBy(-NAV_TEST_RELATIVE_YAW_DEG, NAV_TEST_BASE_SPEED_MMPS)` |
| KEY4 | `Nav_Stop()` |

## 4. 工程文件类型与职责

| 文件或目录 | 类型 | 职责 |
|---|---|---|
| `main.c` | C 源文件 | 系统初始化、MPU6050 零漂、关闭 K230 握手的 KEY1/KEY2 MotionLine 测试入口和 100 Hz 主循环调度 |
| `main.syscfg` | TI SysConfig | 时钟树、GPIO、UART、I2C、PWM、SysTick 和 PinMux 的唯一配置源 |
| `.project`、`.cproject`、`.settings/` | CCS 工程元数据 | 工程名、TI Arm Clang 选项、SDK/SysConfig 依赖和 IDE 设置 |
| `targetConfigs/*.ccxml` | CCS 目标配置 | MSPM0G3507 调试连接配置 |
| `Application/Comms/` | 应用层 C 模块 | 蓝牙调试命令；K230 二进制帧、CRC8、握手和目标解析 |
| `Application/Control/` | 应用层 C 模块 | 通用 PID、公共双轮速度闭环、直线行驶、灰度巡线和目标角转向控制 |
| `Application/Debug/` | 应用层 C 模块 | OLED 调试页面编排与 10 Hz 刷新 |
| `Application/Servo/` | 舵机硬件模块 | TIMA0 双通道 PWM、角度限位和脉宽换算 |
| `Application/State/` | 状态层 C 模块 | Z 轴航向角解算、编码器里程与速度状态 |
| `Hardware/Board/` | 板级驱动 | 按键、LED、蜂鸣器 |
| `Hardware/Comms/` | 通信驱动 | UART1 蓝牙和 UART2 K230 的中断接收环形缓冲区与发送接口 |
| `Hardware/Display/` | 显示驱动与数据 | OLED I2C 驱动、帧缓冲、字模和图像数据 |
| `Hardware/Motor/` | 电机驱动 | TIMG8 PWM、TB6612 方向控制、编码器正交解码 |
| `Hardware/Sensors/` | 传感器驱动 | 五路灰度 GPIO、MPU6050 软件 I2C |
| `System/` | 系统基础模块 | 阻塞延时和 100 Hz SysTick 计数 |
| `Debug/`、`Release/` | 生成目录 | 目标文件、依赖文件、链接文件和固件输出；不手工修改 |
| `.gitignore` | Git 配置 | 排除构建产物 |
| `README.md` | 工程索引 | 时钟、Pin 口、文件职责、公共接口和公共参数 |

### 4.1 源文件快速定位

| 源文件 / 头文件 | 文件职责 |
|---|---|
| `Application/Comms/BluetoothDebug.c/.h` | 解析 `L/R/U/O/D` 命令，完成空闲帧判定、参数限幅、执行和串口应答 |
| `Application/Comms/K230Link.c/.h` | 解析 `AA 55` 二进制帧和 CRC8，执行 READY/READY_ACK 双向握手，保存最新 TARGET |
| `Application/Control/PID.c/.h` | 通用 PID 初始化、调参、复位和单步计算 |
| `Application/Control/MotionStraight.c/.h` | 头文件顶部保存直线参数；源文件实现距离规划、5/6 末段减速、可选终点速度、MPU6050 航向 PD 和软停车状态机 |
| `Application/Control/MotionWheel.c/.h` | 头文件顶部保存公共轮速参数；源文件实现 MotionStraight、MotionLine 与 Nav 共用的双轮速度 PI、前馈、差速修正合成和 PWM 限幅 |
| `Application/Control/MotionLine.c/.h` | 头文件顶部保存巡线参数；源文件实现五路灰度离散权重差速、连续丢线确认、丢线停车和状态管理；巡线层不使用 PID |
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

## 5. 公共函数接口

以下只列出头文件公开声明。`.c` 文件内的 `static` 函数和变量属于文件内部实现，不作为跨模块接口使用。

### 5.1 `Application/Comms/BluetoothDebug.h`

```c
void BluetoothDebug_Init(void);
void BluetoothDebug_Update(uint8_t elapsedTicks);
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
MotionLine_State_t MotionLine_GetState(void);
MotionLine_Error_t MotionLine_GetError(void);
float MotionLine_GetLineError(void);
```

`MotionLine_GetLineError()` 当前返回最近一次有效灰度位图得到的离散权重，范围为 `-3~+3`，它不再是 PID 输入误差。

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

## 6. 公共参数和公共数据

| 所在头文件 | 名称 | 当前值/类型 | 含义 |
|---|---|---:|---|
| `BluetoothDebug.h` | `BLUETOOTH_COMMAND_IDLE_TICKS` | `3U` | 无结束符命令的 30 ms 空闲判定 |
| `K230Link.h` | `K230_LINK_FRAME_MAGIC_0/1` | `0xAAU` / `0x55U` | K230 帧头 |
| `K230Link.h` | `K230_LINK_FRAME_VERSION` | `0x01U` | 当前通信协议版本 |
| `K230Link.h` | `K230_LINK_MAX_PAYLOAD_LENGTH` | `32U` | 允许接收的最大 PAYLOAD 长度 |
| `K230Link.h` | `K230_LINK_READY_RETRY_TICKS` | `10U` | 100 Hz 下每 100 ms 重发 READY |
| `K230Link.h` | `K230_LINK_MESSAGE_READY/READY_ACK/TARGET` | `0x01U` / `0x02U` / `0x10U` | 消息类型编号 |
| `DebugDisplay.h` | `DEBUG_DISPLAY_REFRESH_TICKS` | `10U` | OLED 10 Hz 刷新间隔 |
| `MotionStraight.h` | `MOTION_STRAIGHT_*` | 见 6.2 | 航向 PD、直线速度规划、减速起点比例和距离允许误差 |
| `MotionWheel.h` | `MOTION_WHEEL_*` | 见 6.1 | MotionStraight、MotionLine 与 Nav 共用的速度 PI、前馈和 PWM 限幅 |
| `MotionLine.h` | `MOTION_LINE_*` | 见 6.3 | 灰度权重、最大速度调整比例、巡线速度上限和丢线确认节拍 |
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
| `MOTION_STRAIGHT_HEADING_LIMIT_PWM` / `MOTION_STRAIGHT_CORRECTION_SIGN` | `300.0f` / `-1` |
| `MOTION_STRAIGHT_MAX_SPEED_MMPS` | `600.0f` |
| `MOTION_STRAIGHT_ACCELERATION_MMPS2` / `MOTION_STRAIGHT_DECELERATION_MMPS2` | `200.0f` / `250.0f` |
| `MOTION_STRAIGHT_DECELERATION_START_RATIO` / `MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM` | `5.0f / 6.0f` / `5.0f` |

### 6.3 `MotionLine.h` 参数

以下宏位于 `Application/Control/MotionLine.h` 开头，当前已接入主流程但尚未完成实车巡线标定：

| 宏 | 单位 | 当前值 | 作用 |
|---|---:|---:|---|
| `MOTION_LINE_OUTER_WEIGHT` | 无 | `3` | 左右最外侧灰度权重的绝对值，对应最大修正力度 |
| `MOTION_LINE_INNER_WEIGHT` | 无 | `1` | 左右内侧灰度权重的绝对值，对应最大修正力度的三分之一 |
| `MOTION_LINE_MAX_ADJUST_RATIO` | 比例 | `0.5f` | 权重达到正负 3 时，一侧减去、另一侧增加的巡线速度比例 |
| `MOTION_LINE_MAX_SPEED_MMPS` | mm/s | `1000.0f` | 巡线请求软件上限；应结合公共轮速前馈和最终 PWM 上限设置 |
| `MOTION_LINE_LOST_CONFIRM_TICKS` | 100 Hz 节拍 | `10U` | 连续五路全白达到 10 次后确认丢线，当前约为 100 ms |

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

`PID_t` 的 `Kp/Ki/Kd`、`integral`、`prevError`、`outMax` 和 `integralMax` 为 PID 实例的公共状态与参数。除上述公开声明外，其余 `static` 数据和源文件内宏均为模块内部实现。
