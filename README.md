# MSPM0G3507 天猛星小车工程索引

> 对项目进行任何修改时，只要涉及硬件连接、Pin 口、外设实例、时钟树、波特率、定时器、主循环调用关系或程序功能，就必须同时修改本文的 Pin 口表、时钟表和对应程序说明。禁止只修改代码而不更新本文档。

本文只记录工程文件类型、文件职责、公共函数和公共参数。函数声明以当前头文件为准；`static` 函数和变量属于文件内部实现，不作为跨模块接口记录。`main.syscfg` 是 PinMux、时钟和外设实例的唯一配置源，`Debug/`、`Release/` 和 `ti_msp_dl_config.*` 均为生成内容。

## 1. 时钟与外设占用

| 资源 | 当前配置 | 占用模块 | 作用 |
|---|---:|---|---|
| CPUCLK / SYSCLK | 32 MHz | 全工程 | CPU、总线外设和 SysTick 基准时钟 |
| SysTick | 100 Hz，周期 320000 | `System/Tick`、`App`、`MotionManager`、`Mission` | 10 ms 系统节拍，向控制与任务层传递真实累计 `dt` |
| TIMG8 | 20 kHz，周期 1600 | `PWM`、`Motor`、`MotionWheel` | 小车左右有刷电机 PWM；CCP0 为右轮，CCP1 为左轮 |
| TIMA0 | 1 MHz，周期 20000 | `Servo` | 50 Hz 双路舵机 PWM；CCP0 横向，CCP1 纵向 |
| I2C0 | 400 kHz，Controller | `OLED`、`DebugDisplay` | OLED 地址 `0x3C`，无 DMA |
| UART1 | 115200 baud，8N1，RX/TX 中断 | `Serial1`、`BluetoothDebug` | 蓝牙二进制控制帧和遥测帧，无 DMA；TX 使用软件环形缓冲 |
| UART2 | 115200 baud，8N1，RX 中断 | `Serial2`、`F32C`、`Gimbal` | F32C 3.3 V TTL 通信，无 DMA |
| GPIO 软件 I2C | CPU 延时产生时序 | `MPU6050`、`Heading` | MPU6050 地址 `0x68`，提供连续多圈 Z 轴航向 |
| GPIOA GROUP1 IRQ | A/B 相双边沿 | `Encoder`、`Odometry` | 左右轮软件正交解码 |

## 2. Pin 口占用

| Pin | 方向 / 复用 | 占用对象 | 程序说明 |
|---|---|---|---|
| PA10 | 开漏式 GPIO | MPU6050 SCL | 软件 I2C 时钟 |
| PA11 | 开漏式 GPIO | MPU6050 SDA | 软件 I2C 数据 |
| PA12 | GPIO 输出 | 右电机 AIN2 | TB6612 A 通道方向 |
| PA13 | GPIO 输出 | 右电机 AIN1 | TB6612 A 通道方向 |
| PA14 | GPIO 输入、上拉 | 灰度 CH3 | 灰度位图 bit3；黑线为 1 |
| PA15 | GPIO 输入、上拉、双边沿中断 | 右编码器 A | 右轮正交反馈 |
| PA16 | GPIO 输入、上拉、双边沿中断 | 右编码器 B | 右轮正交反馈 |
| PA17 | GPIO 输入、上拉、双边沿中断 | 左编码器 A | 左轮正交反馈 |
| PA19 | SWDIO | 下载调试 | 不作为普通 GPIO 使用 |
| PA20 | SWCLK | 下载调试 | 不作为普通 GPIO 使用 |
| PA21 | UART2 TX | F32C RX | MCU 发送到 F32C；X 地址 1、Y 地址 2 |
| PA22 | UART2 RX、上拉 | F32C TX | F32C 反馈到 MCU，RX 中断接收 |
| PA24 | GPIO 输入、上拉、双边沿中断 | 左编码器 B | 左轮正交反馈 |
| PA28 | I2C0 SDA | OLED SDA | 400 kHz 硬件 I2C |
| PA30 | GPIO 输入、上拉 | KEY1 | 低电平按下，按键位图 bit0；当前启动 25H |
| PA31 | I2C0 SCL | OLED SCL | 400 kHz 硬件 I2C |
| PB0 | GPIO 输出 | 左电机 BIN1 | TB6612 B 通道方向 |
| PB1 | GPIO 输出 | 左电机 BIN2 | TB6612 B 通道方向 |
| PB6 | UART1 TX | 蓝牙 RX | MCU 非阻塞发送 `0x01` 遥测帧 |
| PB7 | UART1 RX、上拉 | 蓝牙 TX | 接收调试台 `0x80` 控制帧 |
| PB8 | TIMA0 CCP0 | 横向舵机 | `Servo_SetHorizontalAngle()` |
| PB9 | TIMA0 CCP1 | 纵向舵机 | `Servo_SetVerticalAngle()` |
| PB10 | GPIO 输入、上拉 | KEY4 | 低电平按下，按键位图 bit3 |
| PB11 | GPIO 输入、上拉 | KEY2 | 低电平按下，按键位图 bit1 |
| PB14 | GPIO 输入、上拉 | KEY3 | 低电平按下，按键位图 bit2 |
| PB15 | TIMG8 CCP0 | 右电机 PWM | TB6612 A 通道，20 kHz |
| PB16 | TIMG8 CCP1 | 左电机 PWM | TB6612 B 通道，20 kHz |
| PB17 | GPIO 输出 | 蜂鸣器 | 低电平有效 |
| PB18 | GPIO 输入、上拉 | 灰度 CH4 | 灰度位图 bit4；黑线为 1 |
| PB20 | GPIO 输入、上拉 | 灰度 CH2 | 灰度位图 bit2；黑线为 1 |
| PB23 | GPIO 输出 | LED1 | 高电平点亮 |
| PB24 | GPIO 输入、上拉 | 灰度 CH1 | 灰度位图 bit1；黑线为 1 |
| PB25 | GPIO 输入、上拉 | 灰度 CH0 | 灰度位图 bit0；黑线为 1 |
| PB27 | GPIO 输出 | LED2 | 高电平点亮 |

### 2.1 硬件连接检查

- `main.syscfg` 中没有重复分配的 Pin；UART1、UART2、I2C0、TIMG8、TIMA0、软件 I2C 和 SWD 互不冲突。
- F32C 使用独立 8–15 V 电源和 3.3 V TTL 通信。控制器与电机电源必须共地，PA21 接电机 RX，PA22 接电机 TX；禁止反接和带电插拔。
- 二自由度云台的 X/Y 电机使用厂家级联线束。滑环通道手册标注最大 1 A，禁止过流使用。
- UART2 当前专用于 F32C，K230Link 保留但未运行；K230 与 F32C 不能同时占用 PA21/PA22。

## 3. 当前程序说明

### 3.1 主程序

```c
int main(void)
{
    App_UpdateContext_t updateContext;

    App_Init();
    Mission_Init(Accomplish25H_GetMissionGraph());
    Interrupt_Enable();

    for (;;)
    {
        if (App_Update(&updateContext) != 0U)
        {
            BluetoothDebug_ControlUpdate(Mission_IsAtStartState());
            Mission_Update(&updateContext);
        }
    }
}
```

`App_Init()` 在全局中断关闭状态下初始化整车、OLED、MPU6050、编码器、蓝牙和 F32C 通信。`Gimbal_Init()` 只把 UART2/F32C 初始化为禁用状态，不使能或驱动无刷电机。MPU6050 零漂完成后，`Interrupt_Enable()` 才开启全局中断。

每个有效节拍的调用顺序：

```text
Heading -> Odometry -> Gimbal -> Key -> BluetoothDebug 接收/遥测
        -> MotionManager -> Beep -> OLED
        -> BluetoothDebug 等待态命令入口
        -> Mission_Update
```

### 3.2 当前 25H 流程

```text
等待 KEY1
  -> 200 mm/s 巡线
  -> 灰度 bit0、bit1 同时检测到黑线
  -> 向前直行 150 mm，终点速度 0 mm/s
  -> 转到下一连续绝对角目标：启动航向 - 90°、-180°、-270°……
  -> 重新巡线并循环
```

巡线连续全白达到 `MOTION_LINE_LOST_CONFIRM_TICKS` 后返回等待状态。角度使用 MPU6050 连续累计值，不做 ±180° 归一化。

### 3.3 OLED 与蓝牙等待态控制

OLED 默认显示 Z 轴连续角度、五路灰度、四个按键、左右轮路程/速度和当前运动状态。手动摇杆模式显示 `M:MANUAL`。Gimbal 显式使能后切换为 F32C 双轴角度页。

蓝牙固定帧为 `7A + 版本 + 消息ID + u16序号 + u16长度 + Payload + u16 CRC + 7B`，多字节字段使用小端。CRC 为 CRC16/CCITT-FALSE，从版本字段计算到 Payload 末尾，不包含帧头、CRC 和帧尾。

网站发送 `ID 0x80`，Payload 固定 16 字节。该命令只在 Mission 当前处于起始等待状态时执行；题目状态运行中仍解析帧和回传遥测，但不会接管车轮。

| 偏移 | 字段 | 类型 | 单位 / 作用 |
|---:|---|---|---|
| 0 | x | `i16` | 横向摇杆，mm/s，正数向右 |
| 2 | y | `i16` | 纵向摇杆，mm/s，正数前进 |
| 4 | 前进速度 | `i32` | mm/s |
| 8 | 前进距离 | `i32` | mm |
| 12 | 转向角度 | `i16` | 相对当前航向的角度 |
| 14 | 开始前进 | `bool` | 上升沿触发一次 |
| 15 | 开始转向 | `bool` | 上升沿触发一次 |

设备回传 `ID 0x01`，Payload 固定 21 字节：

| 偏移 | 字段 | 类型 | 单位 / 作用 |
|---:|---|---|---|
| 0 | 左轮实际速度 | `f32` | mm/s |
| 4 | 右轮实际速度 | `f32` | mm/s |
| 8 | MPU Z 轴角度 | `f32` | 连续多圈角度，单位度 |
| 12 | 纵向舵机命令角度 | `u16` | 度，无位置反馈 |
| 14 | 横向舵机命令角度 | `u16` | 度，无位置反馈 |
| 16 | 命令允许 | `bool` | 1 表示 Mission 正处于起始等待状态，蓝牙命令可执行 |
| 17 | 当前动作 | `u8` | 0 空闲、1 摇杆、2 直线、3 转向、4 刹车 |
| 18 | 自动动作运行中 | `bool` | 直线或转向是否未完成 |
| 19 | 最近命令结果 | `u8` | 0 正常、1 参数错误、2 忙、3 启动失败、4 按钮冲突、5 超时、6 等待重新使能 |
| 20 | 运动错误 | `u8` | `MotionManager_Error_t` |

调试台建议以 10 Hz 发送完整控制帧。等待态内 500 ms 未收到合法帧时，若蓝牙正在控制运动则停车；按钮释放且摇杆回中后才能恢复。自动直线或转向期间摇杆无效，动作完成后摇杆重新接管。状态机运行期间收到运动请求会返回 `BUSY`，但不影响当前任务。`f32` 使用 IEEE-754 单精度小端格式。

## 4. 工程文件类型与职责

### 4.1 Application

| 文件 / 目录 | 类型 | 职责 |
|---|---|---|
| `Application/Core/App.c/.h` | 应用运行层 | 完整整车初始化、固定更新链和 Mission 上下文 |
| `Application/Core/TestApp.c/.h` | 可选测试运行层 | 跳过 OLED、MPU6050、灰度和里程的快速测试入口；当前未使用 |
| `Application/Comms/BluetoothDebug.c/.h` | 应用通信层 | 解析调试台二进制控制帧、调度调试动作并回传遥测 |
| `Application/Comms/K230Link.c/.h` | 应用通信层 | K230 帧、CRC8、握手和目标解析；当前未运行 |
| `Application/Control/MotionManager.c/.h` | 统一运动调度 | 保证同一时刻只有手动轮速、直线、巡线、转向或刹车之一控制双轮 |
| `Application/Control/MotionStraight.c/.h` | 直线控制 | 定距直线、连续航向保持和到点短接刹车 |
| `Application/Control/MotionLine.c/.h` | 巡线控制 | 五路灰度离散权重差速和连续丢线确认 |
| `Application/Control/MotionWheel.c/.h` | 公共轮速控制 | 双轮 PI、前馈、差速修正和 PWM 限幅 |
| `Application/Control/Nav.c/.h` | 转向控制 | 双轮反向旋转到连续绝对角或相对角 |
| `Application/Control/PID.c/.h` | 通用控制器 | 位置式 PID 计算、复位和调参 |
| `Application/Debug/DebugDisplay.c/.h` | 显示编排 | OLED 零漂页、整车页和 Gimbal 页 |
| `Application/Gimbal/Gimbal.c/.h` | 云台应用层 | 管理 X/Y 地址、T 型多圈位置目标、反馈和到位状态 |
| `Application/Servo/Servo.c/.h` | 舵机控制 | 双路角度限位与 TIMA0 脉宽换算 |
| `Application/State/Heading.c/.h` | 航向状态 | MPU6050 零漂、连续偏航积分和尺度标定 |
| `Application/State/Odometry.c/.h` | 里程状态 | 编码器增量到双轮路程与速度的换算 |

### 4.2 Hardware 与 System

| 文件 / 目录 | 类型 | 职责 |
|---|---|---|
| `Hardware/Board/` | 板级驱动 | 按键、LED 和蜂鸣器 |
| `Hardware/Comms/Serial.c/.h` | UART 驱动 | UART1 RX/TX 环形缓冲与 UART2 中断接收、阻塞发送 |
| `Hardware/Display/OLED.c/.h` | OLED 驱动 | I2C0 显存、文本、数字、图像和图形绘制 |
| `Hardware/Display/OLED_Data.c/.h` | 字模数据 | ASCII、中文字模和图像数据 |
| `Hardware/Motor/Motor.c/.h` | 有刷电机驱动 | TB6612 方向、PWM、释放和主动刹车 |
| `Hardware/Motor/PWM.c/.h` | PWM 驱动 | TIMG8 双通道占空比换算 |
| `Hardware/Motor/Encoder.c/.h` | 编码器驱动 | GPIO 双边沿中断正交计数 |
| `Hardware/Motor/F32C.c/.h` | 无刷电机协议 | F32C 命令编码、校验和反馈解码 |
| `Hardware/Sensors/Graydetect.c/.h` | 灰度驱动 | 五路 GPIO 位图和区域误差 |
| `Hardware/Sensors/MPU6050.c/.h` | IMU 驱动 | 软件 I2C 初始化和原始数据读取 |
| `System/Delay.c/.h` | 系统基础 | 微秒、毫秒和秒级阻塞延时 |
| `System/Tick.c/.h` | 系统基础 | 100 Hz 累计节拍 |
| `System/Interrupt.c/.h` | 系统基础 | 全局中断统一开关 |

### 4.3 Mission 与 Accomplish

| 文件 / 目录 | 类型 | 职责 |
|---|---|---|
| `Application/Mission/Mission.c/.h` | 通用任务执行层 | 校验并执行静态状态图、回调和有序转换 |
| `Accomplish/25E.c/.h` | 题目状态图 | 25E 参数、状态、回调和转换表 |
| `Accomplish/25H.c/.h` | 当前题目状态图 | KEY1 启动的巡线、150 mm 直行和连续绝对左转循环 |
| `Accomplish/Brushless_Motor_Test.c/.h` | 可选测试状态图 | F32C 双轴多圈位置循环测试；当前未加载 |
| `状态机.md` | 使用说明 | 新建 Accomplish 状态图的编写流程 |

### 4.4 工程入口与配置

| 文件 | 类型 | 职责 |
|---|---|---|
| `main.c` | C 源文件 | 选择 App 运行通道和当前 Accomplish 状态图 |
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

void BluetoothDebug_Init(void);                   /* 清空协议、控制和遥测状态。 */
void BluetoothDebug_Update(uint8_t elapsedTicks); /* 拆包、检查超时并周期回传遥测。 */
void BluetoothDebug_ControlUpdate(uint8_t commandAllowed); /* 等待态允许时处理按钮动作和摇杆轮速。 */
uint8_t BluetoothDebug_IsControlling(void);       /* 返回蓝牙是否正在占用运动调度器。 */
BluetoothDebug_Result_t BluetoothDebug_GetLastResult(void); /* 返回最近命令结果。 */

void K230Link_Init(void);                         /* 初始化 K230 帧解析与握手状态。 */
void K230Link_Update(uint8_t elapsedTicks);        /* 按节拍推进握手并解析接收帧。 */
uint8_t K230Link_IsReady(void);                    /* 返回双方握手是否完成。 */
uint8_t K230Link_GetTarget(K230Link_Target_t *target); /* 读取最新目标数据。 */

void DebugDisplay_Init(void);                     /* 初始化 OLED 与刷新计数。 */
void DebugDisplay_ShowHeadingCalibration(uint8_t mpuReady); /* 显示 MPU 零漂或离线页。 */
void DebugDisplay_Update(uint8_t elapsedTicks);    /* 按刷新周期更新整车或 Gimbal 页面。 */
```

| 头文件 | 公共参数 | 当前值 | 作用 |
|---|---|---:|---|
| `BluetoothDebug.h` | `BLUETOOTH_DEBUG_FRAME_HEAD`、`BLUETOOTH_DEBUG_FRAME_TAIL` | `0x7A/0x7B` | 调试台固定帧头和帧尾 |
| `BluetoothDebug.h` | `BLUETOOTH_DEBUG_PROTOCOL_VERSION` | `0x01` | 协议版本 |
| `BluetoothDebug.h` | `BLUETOOTH_DEBUG_MESSAGE_CONTROL`、`BLUETOOTH_DEBUG_MESSAGE_TELEMETRY` | `0x80/0x01` | 控制与遥测消息 ID |
| `BluetoothDebug.h` | `BLUETOOTH_DEBUG_CONTROL_PAYLOAD_LENGTH`、`BLUETOOTH_DEBUG_TELEMETRY_PAYLOAD_LENGTH` | `16U/21U` | 固定 Payload 长度 |
| `BluetoothDebug.h` | `BLUETOOTH_DEBUG_TELEMETRY_TICKS` | `10U` | 100 ms 遥测周期 |
| `BluetoothDebug.h` | `BLUETOOTH_DEBUG_TIMEOUT_TICKS` | `50U` | 500 ms 无合法控制帧时停车 |
| `BluetoothDebug.h` | `BLUETOOTH_DEBUG_TURN_SPEED_MMPS` | `100.0f` | 调试相对转向轮速 |
| `BluetoothDebug.h` | `BLUETOOTH_DEBUG_MANUAL_MAX_SPEED_MMPS` | `500.0f` | 摇杆左右轮目标速度上限 |
| `BluetoothDebug.h` | `BLUETOOTH_DEBUG_JOYSTICK_DEADZONE_MMPS` | `10.0f` | 摇杆速度死区 |
| `K230Link.h` | `K230_LINK_FRAME_MAGIC_0`、`K230_LINK_FRAME_MAGIC_1` | `0xAA/0x55` | 帧头 |
| `K230Link.h` | `K230_LINK_FRAME_VERSION` | `0x01` | 协议版本 |
| `K230Link.h` | `K230_LINK_MAX_PAYLOAD_LENGTH` | `32U` | 最大载荷长度 |
| `K230Link.h` | `K230_LINK_READY_RETRY_TICKS` | `10U` | READY 重发周期，100 ms |
| `K230Link.h` | `K230_LINK_MESSAGE_READY`、`K230_LINK_MESSAGE_READY_ACK`、`K230_LINK_MESSAGE_TARGET` | `0x01/0x02/0x10` | 消息类型 |
| `DebugDisplay.h` | `DEBUG_DISPLAY_REFRESH_TICKS` | `10U` | OLED 刷新周期，100 ms |

### 5.2 MotionManager

```c
MotionManager_Result_t MotionManager_Init(void); /* 初始化全部运动模块。 */
MotionManager_Result_t MotionManager_SetManualWheelSpeeds(
    float leftSpeedMMps, float rightSpeedMMps);   /* 设置调试摇杆双轮目标速度。 */
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
| `MOTION_MANAGER_BRAKE_HOLD_SECONDS` | `0.05f` | MotionManager 主动刹车模式的 `Motor_Brake()` 保持时间；直线到点刹车使用 `MOTION_STRAIGHT_BRAKE_HOLD_SECONDS` |

### 5.3 直线、巡线、转向与轮速

```c
MotionStraight_Result_t MotionStraight_Init(void); /* 初始化直线控制器。 */
MotionStraight_Result_t MotionStraight_Start(
    float distanceMM, float speedMMps, float endSpeedMMps); /* 带符号距离的底层入口。 */
MotionStraight_Result_t MotionStraight_StartForward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps); /* 前进包装。 */
MotionStraight_Result_t MotionStraight_StartBackward(
    uint32_t distanceMM, float speedMMps, float endSpeedMMps); /* 后退包装。 */
void MotionStraight_Update(float dt);               /* 更新直线航向、轮速和到点短接刹车。 */
void MotionStraight_Stop(void);                     /* 停止直线并复位控制器。 */
uint8_t MotionStraight_IsConfigured(void);          /* 返回初始化状态。 */
uint8_t MotionStraight_IsBusy(void);                /* 返回是否运行。 */
uint8_t MotionStraight_IsFinished(void);            /* 返回是否到达目标。 */
MotionStraight_State_t MotionStraight_GetState(void); /* 返回直线状态。 */
MotionStraight_Error_t MotionStraight_GetError(void); /* 返回直线错误。 */
float MotionStraight_GetRemainingDistanceMM(void);  /* 返回带方向的剩余距离。 */

MotionLine_Result_t MotionLine_Init(void);          /* 初始化巡线控制器。 */
MotionLine_Result_t MotionLine_Start(float speedMMps); /* 启动巡线。 */
void MotionLine_Update(float dt);                   /* 更新灰度权重和轮速目标。 */
void MotionLine_Stop(void);                         /* 停止巡线。 */
uint8_t MotionLine_IsConfigured(void);              /* 返回初始化状态。 */
uint8_t MotionLine_IsBusy(void);                    /* 返回是否巡线。 */
uint8_t MotionLine_IsFinished(void);                /* 返回是否确认丢线。 */
MotionLine_State_t MotionLine_GetState(void);       /* 返回巡线状态。 */
MotionLine_Error_t MotionLine_GetError(void);       /* 返回巡线错误。 */
float MotionLine_GetLineError(void);                /* 返回当前离散线路误差。 */

Nav_Result_t Nav_Init(void);                        /* 初始化转向控制器。 */
Nav_Result_t Nav_StartTo(float targetYawDeg, float speedMMps); /* 连续绝对角转向。 */
Nav_Result_t Nav_StartBy(float deltaYawDeg, float speedMMps);  /* 相对角转向。 */
void Nav_Update(float dt);                          /* 更新转向速度规划。 */
void Nav_Stop(void);                                /* 停止转向。 */
uint8_t Nav_IsConfigured(void);                     /* 返回初始化状态。 */
uint8_t Nav_IsBusy(void);                           /* 返回是否转向。 */
uint8_t Nav_IsFinished(void);                       /* 返回是否稳定到角。 */
Nav_State_t Nav_GetState(void);                     /* 返回转向状态。 */
Nav_Error_t Nav_GetError(void);                     /* 返回转向错误。 */
float Nav_GetTargetYawDeg(void);                    /* 返回连续绝对目标角。 */
float Nav_GetAngleErrorDeg(void);                   /* 返回当前角度误差。 */

MotionWheel_Result_t MotionWheel_Init(void);        /* 初始化公共双轮 PI。 */
MotionWheel_Result_t MotionWheel_Update(
    const MotionWheel_Command_t *command, float dt); /* 执行双轮速度和差速修正。 */
void MotionWheel_Reset(void);                       /* 清空 PI 和输出记录。 */
void MotionWheel_Stop(void);                        /* 释放电机并复位 PI。 */
uint8_t MotionWheel_IsConfigured(void);             /* 返回初始化状态。 */
float MotionWheel_GetMaximumCommandPWM(void);       /* 返回公共 PWM 上限。 */
float MotionWheel_GetLeftCommandPWM(void);          /* 返回最近左轮 PWM。 */
float MotionWheel_GetRightCommandPWM(void);         /* 返回最近右轮 PWM。 */
```

| 头文件 | 公共参数 | 当前值 | 作用 |
|---|---|---:|---|
| `MotionStraight.h` | `MOTION_STRAIGHT_HEADING_KP`、`MOTION_STRAIGHT_HEADING_KD` | `6.0/0.4` | 航向回正和阻尼 |
| `MotionStraight.h` | `MOTION_STRAIGHT_HEADING_LIMIT_PWM` | `700.0f` | 航向差速修正上限 |
| `MotionStraight.h` | `MOTION_STRAIGHT_CORRECTION_SIGN` | `-1` | 航向修正方向 |
| `MotionStraight.h` | `MOTION_STRAIGHT_MAX_SPEED_MMPS` | `1000.0f` | 直线速度请求上限 |
| `MotionStraight.h` | `MOTION_STRAIGHT_ACCELERATION_MMPS2` | `300.0f` | 起步加速度 |
| `MotionStraight.h` | `MOTION_STRAIGHT_DISTANCE_TOLERANCE_MM` | `5.0f` | 到达距离容差 |
| `MotionStraight.h` | `MOTION_STRAIGHT_BRAKE_HOLD_SECONDS` | `0.05f` | 终点速度为 0 且到达目标点后的短接刹车保持时间 |
| `MotionLine.h` | `MOTION_LINE_OUTER_WEIGHT`、`MOTION_LINE_INNER_WEIGHT` | `6/3` | 最外侧与内侧灰度权重绝对值 |
| `MotionLine.h` | `MOTION_LINE_MAX_ADJUST_RATIO` | `0.2f` | 最大差速调整比例 |
| `MotionLine.h` | `MOTION_LINE_MAX_SPEED_MMPS` | `1000.0f` | 巡线速度请求上限 |
| `MotionLine.h` | `MOTION_LINE_LOST_CONFIRM_TICKS` | `50U` | 连续全白 500 ms 后确认丢线 |
| `MotionWheel.h` | `MOTION_WHEEL_KP`、`MOTION_WHEEL_KI` | `1.0/0.0` | 双轮速度 PI；积分当前关闭 |
| `MotionWheel.h` | `MOTION_WHEEL_INTEGRAL_LIMIT` | `0.0f` | 轮速积分限幅 |
| `MotionWheel.h` | `MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS` | `2.0f` | 速度到 PWM 的前馈系数 |
| `MotionWheel.h` | `MOTION_WHEEL_STATIC_FRICTION_PWM` | `0.0f` | 非零速度静摩擦补偿 |
| `MotionWheel.h` | `MOTION_WHEEL_MAX_COMMAND_PWM` | `1000.0f` | 每侧最终 PWM 上限 |
| `Nav.h` | `NAV_MAX_TURN_SPEED_MMPS` | `200.0f` | 转向轮速上限 |
| `Nav.h` | `NAV_MIN_TURN_SPEED_MMPS` | `40.0f` | 接近目标时的最低轮速 |
| `Nav.h` | `NAV_SLOWDOWN_ANGLE_DEG` | `45.0f` | 转向减速角度区间 |
| `Nav.h` | `NAV_ACCELERATION_MMPS2` | `150.0f` | 起转加速度 |
| `Nav.h` | `NAV_DECELERATION_MMPS2` | `600.0f` | 接近目标时的减速度 |
| `Nav.h` | `NAV_ANGLE_TOLERANCE_DEG` | `2.0f` | 到角容差 |
| `Nav.h` | `NAV_SETTLE_TICKS` | `3U` | 连续稳定 30 ms 后完成 |
| `Nav.h` | `NAV_ROTATION_COMMAND_SIGN` | `-1` | 转向命令方向 |

### 5.4 PID、Gimbal、Servo 与状态

```c
void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float outMax, float integralMax);     /* 初始化位置式 PID。 */
void PID_SetTunings(PID_t *pid, float kp, float ki, float kd); /* 更新增益。 */
void PID_Reset(PID_t *pid);                         /* 清空积分和上次误差。 */
float PID_Update(PID_t *pid, float setpoint, float measure, float dt); /* 单步计算。 */

Gimbal_Result_t Gimbal_Init(void);                  /* 初始化 F32C 并保持禁用。 */
Gimbal_Result_t Gimbal_Enable(void);                /* 使能并配置两个地址。 */
Gimbal_Result_t Gimbal_Disable(void);               /* 失能两个地址。 */
Gimbal_Result_t Gimbal_SetTargetAngle(
    Gimbal_Axis_t axis, float targetAngleDeg);       /* 设置单轴多圈绝对角。 */
Gimbal_Result_t Gimbal_SetTargetAngles(
    float targetXDeg, float targetYDeg);             /* 设置双轴多圈绝对角。 */
void Gimbal_Update(float dt);                       /* 解析反馈并轮询双轴。 */
uint8_t Gimbal_IsEnabled(void);                     /* 返回使能状态。 */
uint8_t Gimbal_HasFeedback(Gimbal_Axis_t axis);     /* 返回单轴是否收到角度。 */
uint8_t Gimbal_IsAxisAtTarget(Gimbal_Axis_t axis);  /* 返回单轴是否稳定到位。 */
uint8_t Gimbal_AreTargetsReached(void);             /* 返回双轴是否到位。 */
float Gimbal_GetCurrentAngleDeg(Gimbal_Axis_t axis);/* 返回反馈角。 */
float Gimbal_GetTargetAngleDeg(Gimbal_Axis_t axis); /* 返回目标角。 */
Gimbal_State_t Gimbal_GetState(void);               /* 返回运行状态。 */
Gimbal_Error_t Gimbal_GetError(void);               /* 返回错误状态。 */

void Servo_Init(void);                              /* 初始化双路角度状态。 */
void Servo_SetVerticalAngle(uint16_t angle);         /* 限位后设置纵向角。 */
void Servo_SetHorizontalAngle(uint16_t angle);       /* 限位后设置横向角。 */
uint16_t Servo_GetVerticalAngle(void);               /* 返回纵向目标角。 */
uint16_t Servo_GetHorizontalAngle(void);             /* 返回横向目标角。 */
void Servo_Reset(void);                             /* 恢复两个默认角。 */

void Heading_Init(void);                            /* 初始化 MPU 并复位航向。 */
void Heading_Calibrate(void);                       /* 静止阻塞采样零漂。 */
void Heading_Update(float dt);                      /* 积分连续 Z 轴航向。 */
uint8_t Heading_IsReady(void);                      /* 返回 MPU 是否在线。 */
float Heading_GetYaw(void);                         /* 返回连续累计偏航角。 */
void Heading_SetYaw(float yaw);                     /* 设置当前航向基准。 */
void Heading_ScaleCalibStart(void);                 /* 开始陀螺尺度标定。 */
float Heading_ScaleCalibFinish(uint16_t turns);     /* 按真实圈数应用尺度。 */
void Heading_ScaleCalibCancel(void);                /* 取消尺度标定。 */
float Heading_GetCalibAngle(void);                  /* 返回标定累计原始角。 */
float Heading_GetScale(void);                       /* 返回当前尺度因子。 */
void Heading_SetScale(float scale);                 /* 设置尺度因子。 */

void Odometry_Init(void);                           /* 初始化编码器和里程。 */
void Odometry_Update(uint8_t ticks);                /* 更新双轮路程与速度。 */
void Odometry_Reset(void);                          /* 清零当前分段里程。 */
float Odometry_GetDistanceMM(void);                 /* 返回左右平均路程。 */
float Odometry_GetDistanceLMM(void);                /* 返回左轮路程。 */
float Odometry_GetDistanceRMM(void);                /* 返回右轮路程。 */
float Odometry_GetSpeedL(void);                     /* 返回左轮速度。 */
float Odometry_GetSpeedR(void);                     /* 返回右轮速度。 */
```

| 头文件 | 公共参数 | 当前值 | 作用 |
|---|---|---:|---|
| `Gimbal.h` | `GIMBAL_X_MOTOR_ADDRESS`、`GIMBAL_Y_MOTOR_ADDRESS` | `1/2` | X/Y 电机地址 |
| `Gimbal.h` | `GIMBAL_DEFAULT_SPEED_RPM` | `100` | 默认 T 型规划速度 |
| `Gimbal.h` | `GIMBAL_DEFAULT_ACCELERATION_RPMS2` | `100U` | 默认加速度 |
| `Gimbal.h` | `GIMBAL_FEEDBACK_REQUEST_PERIOD_SECONDS` | `0.03f` | 地址轮询周期 |
| `Gimbal.h` | `GIMBAL_FEEDBACK_TIMEOUT_SECONDS` | `1.0f` | 反馈超时 |
| `Gimbal.h` | `GIMBAL_POSITION_TOLERANCE_DEG` | `2.0f` | 到位角度容差 |
| `Gimbal.h` | `GIMBAL_POSITION_SETTLE_FEEDBACKS` | `2U` | 连续到位反馈次数 |
| `Servo.h` | `SERVO_PHYSICAL_RANGE_DEG` | `270U` | 电气角度量程 |
| `Servo.h` | `SERVO_MIN_PULSE_US`、`SERVO_MAX_PULSE_US` | `500/2500 us` | 角度脉宽范围 |
| `Servo.h` | `SERVO_FRAME_US` | `20000U` | 50 Hz 帧周期 |
| `Servo.h` | `SERVO_VERTICAL_MIN_ANGLE`、`SERVO_VERTICAL_MAX_ANGLE`、`SERVO_VERTICAL_DEFAULT_ANGLE` | `0/270/135°` | 纵向限位与默认角 |
| `Servo.h` | `SERVO_HORIZONTAL_MIN_ANGLE`、`SERVO_HORIZONTAL_MAX_ANGLE`、`SERVO_HORIZONTAL_DEFAULT_ANGLE` | `0/270/135°` | 横向限位与默认角 |
| `Heading.h` | `HEADING_CALIBRATION_SAMPLES` | `400U` | 开机零漂采样数 |
| `Heading.h` | `HEADING_CALIBRATION_INTERVAL_MS` | `2U` | 零漂采样间隔 |
| `Odometry.h` | `Odometry_CountsPerMM` | `6.23f` | 每毫米编码器计数，需实车标定 |

## 6. Hardware 与 System 公共接口和参数

### 6.1 板级、通信与传感器

```c
void LED_Init(void);               /* 初始化 LED 输出。 */
void LED1_ON(void);                /* 点亮 LED1。 */
void LED1_OFF(void);               /* 熄灭 LED1。 */
void LED1_Turn(void);              /* 翻转 LED1。 */
void LED2_ON(void);                /* 点亮 LED2。 */
void LED2_OFF(void);               /* 熄灭 LED2。 */
void LED2_Turn(void);              /* 翻转 LED2。 */
void LED_RGB_ON(void);             /* 同时点亮两个 LED。 */
void LED_RGB_OFF(void);            /* 同时熄灭两个 LED。 */

void Beep_Init(void);              /* 初始化蜂鸣器并关闭输出。 */
void Beep_On(void);                /* 打开蜂鸣器。 */
void Beep_Off(void);               /* 关闭蜂鸣器。 */
void Beep_Notify(uint8_t times);   /* 非阻塞短鸣指定次数。 */
void Beep_Long(void);              /* 触发长鸣。 */
void Beep_Tick(void);              /* 100 Hz 推进鸣叫状态。 */

void Key_Init(void);               /* 初始化四个低有效按键。 */
uint8_t Key_GetPressedMask(void);  /* 返回 bit0~bit3 按下位图。 */
uint8_t Key_GetNum(void);          /* 返回单个按键编号，无按键为 0。 */

void Serial1_Init(void);                           /* 初始化 UART1 蓝牙缓冲区。 */
uint32_t Serial1_Available(void);                  /* 返回 UART1 待读字节数。 */
uint8_t Serial1_ReadByte(uint8_t *byte);           /* 非阻塞读取 UART1 字节。 */
uint8_t Serial1_QueueArray(const uint8_t *array,
                           uint16_t length);       /* 非阻塞加入 UART1 TX 队列。 */
void Serial2_Init(void);                           /* 初始化 UART2 F32C 缓冲区。 */
uint32_t Serial2_Available(void);                  /* 返回 UART2 待读字节数。 */
uint8_t Serial2_ReadByte(uint8_t *byte);           /* 非阻塞读取 UART2 字节。 */
void Serial2_SendByte(uint8_t byte);               /* UART2 发送单字节。 */
void Serial2_SendArray(const uint8_t *array, uint16_t length); /* UART2 发送数组。 */

void Graydetect_Init(void);                        /* 初始化五路上拉输入。 */
uint8_t Graydetect_GetState(void);                 /* 返回五路位图，黑线为 1。 */
uint8_t Graydetect_GetBit(uint8_t index);          /* 返回 0~4 指定通道。 */
float Graydetect_GetError(uint8_t side);           /* 返回指定区域加权误差。 */
uint8_t Graydetect_OnLine(uint8_t side);           /* 返回指定区域是否压线。 */

void MPU6050_Init(void);                           /* 初始化软件 I2C 和 MPU6050。 */
uint8_t MPU6050_IsReady(void);                     /* 返回设备是否在线。 */
uint8_t MPU6050_GetID(void);                       /* 读取 WHO_AM_I。 */
void MPU6050_GetData(int16_t *ax, int16_t *ay, int16_t *az,
                     int16_t *gx, int16_t *gy, int16_t *gz); /* 读取六轴原始值。 */
int16_t MPU6050_GetGyroZ(void);                    /* 读取 Z 轴陀螺原始值。 */
```

| 头文件 | 公共参数 | 当前值 | 作用 |
|---|---|---:|---|
| `Serial.h` | `SERIAL1_RX_BUFFER_SIZE` | `1024U` | UART1 RX 环形缓冲区 |
| `Serial.h` | `SERIAL1_TX_BUFFER_SIZE` | `256U` | UART1 TX 环形缓冲区 |
| `Serial.h` | `SERIAL2_RX_BUFFER_SIZE` | `256U` | UART2 RX 环形缓冲区 |
| `Serial.h` | `Serial1_RxFlag` | 公共状态 | UART1 收到数据后置位 |
| `Graydetect.h` | `GRAY_SIDE_ALL`、`GRAY_SIDE_LEFT`、`GRAY_SIDE_RIGHT` | `0/1/2` | 灰度区域选择 |

### 6.2 电机、编码器与 F32C

```c
void PWM_Init(void);                         /* 启动 TIMG8 双通道 PWM。 */
void PWM_SetCompareA(uint16_t Compare);       /* 设置右电机 A 通道，范围 0~1000。 */
void PWM_SetCompareB(uint16_t Compare);       /* 设置左电机 B 通道，范围 0~1000。 */

void Motor_Init(void);                       /* 初始化 PWM 并释放双轮。 */
void Motor_SetLeftPWM(int16_t PWM);           /* 设置物理左轮有符号 PWM。 */
void Motor_SetRightPWM(int16_t PWM);          /* 设置物理右轮有符号 PWM。 */
void Motor_SetPWM(int16_t LeftPWM, int16_t RightPWM); /* 同时设置双轮 PWM。 */
void Motor_StopAll(void);                    /* PWM 归零并释放双轮。 */
void Motor_Brake(void);                      /* TB6612 短路制动，快速刹车。 */
void Motor_Run(int16_t leftSpeed, int16_t rightSpeed); /* 双轮开环运行。 */
void Motor_Forward(int16_t speed);            /* 双轮同速前进。 */
void Motor_Backward(int16_t speed);           /* 双轮同速后退。 */
void Motor_TurnLeft(int16_t speed);           /* 单侧开环左转。 */
void Motor_TurnRight(int16_t speed);          /* 单侧开环右转。 */
void Motor_SpinLeft(int16_t speed);           /* 双轮反向原地左转。 */
void Motor_SpinRight(int16_t speed);          /* 双轮反向原地右转。 */
void Motor_Stop(void);                       /* `Motor_StopAll()` 的兼容接口。 */

void Encoder_Init(void);                     /* 初始化双轮正交中断计数。 */
int16_t Encoder_Get(uint8_t n);               /* 读取并清零左轮 n=1 或右轮 n=2 增量。 */

void F32C_Init(void);                         /* 初始化 UART2 解析状态。 */
F32C_Result_t F32C_Enable(uint8_t address);   /* 使能指定地址。 */
F32C_Result_t F32C_Disable(uint8_t address);  /* 失能指定地址。 */
F32C_Result_t F32C_SetMode(uint8_t address, F32C_Mode_t mode); /* 设置控制模式。 */
F32C_Result_t F32C_SetSpeedRPM(uint8_t address, int16_t speedRPM); /* 设置规划速度。 */
F32C_Result_t F32C_SetAcceleration(
    uint8_t address, uint16_t accelerationRPMS2); /* 设置规划加速度。 */
F32C_Result_t F32C_ClearMultiTurnAngle(uint8_t address); /* 将当前位置记为多圈零点。 */
F32C_Result_t F32C_SetMultiTurnPositionDegrees(
    uint8_t address, float targetAngleDeg);   /* 设置多圈绝对角目标。 */
F32C_Result_t F32C_RequestFeedback(
    uint8_t address, F32C_FeedbackType_t type); /* 请求指定反馈。 */
uint8_t F32C_PopFeedback(F32C_Feedback_t *feedback); /* 解析一帧有效反馈。 */
```

| 头文件 | 公共参数 | 当前值 | 作用 |
|---|---|---:|---|
| `PWM.h` | `PWM_MAX_DUTY` | `1000U` | 有刷电机 PWM 软件满量程 |
| `F32C.h` | `F32C_MIN_ADDRESS` | `1U` | 最小有效电机地址 |
| `F32C.h` | `F32C_MAX_SPEED_RPM` | `1000` | 协议速度上限 |
| `F32C.h` | `F32C_MAX_MULTI_TURN_ANGLE_DEG` | `214748300.0f` | 多圈角度软件上限 |
| `F32C.h` | `F32C_COMMAND_INTERVAL_MS` | `1U` | 相邻 F32C 指令最小间隔 |

### 6.3 OLED 与 System

```c
void OLED_Init(void);                              /* 初始化 OLED 和显存。 */
uint8_t OLED_IsReady(void);                        /* 返回 I2C 初始化结果。 */
void OLED_Update(void);                            /* 刷新全部显存。 */
void OLED_UpdateArea(int16_t x, int16_t y, uint8_t width, uint8_t height); /* 刷新区域。 */
void OLED_Clear(void);                             /* 清空显存。 */
void OLED_ClearArea(int16_t x, int16_t y, uint8_t width, uint8_t height); /* 清空区域。 */
void OLED_Reverse(void);                           /* 反色全部显存。 */
void OLED_ReverseArea(int16_t x, int16_t y, uint8_t width, uint8_t height); /* 反色区域。 */
void OLED_ShowChar(int16_t x, int16_t y, char value, uint8_t fontSize); /* 显示字符。 */
void OLED_ShowString(int16_t x, int16_t y, const char *string, uint8_t fontSize); /* 显示字符串。 */
void OLED_ShowNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize); /* 显示无符号数。 */
void OLED_ShowSignedNum(int16_t x, int16_t y, int32_t number, uint8_t length, uint8_t fontSize); /* 显示有符号数。 */
void OLED_ShowHexNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize); /* 显示十六进制数。 */
void OLED_ShowBinNum(int16_t x, int16_t y, uint32_t number, uint8_t length, uint8_t fontSize); /* 显示二进制数。 */
void OLED_ShowFloatNum(int16_t x, int16_t y, double number,
                       uint8_t intLength, uint8_t fraLength,
                       uint8_t fontSize);            /* 显示浮点数。 */
void OLED_ShowImage(int16_t x, int16_t y, uint8_t width,
                    uint8_t height, const uint8_t *image); /* 显示位图。 */
void OLED_Printf(int16_t x, int16_t y, uint8_t fontSize,
                 const char *format, ...);          /* 格式化显示。 */
void OLED_DrawPoint(int16_t x, int16_t y);          /* 绘制点。 */
uint8_t OLED_GetPoint(int16_t x, int16_t y);        /* 读取显存像素。 */
void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1); /* 绘制线段。 */
void OLED_DrawRectangle(int16_t x, int16_t y, uint8_t width,
                        uint8_t height, uint8_t filled); /* 绘制矩形。 */
void OLED_DrawTriangle(int16_t x0, int16_t y0, int16_t x1,
                       int16_t y1, int16_t x2, int16_t y2,
                       uint8_t filled);              /* 绘制三角形。 */
void OLED_DrawCircle(int16_t x, int16_t y, uint8_t radius,
                     uint8_t filled);                /* 绘制圆。 */
void OLED_DrawEllipse(int16_t x, int16_t y, uint8_t a,
                      uint8_t b, uint8_t filled);    /* 绘制椭圆。 */
void OLED_DrawArc(int16_t x, int16_t y, uint8_t radius,
                  int16_t startAngle, int16_t endAngle,
                  uint8_t filled);                   /* 绘制圆弧。 */

void Delay_us(uint32_t us);                         /* 阻塞微秒延时。 */
void Delay_ms(uint32_t ms);                         /* 阻塞毫秒延时。 */
void Delay_s(uint32_t s);                           /* 阻塞秒延时。 */
void Tick_Init(void);                               /* 初始化累计 Tick。 */
uint8_t Tick_Poll(void);                            /* 有 Tick 时消费一个并返回 1。 */
uint8_t Tick_PollCount(void);                       /* 取出并清空累计 Tick 数。 */
void Interrupt_Enable(void);                        /* 统一开启全局中断。 */
void Interrupt_Disable(void);                       /* 统一关闭全局中断。 */
```

| 头文件 | 公共参数 / 数据 | 当前值 | 作用 |
|---|---|---:|---|
| `OLED.h` | `OLED_8X16/OLED_6X8` | `8U/6U` | 字体选择 |
| `OLED.h` | `OLED_UNFILLED/OLED_FILLED` | `0U/1U` | 图形填充选择 |
| `OLED_Data.h` | `OLED_CHARSET_UTF8` | 已启用 | 中文字模索引编码 |
| `OLED_Data.h` | `OLED_F8x16/OLED_F6x8/OLED_CF16x16/Diode` | 公共只读数据 | ASCII、中文和图像数据 |
| `Tick.h` | `TICK_HZ` | `100U` | 系统节拍频率 |
| `Tick.h` | `TICK_DT` | `0.01f` | 单节拍秒数 |

## 7. Mission 与 Accomplish 公共接口和参数

### 7.1 Mission 状态图接口

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
uint8_t Mission_IsAtStartState(void);               /* 返回当前是否处于状态图起始等待状态。 */
```

转换数组从前到后就是优先级。动作运行时只检查打断转换，动作完成后只检查正常转换；每个系统节拍最多转换一次。蓝牙控制不再拥有独立调试入口，只在 `Mission_IsAtStartState()` 为 1 时允许执行摇杆、定距直行或相对转向命令。

### 7.2 Accomplish 状态图入口

```c
const Mission_GraphDefinition_t *Accomplish25E_GetMissionGraph(void); /* 返回 25E 静态状态图。 */
const Mission_GraphDefinition_t *Accomplish25H_GetMissionGraph(void); /* 返回当前 25H 静态状态图。 */
const Mission_GraphDefinition_t *BrushlessMotorTest_GetMissionGraph(void); /* 返回 F32C 测试状态图。 */
```

| 头文件 | 公共参数 | 当前值 | 作用 |
|---|---|---:|---|
| `25E.h` | `ACCOMPLISH_25E_START_KEY_MASK` | `0x01U` | KEY1 启动掩码 |
| `25E.h` | `ACCOMPLISH_25E_STRAIGHT_DISTANCE_MM` | `2000U` | 单轮直线最大距离 |
| `25E.h` | `ACCOMPLISH_25E_STRAIGHT_SPEED_MMPS` | `300.0f` | 直线巡航速度 |
| `25E.h` | `ACCOMPLISH_25E_STRAIGHT_END_SPEED_MMPS` | `0.0f` | 走满距离后的终点速度 |
| `25E.h` | `ACCOMPLISH_25E_LINE_SPEED_MMPS` | `200.0f` | 巡线速度 |
| `25E.h` | `ACCOMPLISH_25E_LINE_DETECT_CONFIRM_TICKS` | `3U` | 连续检测到线 30 ms 后切换 |
| `25E.h` | `ACCOMPLISH_25E_TURN_TARGET_OFFSET_DEG` | `180.0f` | 每轮连续绝对角增量 |
| `25E.h` | `ACCOMPLISH_25E_TURN_SPEED_MMPS` | `80.0f` | 转向轮速 |
| `25H.h` | `ACCOMPLISH_25H_START_KEY_MASK` | `0x01U` | KEY1 启动掩码 |
| `25H.h` | `ACCOMPLISH_25H_LEFT_MARKER_MASK` | `0x03U` | bit0、bit1 同时为黑线的标志条件 |
| `25H.h` | `ACCOMPLISH_25H_LINE_SPEED_MMPS` | `200.0f` | 巡线速度 |
| `25H.h` | `ACCOMPLISH_25H_FORWARD_DISTANCE_MM` | `150U` | 标志后前进距离 |
| `25H.h` | `ACCOMPLISH_25H_FORWARD_SPEED_MMPS` | `200.0f` | 标志后直线速度 |
| `25H.h` | `ACCOMPLISH_25H_FORWARD_END_SPEED_MMPS` | `0.0f` | 标志后直线终点速度 |
| `25H.h` | `ACCOMPLISH_25H_TURN_STEP_DEG` | `-90.0f` | 每轮连续绝对左转目标增量 |
| `25H.h` | `ACCOMPLISH_25H_TURN_SPEED_MMPS` | `80.0f` | 转向轮速 |
| `Brushless_Motor_Test.h` | `BRUSHLESS_MOTOR_TEST_STOP_KEY_MASK` | `0x02U` | KEY2 停止测试 |
| `Brushless_Motor_Test.h` | `BRUSHLESS_MOTOR_TEST_X_RIGHT_SIGN` | `1.0f` | X 轴右转方向符号 |
| `Brushless_Motor_Test.h` | `BRUSHLESS_MOTOR_TEST_X_STEP_DEG` | `180.0f` | X 轴每轮多圈角增量 |
| `Brushless_Motor_Test.h` | `BRUSHLESS_MOTOR_TEST_Y_LOW_DEG`、`BRUSHLESS_MOTOR_TEST_Y_HIGH_DEG` | `0/180°` | Y 轴往返目标 |
