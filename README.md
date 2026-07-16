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
| SysTick | 32 MHz / 320000 = 100 Hz | `System/Tick`、`main.c`、`Application/Control/Drive` | 10 ms 系统节拍，`TICK_DT=0.01 s`；主循环调用 `Drive_Update()` 更新直线闭环 |
| TIMG8 | 32 MHz，周期 1600 = 20 kHz | `Hardware/Motor/PWM`、`Application/Control/Drive`（间接） | 电机 PWM；CCP0/PB15 为右轮，CCP1/PB16 为左轮 |
| TIMA0 | BUSCLK / 32 = 1 MHz，周期 20000 = 50 Hz | `Application/Servo` | 双路舵机 PWM；1 个计数等于 1 us |
| I2C0 | BUSCLK 32 MHz，SCL 400 kHz | `Hardware/Display/OLED` | OLED 控制器通信 |
| UART1 | BUSCLK 32 MHz，9600 baud，8N1，RX 中断 | `Hardware/Comms/Serial`、`Application/Comms/BluetoothDebug` | 蓝牙调试命令和应答 |
| UART2 | BUSCLK 32 MHz，115200 baud，8N1，RX 外设中断已配置 | `main.syscfg` 预留 | K230 接口预留；当前 `main.c` 不启用 NVIC、不解析数据 |
| GPIO 软件 I2C | CPU 延时产生时序 | `Hardware/Sensors/MPU6050`、`Application/Control/Drive`（间接） | MPU6050 连续多圈航向反馈；不占用 I2C 外设实例 |
| GPIOA GROUP1 IRQ | A/B 相双边沿 | `Hardware/Motor/Encoder`、`Application/Control/Drive`（间接） | 左右编码器软件正交解码，为直线控制提供路程和速度反馈 |

## 2. Pin 口占用

| Pin | 方向/复用 | 占用对象 | 程序映射与说明 |
|---|---|---|---|
| PA10 | 开漏式 GPIO | MPU6050 SCL | 软件 I2C 时钟；`Drive` 连续多圈航向反馈来源 |
| PA11 | 开漏式 GPIO | MPU6050 SDA | 软件 I2C 数据；`Drive` 连续多圈航向反馈来源 |
| PA12 | GPIO 输出 | 右电机 AIN2 | TB6612 A 通道方向；`Drive` 右轮输出 |
| PA13 | GPIO 输出 | 右电机 AIN1 | TB6612 A 通道方向；`Drive` 右轮输出 |
| PA14 | GPIO 输入、上拉 | 灰度 CH3 | `Graydetect` 位图 bit3 |
| PA15 | GPIO 输入、上拉、双边沿中断 | 右编码器 A | GPIOA GROUP1 IRQ；`Drive` 右轮反馈 |
| PA16 | GPIO 输入、上拉、双边沿中断 | 右编码器 B | GPIOA GROUP1 IRQ；`Drive` 右轮反馈 |
| PA17 | GPIO 输入、上拉、双边沿中断 | 左编码器 A | GPIOA GROUP1 IRQ；`Drive` 左轮反馈 |
| PA19 | SWDIO | 下载调试 | 不作为普通 GPIO 使用 |
| PA20 | SWCLK | 下载调试 | 不作为普通 GPIO 使用 |
| PA21 | UART2 TX | K230 预留 | 当前主程序不发送 K230 数据 |
| PA22 | UART2 RX | K230 预留 | 当前主程序不接收 K230 数据 |
| PA24 | GPIO 输入、上拉、双边沿中断 | 左编码器 B | GPIOA GROUP1 IRQ；`Drive` 左轮反馈 |
| PA28 | I2C0 SDA | OLED | 400 kHz |
| PA30 | GPIO 输入、上拉 | KEY1 | 低电平按下，按键位图 bit0；调用 `Drive_StartForward(1200U, DRIVE_SPEED_NORMAL)` |
| PA31 | I2C0 SCL | OLED | 400 kHz |
| PB0 | GPIO 输出 | 左电机 BIN1 | TB6612 B 通道方向；`Drive` 左轮输出 |
| PB1 | GPIO 输出 | 左电机 BIN2 | TB6612 B 通道方向；`Drive` 左轮输出 |
| PB6 | UART1 TX | 蓝牙 | MCU 发送到蓝牙 RX |
| PB7 | UART1 RX、上拉 | 蓝牙 | 蓝牙 TX 发送到 MCU，RX 中断接收 |
| PB8 | TIMA0 CCP0 | 横向舵机 | `D` 命令，`Servo_SetHorizontalAngle()` |
| PB9 | TIMA0 CCP1 | 纵向舵机 | `O` 命令，`Servo_SetVerticalAngle()` |
| PB10 | GPIO 输入、上拉 | KEY4 | 低电平按下，按键位图 bit3 |
| PB11 | GPIO 输入、上拉 | KEY2 | 低电平按下，按键位图 bit1；立即停止直线测试 |
| PB14 | GPIO 输入、上拉 | KEY3 | 低电平按下，按键位图 bit2 |
| PB15 | TIMG8 CCP0 | 右电机 PWM | TB6612 A 通道，20 kHz；`Drive` 右轮输出 |
| PB16 | TIMG8 CCP1 | 左电机 PWM | TB6612 B 通道，20 kHz；`Drive` 左轮输出 |
| PB17 | GPIO 输出 | 蜂鸣器 | 低电平有效 |
| PB18 | GPIO 输入、上拉 | 灰度 CH4 | `Graydetect` 位图 bit4 |
| PB20 | GPIO 输入、上拉 | 灰度 CH2 | `Graydetect` 位图 bit2 |
| PB23 | GPIO 输出 | LED1 | 高电平点亮 |
| PB24 | GPIO 输入、上拉 | 灰度 CH1 | `Graydetect` 位图 bit1 |
| PB25 | GPIO 输入、上拉 | 灰度 CH0 | `Graydetect` 位图 bit0 |
| PB27 | GPIO 输出 | LED2 | 高电平点亮；蜂鸣提示同步使用 |

## 3. 当前程序调用关系

### 3.1 启动阶段

`main.c::App_Init()` 按以下顺序运行：

1. `SYSCFG_DL_init()` 应用 `main.syscfg` 生成的时钟、PinMux 和外设配置。
2. 初始化 Tick、LED、蜂鸣器、按键、灰度、电机、舵机、蓝牙串口和编码器里程计。
3. 开启全局中断并初始化 OLED。
4. 初始化 MPU6050；OLED 显示 `ZERO CALIBRATING...`，要求小车保持静止。
5. `Heading_Calibrate()` 采集 400 次 Z 轴陀螺仪零偏，采样间隔 2 ms；若 MPU6050 不在线，OLED 显示 `OFFLINE`。
6. 清除零漂阶段累计的 Tick 和编码器计数，初始化蓝牙命令解析器。
7. 调用 `Drive_InitDefault()`，使用 `g_driveConfig` 初始化 `Drive`；参数非法时长鸣一次。

### 3.2 100 Hz 主循环

`Tick_PollCount()` 每次取出累计节拍，随后依次调用：

```text
Heading_Update -> Odometry_Update -> 按键边沿检测 -> Drive_Update
               -> BluetoothDebug_Update -> 状态蜂鸣提示
               -> Beep_Tick -> DebugDisplay_Update
```

当前直线测试入口：按下 KEY1 后以 `DRIVE_SPEED_NORMAL=100 mm/s` 前进 `1200 mm`；按下 KEY2 立即停止。正常到达后短鸣两次，初始化或启动失败、运行错误时长鸣一次。普通流程使用速度档位，精确速度仍可通过底层接口指定。

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
| 7 | 纵向舵机角度 `O`、横向舵机角度 `D` |

### 3.3 蓝牙调试协议

命令不区分大小写。推荐以 `\r` 或 `\n` 结束；也支持空格、逗号、分号作为分隔符。没有结束符时，接收空闲 3 个系统节拍（30 ms）后执行。每条命令都会返回 `OK ...` 或 `ERR ...`。

直线行驶上机测试期间关闭蓝牙模块电源，避免 `L/R/U` 与 `Drive` 同时改写电机 PWM。当前 Pin 口表中没有蓝牙电源控制引脚，因此固件不能控制模块断电，需要使用外部电源开关或断开模块供电。需要蓝牙手动调试时再给模块上电，并且不要同时按 KEY1 启动直线测试。

| 命令 | 作用 | 输入范围与限位 | 示例 |
|---|---|---|---|
| `L<number>` | 只更新左轮 PWM，右轮保持上次指令 | `-1000~1000`，超限自动夹紧 | `L10` |
| `R<number>` | 只更新右轮 PWM，左轮保持上次指令 | `-1000~1000`，超限自动夹紧 | `R10` |
| `U<number>` | 左右轮使用相同 PWM | `-1000~1000`；正数前进，负数后退 | `U100` |
| `O<number>` | 纵向舵机移动到指定角度 | 当前限位 `0°~270°` | `O10` |
| `D<number>` | 横向舵机移动到指定角度 | 当前限位 `0°~270°` | `D10` |

`L/R/U` 的数字是开环 PWM 指令，不是 mm/s 物理速度。OLED 上的 `LV/RV` 才是由编码器和 `Odometry_CountsPerMM` 换算的实测速度。

当前舵机限位仅对应源码中 270° 舵机的电气量程。实车连杆若存在更小的机械行程，通电调试前必须先收紧 `SERVO_VERTICAL_*_ANGLE` 和 `SERVO_HORIZONTAL_*_ANGLE`，并同步修改本文的 Pin 口表、协议表和公共参数表。

### 3.4 `Drive` 直线行驶控制库

`Application/Control/Drive` 已接入 `main.c`，以 100 Hz 非阻塞运行。上机测试时由 KEY1 启动、KEY2 停止；蓝牙模块按 3.3 节要求断电。

控制结构分为三层：

1. 距离层读取左右编码器相对路程的平均值，根据剩余距离和减速度计算允许速度，并生成加减速曲线。
2. 速度层分别对左右轮编码器实测速度执行 PI，叠加实测得到的速度前馈和静摩擦补偿。
3. 航向层锁定启动瞬间的 MPU6050 连续累计偏航角，使用 PD 生成左右轮差速修正，不做 ±180° 归一化。

调用顺序：

```text
上电且 MPU 零漂完成
    -> Drive_InitDefault()
    -> 按 KEY1
    -> Drive_StartForward(1200U, DRIVE_SPEED_NORMAL)

每个 100 Hz 节拍：
Heading_Update(dt) -> Odometry_Update(ticks) -> Drive_Update(dt)
```

直线行驶参数集中在 `Application/Control/DriveConfig.c` 的 `g_driveConfig`。当前已写入第一轮低速测试值，不是最终标定结果。普通流程使用便利接口，`main.c` 中的实际调用关系为：

```c
#include "Application/Control/Drive.h"

/* 初始化阶段：必须在电机、里程计和 MPU6050 零漂标定完成后调用。 */
if (Drive_InitDefault() != DRIVE_RESULT_OK)
{
    /* 默认参数范围非法。 */
}

/* KEY1 按下沿：前进 1200 mm，使用普通速度档 100 mm/s。 */
(void)Drive_StartForward(1200U, DRIVE_SPEED_NORMAL);

/* 100 Hz 主循环：先更新传感器状态，再更新 Drive。 */
Heading_Update(elapsedSeconds);
Odometry_Update(elapsedTicks);
Drive_Update(elapsedSeconds);
```

- `distanceMM > 0` 表示前进，`distanceMM < 0` 表示后退。
- `speedMMps` 必须为正数；超过 `maximumSpeedMMps` 时自动限幅。
- 常用流程使用 `Drive_StartForward()` 或 `Drive_StartBackward()`，距离参数始终填写正整数。
- `DRIVE_SPEED_SLOW/NORMAL/FAST` 分别为 `60/100/120 mm/s`；需要其他速度时使用 `Drive_StartStraight()`。
- 到达 `distanceToleranceMM` 后先主动制动 `brakeDurationS`，再停止 PWM。
- MPU6050 掉线、里程换算无效或更新周期非法时立即停止并进入错误状态。
- `Drive` 直接使用 `Odometry` 已更新的左右路程和速度，不能再次读取 `Encoder_Get()`，否则会提前清空编码器增量。
- `Drive` 运行期间不要调用 `Heading_SetYaw()` 重置角度，否则会改变本次直线行驶的航向基准。

实车调参顺序：

1. 单独进行蓝牙开环测试时，使用 `U100/U200/...` 记录 OLED 稳态速度，拟合 `feedforwardPWMPerMMps`，并确定静摩擦 PWM 与最大可靠速度；完成后关闭蓝牙模块再测 `Drive`。
2. 先令 `ki=0`，逐步增加速度 `kp` 到响应足够快且不持续振荡，再少量加入 `ki` 消除稳态误差。
3. 低速测试航向 `kp`；若偏差被放大，将 `correctionSign` 从 `1` 改为 `-1` 或反向。随后少量增加 `kd` 抑制摆动。
4. 最后调整加速度、减速度、最小接近速度、距离允许误差和制动时间。

## 4. 工程文件类型与职责

| 文件或目录 | 类型 | 职责 |
|---|---|---|
| `main.c` | C 源文件 | 系统初始化、启动零漂、KEY1/KEY2 直线测试入口和 100 Hz 主循环调度 |
| `main.syscfg` | TI SysConfig | 时钟树、GPIO、UART、I2C、PWM、SysTick 和 PinMux 的唯一配置源 |
| `.project`、`.cproject`、`.settings/` | CCS 工程元数据 | 工程名、TI Arm Clang 选项、SDK/SysConfig 依赖和 IDE 设置 |
| `targetConfigs/*.ccxml` | CCS 目标配置 | MSPM0G3507 调试连接配置 |
| `Application/Comms/` | 应用层 C 模块 | 蓝牙调试命令解析、限幅、执行与应答 |
| `Application/Control/` | 应用层 C 模块 | 通用 PID，以及基于 MPU6050 和双编码器的直线行驶闭环 |
| `Application/Debug/` | 应用层 C 模块 | OLED 调试页面编排与 10 Hz 刷新 |
| `Application/Servo/` | 舵机硬件模块 | TIMA0 双通道 PWM、角度限位和脉宽换算 |
| `Application/State/` | 状态层 C 模块 | Z 轴航向角解算、编码器里程与速度状态 |
| `Hardware/Board/` | 板级驱动 | 按键、LED、蜂鸣器 |
| `Hardware/Comms/` | 通信驱动 | UART1 中断接收环形缓冲区和发送接口 |
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
| `Application/Control/PID.c/.h` | 通用 PID 初始化、调参、复位和单步计算 |
| `Application/Control/Drive.c/.h` | 直线距离规划、便利调用接口、左右轮速度 PI、MPU6050 航向 PD 和停车状态机 |
| `Application/Control/DriveConfig.c/.h` | 集中保存 `g_driveConfig` 第一轮测试值和后续实车调参数据 |
| `Application/Debug/DebugDisplay.c/.h` | 组织启动零漂提示和 OLED 八行调试数据 |
| `Application/Servo/Servo.c/.h` | 将舵机角度换算为 TIMA0 比较值，并执行纵向/横向限位 |
| `Application/State/Heading.c/.h` | MPU6050 Z 轴零漂标定、角速度积分和尺度标定 |
| `Application/State/Odometry.c/.h` | 读取编码器增量，累计左右路程并计算 mm/s 速度 |
| `Hardware/Board/Beep.c/.h` | 蜂鸣器与 LED2 的非阻塞提示状态机 |
| `Hardware/Board/Key.c/.h` | 四个低有效按键的非阻塞状态读取 |
| `Hardware/Board/LED.c/.h` | LED1、LED2 的开、关、翻转接口 |
| `Hardware/Comms/Serial.c/.h` | UART1 RX 中断、1024 字节环形缓冲区和阻塞发送 |
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

### 5.3 `Application/Control/Drive.h`

```c
typedef enum
{
    DRIVE_SPEED_SLOW = 60,
    DRIVE_SPEED_NORMAL = 100,
    DRIVE_SPEED_FAST = 120
} Drive_Speed_t;

Drive_Result_t Drive_Init(const Drive_Config_t *config);
Drive_Result_t Drive_InitDefault(void);
Drive_Result_t Drive_StartStraight(float distanceMM, float speedMMps);
Drive_Result_t Drive_StartForward(uint32_t distanceMM, Drive_Speed_t speed);
Drive_Result_t Drive_StartBackward(uint32_t distanceMM, Drive_Speed_t speed);
void Drive_Update(float dt);
void Drive_Stop(void);
uint8_t Drive_IsConfigured(void);
uint8_t Drive_IsBusy(void);
uint8_t Drive_IsFinished(void);
Drive_State_t Drive_GetState(void);
Drive_Error_t Drive_GetError(void);
float Drive_GetRemainingDistanceMM(void);
```

`Drive_State_t` 包含空闲、运行、制动、完成和错误状态；`Drive_Error_t` 区分 MPU 掉线、里程换算无效和更新周期非法；`Drive_Result_t` 返回启动、忙、参数、配置和传感器检查结果。

### 5.4 `Application/Debug/DebugDisplay.h`

```c
void DebugDisplay_Init(void);
void DebugDisplay_ShowHeadingCalibration(uint8_t mpuReady);
void DebugDisplay_Update(uint8_t elapsedTicks);
```

### 5.5 `Application/Servo/Servo.h`

```c
void Servo_Init(void);
void Servo_SetVerticalAngle(uint16_t angle);
void Servo_SetHorizontalAngle(uint16_t angle);
uint16_t Servo_GetVerticalAngle(void);
uint16_t Servo_GetHorizontalAngle(void);
void Servo_Reset(void);
```

### 5.6 `Application/State/Heading.h`

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

### 5.7 `Application/State/Odometry.h`

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

### 5.8 `Hardware/Board/Beep.h`

```c
void Beep_Init(void);
void Beep_On(void);
void Beep_Off(void);
void Beep_Notify(uint8_t times);
void Beep_Long(void);
void Beep_Tick(void);
```

### 5.9 `Hardware/Board/Key.h`

```c
void Key_Init(void);
uint8_t Key_GetPressedMask(void);
uint8_t Key_GetNum(void);
```

`Key_GetPressedMask()` 的 bit0~bit3 对应 KEY1~KEY4；`Key_GetNum()` 返回当前第一个按下的键号，未按下返回 0。两个读取接口均不阻塞。

### 5.10 `Hardware/Board/LED.h`

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

### 5.11 `Hardware/Comms/Serial.h`

```c
void Serial1_Init(void);
uint32_t Serial1_Available(void);
uint8_t Serial1_ReadByte(uint8_t *byte);
void Serial1_SendByte(uint8_t byte);
void Serial1_SendArray(const uint8_t *array, uint16_t length);
void Serial1_SendString(const char *string);
void Serial1_Printf(const char *format, ...);
```

### 5.12 `Hardware/Display/OLED.h`

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

### 5.13 `Hardware/Motor/Encoder.h`

```c
void Encoder_Init(void);
int16_t Encoder_Get(uint8_t n);
```

`Encoder_Get(1)` 读取并清零左编码器增量，`Encoder_Get(2)` 读取并清零右编码器增量。

### 5.14 `Hardware/Motor/Motor.h`

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

### 5.15 `Hardware/Motor/PWM.h`

```c
void PWM_Init(void);
void PWM_SetCompareA(uint16_t Compare);
void PWM_SetCompareB(uint16_t Compare);
```

### 5.16 `Hardware/Sensors/Graydetect.h`

```c
void Graydetect_Init(void);
uint8_t Graydetect_GetState(void);
uint8_t Graydetect_GetBit(uint8_t index);
float Graydetect_GetError(uint8_t side);
uint8_t Graydetect_OnLine(uint8_t side);
```

### 5.17 `Hardware/Sensors/MPU6050.h`

```c
void MPU6050_Init(void);
uint8_t MPU6050_IsReady(void);
uint8_t MPU6050_GetID(void);
void MPU6050_GetData(int16_t *ax, int16_t *ay, int16_t *az,
                     int16_t *gx, int16_t *gy, int16_t *gz);
int16_t MPU6050_GetGyroZ(void);
```

### 5.18 `System/Delay.h`

```c
void Delay_us(uint32_t us);
void Delay_ms(uint32_t ms);
void Delay_s(uint32_t s);
```

### 5.19 `System/Tick.h`

```c
void Tick_Init(void);
uint8_t Tick_Poll(void);
uint8_t Tick_PollCount(void);
```

## 6. 公共参数和公共数据

| 所在头文件 | 名称 | 当前值/类型 | 含义 |
|---|---|---:|---|
| `BluetoothDebug.h` | `BLUETOOTH_COMMAND_IDLE_TICKS` | `3U` | 无结束符命令的 30 ms 空闲判定 |
| `DebugDisplay.h` | `DEBUG_DISPLAY_REFRESH_TICKS` | `10U` | OLED 10 Hz 刷新间隔 |
| `Drive.h` | `Drive_Config_t` | 实车标定结构体 | 速度 PI、航向 PD、前馈、运动规划、限幅和制动参数 |
| `Drive.h` | `Drive_Speed_t` | `60/100/120 mm/s` | 便利接口的慢速、普通、快速档位 |
| `DriveConfig.h` | `g_driveConfig` | `const Drive_Config_t` | 直线行驶集中调参对象；当前为第一轮低速测试值 |
| `Servo.h` | `SERVO_PHYSICAL_RANGE_DEG` | `270U` | 脉宽换算对应的舵机物理量程 |
| `Servo.h` | `SERVO_MIN_PULSE_US` / `SERVO_MAX_PULSE_US` | `500U` / `2500U` | 舵机最小/最大高电平脉宽 |
| `Servo.h` | `SERVO_FRAME_US` | `20000U` | 50 Hz 舵机帧周期 |
| `Servo.h` | `SERVO_VERTICAL_MIN_ANGLE` / `MAX` / `DEFAULT` | `0U` / `270U` / `135U` | 纵向轴限位与上电角度 |
| `Servo.h` | `SERVO_HORIZONTAL_MIN_ANGLE` / `MAX` / `DEFAULT` | `0U` / `270U` / `135U` | 横向轴限位与上电角度 |
| `Heading.h` | `HEADING_CALIBRATION_SAMPLES` | `400U` | 开机零漂采样数 |
| `Heading.h` | `HEADING_CALIBRATION_INTERVAL_MS` | `2U` | 零漂采样间隔 |
| `Odometry.h` | `Odometry_CountsPerMM` | `float`，初值 `6.23f` | 每毫米编码器计数，必须按实车标定 |
| `Serial.h` | `SERIAL1_RX_BUFFER_SIZE` | `1024U` | UART1 环形接收缓冲区容量 |
| `Serial.h` | `Serial1_RxFlag` | `volatile uint8_t` | UART1 存在未读数据标志 |
| `PWM.h` | `PWM_MAX_DUTY` | `1000U` | 电机 PWM 指令绝对值上限 |
| `Graydetect.h` | `GRAY_SIDE_ALL/LEFT/RIGHT` | `0/1/2` | 灰度误差计算的通道范围 |
| `OLED.h` | `OLED_8X16` / `OLED_6X8` | `8U` / `6U` | 字体尺寸选择 |
| `OLED.h` | `OLED_UNFILLED` / `OLED_FILLED` | `0U` / `1U` | 图形空心/实心选择 |
| `OLED_Data.h` | `OLED_F8x16`、`OLED_F6x8`、`OLED_CF16x16`、`Diode` | `const` 字模/位图数组 | OLED 公共显示数据 |
| `OLED_Data.h` | `ChineseCell_t` / `OLED_CHARSET_UTF8` | 字模结构 / 字符集宏 | 中文字模索引与 16×16 数据格式 |
| `Tick.h` | `TICK_HZ` / `TICK_DT` | `100U` / `0.01f` | 系统节拍频率与秒单位周期 |

### 6.1 `Drive_Config_t` 参数

以下字段位于 `Application/Control/DriveConfig.c` 的 `g_driveConfig` 中。当前数值只用于第一轮低速上机测试，必须根据编码器、车体和路面响应继续标定：

| 字段 | 单位 | 作用 |
|---|---:|---|
| `speed.kp` | PWM/(mm/s) | 单轮速度比例增益 |
| `speed.ki` | PWM/mm | 单轮速度积分增益 |
| `speed.integralLimit` | mm | 速度积分绝对值限幅；`ki>0` 时必须大于 0 |
| `speed.feedforwardPWMPerMMps` | PWM/(mm/s) | 速度到 PWM 的线性前馈斜率 |
| `speed.staticFrictionPWM` | PWM | 克服静摩擦所需的符号前馈 |
| `heading.kp` | PWM/° | 航向误差比例增益 |
| `heading.kd` | PWM/(°/s) | 航向误差微分增益 |
| `heading.correctionLimitPWM` | PWM | 航向差速修正绝对值上限，必须大于 0 |
| `heading.correctionSign` | `1` 或 `-1` | 航向差速方向；偏差被放大时翻转符号 |
| `maximumSpeedMMps` | mm/s | 允许请求的最大直线速度，超出时自动限幅 |
| `maximumCommandPWM` | PWM | 最终单轮 PWM 限幅，不得超过 `PWM_MAX_DUTY` |
| `accelerationMMps2` | mm/s² | 目标速度上升斜率 |
| `decelerationMMps2` | mm/s² | 减速曲线使用的减速度 |
| `minimumApproachSpeedMMps` | mm/s | 接近终点时防止电机停滞的最低目标速度 |
| `distanceToleranceMM` | mm | 进入停车流程的剩余距离允许误差 |
| `brakeDurationS` | s | 到点后主动制动持续时间；为 0 时直接停止 PWM |

当前第一轮测试值：

| 字段 | 当前值 |
|---|---:|
| `speed.kp` / `speed.ki` / `speed.integralLimit` | `1.0f` / `0.0f` / `0.0f` |
| `speed.feedforwardPWMPerMMps` / `speed.staticFrictionPWM` | `2.0f` / `80.0f` |
| `heading.kp` / `heading.kd` | `6.0f` / `0.4f` |
| `heading.correctionLimitPWM` / `heading.correctionSign` | `100.0f` / `-1` |
| `maximumSpeedMMps` / `maximumCommandPWM` | `120.0f` / `400.0f` |
| `accelerationMMps2` / `decelerationMMps2` | `150.0f` / `200.0f` |
| `minimumApproachSpeedMMps` / `distanceToleranceMM` | `20.0f` / `5.0f` |
| `brakeDurationS` | `0.05f` |

`PID_t` 的 `Kp/Ki/Kd`、`integral`、`prevError`、`outMax` 和 `integralMax` 为 PID 实例的公共状态与参数。除上述公开声明外，其余 `static` 数据和源文件内宏均为模块内部实现。
