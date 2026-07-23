# MSPM0G3507 天猛星小车工程索引

> 对项目进行任何修改时，只要涉及硬件连接、Pin 口、外设实例、时钟树、波特率、定时器、主循环调用关系或程序功能，就必须同时修改本文的 Pin 口表、时钟表和对应程序说明。禁止只修改代码而不更新本文档。

本文只记录工程文件类型、文件职责、公共函数和公共参数。函数声明以当前头文件为准；`static` 函数和变量属于文件内部实现，不作为跨模块接口记录。`main.syscfg` 是 PinMux、时钟和外设实例的唯一配置源，`Debug/`、`Release/` 和 `ti_msp_dl_config.*` 均为生成内容。

## 1. 时钟与外设占用

| 资源 | 当前配置 | 占用模块 | 作用 |
|---|---:|---|---|
| CPUCLK / SYSCLK | 32 MHz | 全工程 | SysConfig 默认时钟源；延时、总线外设和 SysTick 的基准时钟 |
| SysTick | 32 MHz / 320000 = 100 Hz | `System/Tick`、`Application/Core/App`、`MotionManager`、`Mission`、`Accomplish/25H` | 10 ms 系统节拍；`App_Update()` 传递实际累计 Tick 和 dt；当前巡线连续全白 50 拍（约 500 ms）后安全返回等待 |
| TIMG8 | 32 MHz，周期 1600 = 20 kHz | `Hardware/Motor/PWM`、`MotionWheel`、`MotionManager`、`Accomplish/25H` | 25H 通过 MotionManager 依次选择巡线、定距直线和绝对角转向，再由 MotionWheel 输出双轮 PWM |
| TIMA0 | BUSCLK / 32 = 1 MHz，周期 20000 = 50 Hz | `Application/Servo` | 双路舵机 PWM；1 个计数等于 1 us |
| I2C0 | BUSCLK 32 MHz，SCL 400 kHz | `Hardware/Display/OLED` | OLED 控制器通信 |
| UART1 | BUSCLK 32 MHz，115200 baud，8N1，RX 中断 | `Hardware/Comms/Serial`、`Application/Comms/BluetoothDebug`、`App`、`Mission` | 无线 DAPLink 串口，接电脑调试网页；`C0` 全局停车；保留其他任务事件和手动调试命令，当前 25H 使用 KEY1 启动 |
| UART2 | BUSCLK 32 MHz，115200 baud，8N1，当前不启用 RX NVIC | `Hardware/Comms/Serial`、`Application/Comms/K230Link` | K230 硬件配置保留；`App_Init()` 调用 `K230Link_Init()`，`App_Update()` 调用 `K230Link_Update()`，握手帧正常收发 |
| GPIO 软件 I2C | CPU 延时产生时序 | `Hardware/Sensors/MPU6050`、`Application/State/Heading`、`Accomplish/25H` | 提供连续多圈航向；25H 保存 KEY1 启动基准，左转绝对目标依次为基准减 90°、180°、270°……；不占用 I2C 外设实例 |
| GPIOA GROUP1 IRQ | A/B 相双边沿 | `Hardware/Motor/Encoder`、`MotionWheel` | 左右编码器软件正交解码，为公共双轮速度 PI 提供速度反馈 |

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
| PA21 | UART2 TX | K230 RX | K230Link 握手已启用；连接 K230 GPIO4（RX） |
| PA22 | UART2 RX | K230 TX | K230Link 握手已启用；连接 K230 GPIO3（TX） |
| PA24 | GPIO 输入、上拉、双边沿中断 | 左编码器 B | GPIOA GROUP1 IRQ；`MotionWheel` 左轮反馈 |
| PA28 | I2C0 SDA | OLED | 400 kHz |
| PA30 | GPIO 输入、上拉 | KEY1 | 低电平按下，按键位图 bit0；App 生成按下沿事件，当前用于启动 25H |
| PA31 | I2C0 SCL | OLED | 400 kHz |
| PB0 | GPIO 输出 | 左电机 BIN1 | TB6612 B 通道方向；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB1 | GPIO 输出 | 左电机 BIN2 | TB6612 B 通道方向；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB6 | UART1 TX | 无线 DAPLink 串口 | MCU 发送到 DAPLink RX；115200 |
| PB7 | UART1 RX、上拉 | 无线 DAPLink 串口 | DAPLink TX 发送到 MCU，RX 中断接收；115200；`C0` 全局停车，`C1` 当前未绑定 25H |
| PB8 | TIMA0 CCP0 | 横向舵机 | `D` 命令，`Servo_SetHorizontalAngle()` |
| PB9 | TIMA0 CCP1 | 纵向舵机 | `O` 命令，`Servo_SetVerticalAngle()` |
| PB10 | GPIO 输入、上拉 | KEY4 | 低电平按下，按键位图 bit3；App 生成按下沿事件，具体含义由 Mission 决定 |
| PB11 | GPIO 输入、上拉 | KEY2 | 低电平按下，按键位图 bit1；App 生成按下沿事件，具体含义由 Mission 决定 |
| PB14 | GPIO 输入、上拉 | KEY3 | 低电平按下，按键位图 bit2；App 生成按下沿事件，具体含义由 Mission 决定 |
| PB15 | TIMG8 CCP0 | 右电机 PWM | TB6612 A 通道，20 kHz；由 MotionManager 当前模式经 MotionWheel 输出 |
| PB16 | TIMG8 CCP1 | 左电机 PWM | TB6612 B 通道，20 kHz；由 MotionManager 当前模式经 MotionWheel 输出 |
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

K230 握手已启用：`App_Init()` 调用 `K230Link_Init()`，`App_Update()` 每拍调用 `K230Link_Update()`，握手帧正常收发。K230Link 源码和 UART2 SysConfig 配置均保留并生效。

### 3.2 100 Hz 主循环与 Mission

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
            Mission_Update(&updateContext);
        }
    }
}
```

`App_Init()` 在全局中断关闭状态下初始化整车、OLED、MPU6050、编码器、蓝牙和 F32C 通信。`Gimbal_Init()` 只把 UART2/F32C 初始化为禁用状态，不使能或驱动无刷电机。MPU6050 零漂完成后，`Interrupt_Enable()` 才开启全局中断。

```text
Heading -> Odometry -> 按键边沿 -> BluetoothDebug
        -> C0 全局停车 -> MotionManager -> Beep -> OLED
        -> Mission_Update
```

Mission 使用静态状态图，不使用 `malloc`。每个状态含 `onEnter/onUpdate/onExit`、有序转换表和 `interruptible`。动作运行时只检查打断转换；动作完成后只检查正常转换；每拍最多转换一次。被打断状态调用退出回调并停车，不保存或恢复原进度。

当前加载 `Accomplish/25H.c` 的静态状态图。KEY1 按下沿启动 200 mm/s 巡线，并保存 MPU6050 连续航向作为 0°基准；最左侧 bit0 与左内侧 bit1 同时为 1 时立即打断巡线，向前直行 150 mm 并减速至零。零速目标固定保持后，直接调用 `MotionManager_TurnTo()` 指向下一绝对左转目标。目标依次为启动航向减 90°、180°、270°……；到角后继续巡线并循环。巡线连续全白 50 拍时停车并返回等待；`C0` 始终能够停车、返回等待并清除航向基准。

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

K230Link 统一帧格式：

```text
Heading -> Odometry -> Gimbal -> Key -> BluetoothDebug
        -> C0 全局停止 -> MotionManager -> Beep -> OLED -> Mission
```

- `VER=0x01`；`TYPE` 为 `READY=0x01`、`READY_ACK=0x02`、`TARGET=0x10`、`CAPTURE=0x20`、`CAPTURE_ACK=0x21`。
- `SEQ` 为 8 位帧序号，`LEN` 最大为 32。
- CRC 使用 CRC-8/ATM，多项式 `0x07`、初始值 0，校验范围为 `VER` 到 `PAYLOAD`。
- TARGET 的 PAYLOAD 固定为 `valid:u8 + offsetX:int16_LE + offsetY:int16_LE`，共 5 字节。
- CAPTURE（MCU → K230）的 PAYLOAD 为 `count:u8`，共 1 字节；`count` 为连拍张数，范围 `1~20`。
- CAPTURE_ACK（K230 → MCU）的 PAYLOAD 为 `ok:u8 + index:uint16_LE`，共 3 字节；`ok=1` 表示成功，`index` 为存入 TF 卡的起始文件序号。
- K230 `uart_io.py` 测试入口可在握手后持续发送 `valid=1、offsetX=123、offsetY=-45`。

### 3.3 调试串口任务与命令协议

本节的命令走 UART1，物理链路是**无线 DAPLink 串口**（115200 8N1），不是蓝牙。源码模块名 `BluetoothDebug` 与本文其余处的「蓝牙」措辞是早期接蓝牙模块时留下的历史名称，为避免大范围改名引入风险而保留；凡提到「蓝牙命令」处，均指本节这套 UART1 命令协议。

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
| `G<number>` | 设置遥测 CSV 输出频率；`G0` 关闭输出 | `0~100` Hz（硬上限）；实际安全上限由当前字段掩码动态决定，超限返回 `ERR RANGE MAX=<当前安全上限>` | `G20` |
| `M<number>` | 设置遥测**通道掩码**（12 位，见 3.3.2），改变时发一帧二进制 SCHEMA | `1~4095`（`0x0FFF`），`0` 或超限返回 `ERR RANGE`；成功回报新频率 `OK M=<mask> G=<hz>` | `M6`（TR+RV 略） |
| `V<number>` | 设置调试巡航速度 | `20~800` mm/s，默认 200 | `V200` |
| `F<number>` | 前进定距，终点速度 0 | `1~10000` mm，忙时 `ERR BUSY` | `F300` |
| `B<number>` | 后退定距，终点速度 0 | `1~10000` mm，忙时 `ERR BUSY` | `B300` |
| `T<number>` | 相对转角，带符号 | `-3600~3600` 度，忙时 `ERR BUSY` | `T90` |
| `A<number>` | 绝对连续航向角 | `-3600~3600` 度，忙时 `ERR BUSY` | `A90` |
| `Z<number>` | `Z1` 只把当前朝向定为 0° 基准；`Z2` 静止重采 MPU6050 Z 轴零漂并把航向归零 | 仅接受 1/2，忙时 `ERR BUSY`；`Z2` 需保持车辆静止约 0.8 秒，离线返回 `ERR Z OFFLINE`，正在 E/Y 尺度标定时返回 `ERR Z CALIBRATING` | `Z2` |
| `P<number>` | 请求 K230 连拍并存 TF 卡 | `1~20`，链路未就绪返回 `ERR CAP NOLINK` | `P1` |
| `W<number>` | 闭环恒速模式：双轮同目标速度、无规划斜坡、无航向修正，是轮速 PI 的标准阶跃激励；运动中重复发 `W` 直接改目标（不复位 PID，可链式阶跃）；`W0` 停止并释放电机 | `-800~800` mm/s；其他模式忙时 `ERR BUSY`，`W0` 只停恒速模式 | `W300` |
| `N<number>` | 直接启动巡线（不经 Mission 状态图）；`N0` 停止 | `20~800` mm/s；其他模式忙时 `ERR BUSY`，`N0` 只停巡线模式；丢线后自动完成 | `N200` |
| `K?` / `K<id>?` / `K<id>=<float>` | 运行时读写控制参数（见 3.3.1 参数表）：列表 / 读单个 / 写入（支持小数与负号，写入立即生效） | id `1~26`；越界返回 `ERR K RANGE MIN=<min> MAX=<max>`，格式错误返回 `ERR K FORMAT` | `K17=1.5` |
| `E<number>` | 陀螺仪尺度标定：`E1` 开始（清零标定角），`E0` 取消 | 仅 `0/1`；运动中 `ERR BUSY`，MPU 离线 `ERR CAL OFFLINE` | `E1` |
| `Y<number>` | 原地转 n 整圈回到起始朝向后结束标定，解算并应用尺度因子 | `1~20` 圈；未在标定中返回 `ERR CAL IDLE`，积分角过小返回 `ERR CAL SMALL`；成功回 `OK Y SCALE=<新因子> RAW=<积分角>` | `Y3` |
| `X<mask>` | **板载捕获**：按通道掩码 arm，下一条运动命令自动触发，以 100 Hz 无损写入 RAM（期间不发串口） | 掩码见 3.3.2；通道数超 8 或掩码非法返回 `ERR X MASK MAXCH=8`；成功回 `OK X ARM=<mask> CAP=<最大样本数>` | `X7` |
| `X0` | 停止捕获并分批 dump 全部数据 | 无数据返回 `ERR X EMPTY`；成功先回 `OK X DUMP=<n>`，随后输出 `CH,` 表头与 `C,` 数据行，末尾 `OK X END=<n>` | `X0` |
| `Q` | 查询遥测能力与捕获状态；**上位机据此自适应频率，不再各存一份阈值** | 无参数（裸 `Q` 即可）；回 `OK Q MAX=<上限Hz> MASK=<掩码> RATE=<当前Hz> CAPST=<捕获状态> CAPN=<已采样本> CAPMAX=<容量>` | `Q` |

**`ERR MOTION 3` 的含义。** MotionManager 错误码 `3` 是 `SENSOR_NOT_READY`。若 `B/F/T/A` 都返回 `ERR MOTION 3`，同时 `Z2` 返回 `ERR Z OFFLINE`，说明动作命令没有真正启动，根因是 Heading 层未就绪，通常是 MPU6050 离线或初始化失败。此时 `yaw/navE` 遥测会作为无效字段发送，网页显示为空值，不再把默认 `0°` 当成有效航向；优先检查 MPU6050 供电、GND、PA10/PA11 软件 I2C 线序、上拉/开漏连接和地址 `0x68`。

**`T` 与 `A` 的参考系差异。** `T` 是相对转角，从当前航向再偏转指定度数，支持带符号输入（`T-90` 反转 90°）。`A` 是绝对连续航向角，目标为 `Heading_GetYaw()` 的累计值空间中的绝对角度。两者底层均使用 `Nav`，**`Nav` 不做 ±180° 最短路径优化**：当前连续航向为 370° 时，`A90` 会计算误差 `90 - 370 = -280°`，选择倒转 280° 而不是顺转 80° 走最短路径。若想总是最短路径到达某个方向，应在上位机计算出当前航向与目标方向之间的最短差值后，改用 `T` 命令发送相对角。`Z1` 只把当前朝向重置为 0°，不会更新零漂；车辆静止时可用 `Z2` 重新采样零漂并归零，再用 `A` 命令指向绝对角度。

`L/R/U` 只在 MotionManager 空闲时执行，自动运动期间返回 `ERR BUSY`。数字仍是开环 PWM，不是 mm/s；OLED 上的 `LV/RV` 是编码器实测速度。

**`G` 命令的上限是动态的。** `Serial1_SendByte()` 是阻塞发送，发送期间主循环完全停止。字段越多行越长，允许的最高频率越低：全字段（掩码 1023）时安全上限约 14 Hz，轮速调参子集（掩码 216 = speed+mode+target+pwm）约 30 Hz，只开 YAW（掩码 1）时可达 92 Hz。发送 `G<n>` 超过当前安全上限时，返回 `ERR RANGE MAX=<上限>` 而不是固定文本，用户可据此调整频率或先用 `M` 精简字段再提速。调参时应按网页「调参试验」的预设只开相关字段换高频。

### 3.3.0 二进制 DMA 遥测架构

> **本节描述当前架构。** 遥测数据已从 ASCII CSV 改为二进制帧 + DMA 非阻塞发送。
> 设计动机与协议规范见 `docs/superpowers/specs/2026-07-21-二进制DMA架构-design.md`；
> 下文 3.3.2 及"遥测 CSV 格式"等 ASCII 描述是**历史架构**，已被本节取代。

**核心变化。** UART1 的发送从逐字节阻塞（`transmitDataBlocking`）改为 **DMA 搬运 + 环形缓冲**：主循环只往缓冲写、从不等硬件，DMA 完成中断推进下一段。发送不再占用主循环，遥测带宽预算从 20% 放宽到 70%。数据从 ASCII CSV（一个 float 占 11 字节）改为**二进制帧**（float32 只占 4 字节，压缩 2.5×）。两者叠加，单侧实时流上限从阻塞 ASCII 的 57 Hz 提到约 **450 Hz** 理论值，实际夹在 100 Hz 硬上限——**远超 100 Hz 控制环，实时流本身即无损**。

**帧格式**（与 K230Link 同范式，`0xAA` 起头与 ASCII 命令回应可靠共存于同一串口）：

```text
0xAA 0x55 | VER | TYPE | SEQ | LEN | PAYLOAD | CRC8
```

- `0xAA` 是非 ASCII 可打印字符，绝不出现在文本回应里；网页见 `0xAA` 进二进制解析，否则累积成 ASCII 文本行。
- `CRC8` 为 CRC-8/ATM（多项式 `0x07`，初值 0），覆盖 `VER` 到 `PAYLOAD`。
- 帧类型：`SCHEMA=0x30`（掩码变化时先发，含列名+单位码）、`SAMPLE=0x31`（ms + 各通道 float32）、`CAP_META=0x32`/`CAP_SAMPLE=0x33`/`CAP_END=0x34`（板载捕获 dump）。

**命令仍走 ASCII。** `K/W/N/M/G/F/B/T/A/Z/X/Q/E/Y/P/C/L/R/U/O/D` 及其 `OK`/`ERR` 回应保持文本——低频、要人读、任何串口助手可直接调试。只有高频遥测数据二进制化。

**双通道仍在，但定位变了：**

| | 通道 A：实时流 | 通道 B：板载捕获 |
|---|---|---|
| 命令 | `M` 掩码 + `G` 频率 | `X<mask>` arm / `X0` dump |
| 帧类型 | `SCHEMA` + `SAMPLE` | `CAP_META` + `CAP_SAMPLE` + `CAP_END` |
| 频率 | 可达 100 Hz（DMA 后与控制环同频） | 固定 100 Hz |
| 传输时机 | 边跑边发（DMA 不阻塞） | 记录期间不发，动作结束后 dump |
| 容量 | 无限（持续流） | 24 KB RAM，可变长度 |
| 用途 | 监视、遥控采数据、**调参**（已够用） | 超长录制、绝对零丢包的瞬态 |

**为什么捕获降级。** 第一次架构（ASCII 双通道）引入板载捕获，唯一理由是"实时流 30 Hz 低于控制环 100 Hz"。二进制 + DMA 后实时流本身就是 100 Hz 无损，捕获不再是调参必需，而是两种补充场景：① 想录得比串口实时率更久（**单通道 8 字节/样本，24 KB 可录约 30 秒**）；② 想要绝对零丢包（防 Web Serial 偶发丢包）。这与 ODrive 板载 oscilloscope、Betaflight Blackbox 同源。

### 3.3.2 通道掩码（M 与 X 命令共用）

实时流与捕获**共用同一套 12 通道位定义**（`TELEMETRY_CH_*`）。`M<mask>` 设实时流通道，`X<mask>` arm 捕获通道。捕获最多 **8 通道**（`CAPTURE_MAX_CHANNELS`），超出返回 `ERR X MASK MAXCH=8`。

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
| `0x200` | `gray` | 位图 | 五路灰度 |
| `0x400` | `LD` | mm | 左轮累计路程 |
| `0x800` | `RD` | mm | 右轮累计路程 |

`TELEMETRY_CH_ALL = 0x0FFF`。位序即帧列序，一经发布不得重排。二进制 SAMPLE 帧字节数 = 7（帧开销）+ 4（ms）+ 通道数×4；全 12 通道 59 字节，100 Hz 时仅 5.9 KB/s，占 115200 波特带宽的一半。网页调参试验按阶段自动选最优通道并用 `Q` 命令问出真实上限。

| 位 | 通道 | 内容 |
|---:|---|---|
| `0x001` | `TL` | 左轮目标速度 mm/s |
| `0x002` | `LV` | 左轮实测速度 mm/s |
| `0x004` | `PL` | 左轮输出 PWM |
| `0x008` | `TR` | 右轮目标速度 mm/s |
| `0x010` | `RV` | 右轮实测速度 mm/s |
| `0x020` | `PR` | 右轮输出 PWM |
| `0x040` | `yaw` | 连续累计航向角（度）；Heading 离线时无效/空值 |
| `0x080` | `navE` | 转向角误差（度）；Heading 离线时无效/空值 |
| `0x100` | `lerr` | 巡线权重误差 |

常用组合：调左轮 `X7`（TL+LV+PL）、调右轮 `X56`（TR+RV+PR）、双轮对比 `X27`（TL+LV+TR+RV）、转向 `X208`（navE+yaw+LV+RV）。

位序即 dump 时的列序，**一经发布不得重排，只能在尾部追加**——网页按 `CH,` 表头的列名解析，但固件写入顺序由位序决定。

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
| 9 | `lra` | `MOTION_LINE_MAX_ADJUST_RATIO` | 比例 | 0.01~1 |
| 10 | `lkd` | `MotionLine_TuneWeightKd`（新增，默认 0） | mm/s 每 权重/s | 0~100 |
| 11 | `nvx` | `NAV_MAX_TURN_SPEED_MMPS` | mm/s | 10~500 |
| 12 | `nvn` | `NAV_MIN_TURN_SPEED_MMPS` | mm/s | 1~500 |
| 13 | `nsa` | `NAV_SLOWDOWN_ANGLE_DEG` | ° | 5~180 |
| 14 | `ntl` | `NAV_ANGLE_TOLERANCE_DEG` | ° | 0.5~20 |
| 15 | `gsc` | 陀螺仪尺度因子（`Heading_Get/SetScale`，默认 1.0） | 比例 | 0.5~2 |
| 16 | `cpm` | `Odometry_CountsPerMM`（默认 6.23） | 计数/mm | 0.5~50 |
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

id 一经发布不得重排，新增参数只能在尾部追加。K1~K5 为旧上位机保留：写入会同时覆盖左右轮，读取返回两侧当前值的平均数；新调参应使用 K17~K26 分别设置左右轮。基础前馈公式为 `PWMbase = speed×ff + sign(speed)×sf`，再叠加该轮 PI 与上层 trim，最终夹到 ±1000；网页会按当前 W 目标实时显示左右基础 PWM 并提示饱和。`lkd` 是巡线权重变化率阻尼：默认 0 时巡线行为与纯离散权重差速完全一致，弧线段左右摆动明显时少量增加抑制震荡。`gsc` 由 `E1`→原地转 N 圈→`Y<n>` 标定流程自动写入；`cpm` 建议用网页里程标定向导（记起点→`F1000`→填卷尺实测→自动换算写入）。

**遥测 CSV 格式（⚠️ 历史架构，已被 3.3.0 的二进制帧取代）。** 以下 ASCII CSV 描述对应第一次架构；当前固件发二进制 SCHEMA/SAMPLE 帧，通道定义见 3.3.2。保留本段仅为理解演进历史。每次字段掩码改变时输出一行表头 `H,...`，随后每隔 `1000/G` ms 输出一行数据：

| 列名 | 对应掩码位 | 列数 | 内容 |
|---|---|---|---|
| `ms` | 固定输出 | 1 | 自上电以来的系统节拍累计毫秒数 |
| `yaw` | `TELEMETRY_FIELD_YAW` = `0x01` | 1 | Z 轴连续累计航向角（度，`%.2f`），不做 ±180° 归一化 |
| `gray`,`keys` | `TELEMETRY_FIELD_SENSOR` = `0x02` | 2 | `gray` 是五路灰度位图，**十六进制两位**（如 `1F`）；`keys` 是按键位图，十进制 |
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

OLED 默认显示 Z 轴连续角度、五路灰度、四个按键、左右轮路程/速度和当前运动状态。Gimbal 显式使能后切换为 F32C 双轴角度页。

| 蓝牙命令 | 作用 | 当前限制 |
|---|---|---|
| `C0` | 全局停车并复位 Mission | 始终有效，同时失能 Gimbal |
| `C1~C255` | Mission 单次事件 | 当前 25H 未绑定 |
| `L<number>` | 设置左轮开环 PWM | MotionManager 空闲时有效，范围 `-1000~1000` |
| `R<number>` | 设置右轮开环 PWM | MotionManager 空闲时有效，范围 `-1000~1000` |
| `U<number>` | 设置双轮相同开环 PWM | MotionManager 空闲时有效，范围 `-1000~1000` |
| `O<number>` | 设置纵向舵机角度 | 当前限位 `0°~270°` |
| `D<number>` | 设置横向舵机角度 | 当前限位 `0°~270°` |

## 4. 工程文件类型与职责

### 4.1 Application

| 文件 / 目录 | 类型 | 职责 |
|---|---|---|
| `Application/Core/App.c/.h` | 应用运行层 | 完整整车初始化、固定更新链和 Mission 上下文 |
| `Application/Core/TestApp.c/.h` | 可选测试运行层 | 跳过 OLED、MPU6050、灰度和里程的快速测试入口；当前未使用 |
| `Application/Comms/BluetoothDebug.c/.h` | 应用通信层 | 解析 `C/L/R/U/O/D` 命令并提供任务事件 |
| `Application/Comms/K230Link.c/.h` | 应用通信层 | K230 帧、CRC8、握手和目标解析；当前未运行 |
| `Application/Control/MotionManager.c/.h` | 统一运动调度 | 保证同一时刻只有直线、巡线、转向或刹车之一控制双轮 |
| `Application/Control/MotionStraight.c/.h` | 直线控制 | 定距速度规划、连续航向保持和可选终点速度 |
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
| `Hardware/Comms/Serial.c/.h` | UART 驱动 | UART1/UART2 中断接收环形缓冲区和发送接口 |
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

- `To` 接口输入连续累计绝对角；例如当前为 370°，输入 90° 会按直接误差回到 90°，不会自动选择 ±180° 最短路径。
- `By` 接口输入相对转角；正负方向由 `NAV_ROTATION_COMMAND_SIGN` 与实车安装共同决定，可输入大于 360° 的多圈角度。
- 首次测试使用 60~80 mm/s。若启动后角度误差持续增大，只翻转 `NAV_ROTATION_COMMAND_SIGN`。
- Nav 到角后先把轮速斜坡降到零，再要求连续 `NAV_SETTLE_TICKS` 个周期处于允许误差内，避免单次采样抖动误判完成。

当前 25H 在 KEY1 启动巡线时记录连续航向作为 0°基准。每次左侧双灰度触发后均先完成 150 mm 直行和固定时长的零速目标保持，再把下一绝对目标减去 90°并调用 `MotionManager_TurnTo()`；因此目标序列为 `startYaw-90°`、`startYaw-180°`、`startYaw-270°`……，不是在进入 TURN 时临时调用相对角接口。

## 4. 工程文件类型与职责

| 文件或目录 | 类型 | 职责 |
|---|---|---|
| `main.c` | C 源文件 | 仅调用 App 初始化、Mission 初始化、硬件中断启用以及 App/Mission 主循环 |
| `main.syscfg` | TI SysConfig | 时钟树、GPIO、UART、I2C、PWM、SysTick 和 PinMux 的唯一配置源 |
| `car_debug.html` | 单文件调试网页 | 浏览器端上位机：Web Serial 连接无线串口，发送第 3.3 节命令、解析遥测 CSV、实时画曲线并导出。无外部依赖，不参与固件编译 |
| `tests/*.mjs` | Node 测试脚本 | 无依赖，`node tests/<文件>` 直接跑。覆盖遥测解析、页面启动、固件-网页协议契约与教程源码一致性；清单见 4.2 节末 |
| `tutorial/sync-sources.mjs` | Node 工具脚本 | 按 `data-source`/`data-lines` 把教程内联源码重新对齐到当前源文件；源码位移时按内容自动重定位行号 |
| `.project`、`.cproject`、`.settings/` | CCS 工程元数据 | 工程名、TI Arm Clang 选项、SDK/SysConfig 依赖和 IDE 设置 |
| `targetConfigs/*.ccxml` | CCS 目标配置 | MSPM0G3507 调试连接配置 |
| `Application/Comms/` | 应用层 C 模块 | 蓝牙调试命令；K230 二进制帧、CRC8、握手和目标解析 |
| `Application/Core/` | 应用运行层 C 模块 | 固定硬件初始化、零漂、100 Hz 后台服务、按键和蓝牙事件采集 |
| `Application/Mission/` | 通用任务执行层 C 模块 | 校验并执行题目层提供的静态状态图、生命周期回调、有序条件转换和打断处理 |
| `Accomplish/` | 具体题目实现 C 模块 | 位于工程根目录；每道题独立保存用户参数、状态编号、回调、转换表和静态状态图；当前启用 `25H.c/.h`，并保留 `25E.c/.h` 与刹车测试 `Test.c/.h` |
| `Application/Control/` | 运动控制层 C 模块 | MotionManager、通用 PID、公共双轮速度闭环、直线、巡线、目标角转向和短暂主动刹车 |
| `Application/Debug/` | 应用层 C 模块 | OLED 调试页面编排、CSV 遥测输出与运行时调参注册表 |
| `Application/Servo/` | 舵机硬件模块 | TIMA0 双通道 PWM、角度限位和脉宽换算 |
| `Application/State/` | 状态层 C 模块 | Z 轴航向角解算、编码器里程与速度状态 |
| `Hardware/Board/` | 板级驱动 | 按键、LED、蜂鸣器 |
| `Hardware/Comms/` | 通信驱动 | UART1 无线串口和 UART2 K230 的中断接收环形缓冲区与发送接口 |
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
| `Application/Comms/BluetoothDebug.c/.h` | 解析 `C/L/R/U/O/D/G/M/V/F/B/T/A/Z/P/W/N/K/E/Y` 命令，保存单槽任务事件，限制自动运动期间的开环电机调试；`K` 命令带文本参数缓冲转交 Param 模块 |
| `Application/Core/App.c/.h` | 封装系统初始化和每拍固定更新，向 Mission 提供 dt、按键边沿和蓝牙信号 |
| `Application/Mission/Mission.c/.h` | 定义状态图公共类型，校验题目状态图并执行每拍最多一次的状态转换 |
| `Accomplish/25E.c/.h` | 保存 25E 参数和状态图：每轮绝对目标在上一目标上增加 180°；当前未由 main 加载 |
| `Accomplish/25H.c/.h` | 保存 25H 参数和状态图：KEY1 启动巡线，左侧双黑线后直行 150 mm、固定时长零速保持，绝对左转目标每轮减少 90°并循环 |
| `Accomplish/Test.c/.h` | 独立刹车测试状态图：KEY2 启动定距直行、短暂刹车并返回等待；需要测试时才在 main.c 临时加载 |
| `Application/Comms/K230Link.c/.h` | 解析 `AA 55` 二进制帧和 CRC8，执行 READY/READY_ACK 双向握手，保存最新 TARGET |
| `Application/Control/PID.c/.h` | 通用 PID 初始化、调参、复位和单步计算 |
| `Application/Control/MotionStraight.c/.h` | 头文件顶部保存直线参数；源文件实现距离规划、5/6 末段减速、可选终点速度、MPU6050 航向 PD 和软停车状态机 |
| `Application/Control/MotionWheel.c/.h` | 头文件顶部保存公共轮速参数；源文件实现 MotionStraight、MotionLine 与 Nav 共用的双轮速度 PI、前馈、差速修正合成和 PWM 限幅 |
| `Application/Control/MotionLine.c/.h` | 头文件顶部保存巡线参数；源文件实现五路灰度离散权重差速、连续丢线确认、丢线正常完成和状态管理；巡线层不使用 PID |
| `Application/Control/MotionManager.c/.h` | 统一包装直线、巡线、转向和短暂刹车；自动停止旧模式并只更新当前模式 |
| `Application/Control/Nav.c/.h` | 头文件顶部保存转向参数；源文件实现连续航向目标、双轮等速反向转向和到角稳定判定 |
| `Application/Debug/DebugDisplay.c/.h` | 组织启动零漂提示、基础状态和 MotionLine 运行状态的 OLED 八行调试数据 |
| `Application/Debug/Telemetry.c/.h` | 表驱动组装二进制 SCHEMA/SAMPLE 帧，按频率和 12 通道掩码经 UART1（DMA）输出；暴露 `SampleChannels`/`SendSchema` 供捕获复用 |
| `Application/Debug/TelemFrame.c/.h` | 二进制帧编码：CRC-8/ATM、帧构建、float32 小端打包；与 K230Link 同 CRC，供遥测与捕获共用 |
| `Application/Debug/Param.c/.h` | 运行时调参注册表：K 命令后端，26 个参数的读写、范围校验、左右轮独立参数与 PID apply 钩子 |
| `Application/Debug/Capture.c/.h` | 板载高速捕获：24 KB RAM 缓冲，可变长度二进制存储最多 8 通道（单通道约 30 秒）；与遥测共用通道表，dump 发 CAP_META/SAMPLE/END 二进制帧 |
| `Application/Servo/Servo.c/.h` | 将舵机角度换算为 TIMA0 比较值，并执行纵向/横向限位 |
| `Application/State/Heading.c/.h` | MPU6050 Z 轴零漂标定、角速度积分和尺度标定 |
| `Application/State/Odometry.c/.h` | 读取编码器增量，累计左右路程并计算 mm/s 速度 |
| `Hardware/Board/Beep.c/.h` | 蜂鸣器与 LED2 的非阻塞提示状态机 |
| `Hardware/Board/Key.c/.h` | 四个低有效按键的非阻塞状态读取 |
| `Hardware/Board/LED.c/.h` | LED1、LED2 的开、关、翻转接口 |
| `Hardware/Comms/Serial.c/.h` | UART1/UART2 RX 中断与环形缓冲；UART1 **TX 走 DMA 非阻塞发送**（TX 环形缓冲 + `DMA_DONE_TX` 中断推进、环绕分两次），主循环只写不等，缓冲满整帧丢弃并计数 |
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

### 4.2 `car_debug.html` 调试网页

**打开方式。** 双击 `car_debug.html` 即可，`file://` 下 Web Serial 可用（该协议属于安全上下文），不需要架本地服务器。只支持 **Chrome / Edge** 等 Chromium 内核浏览器，Firefox 与 Safari 没有实现 Web Serial API。点「连接串口」后在弹出的设备列表里选无线 DAPLink 对应的串口，波特率固定 115200 8N1。

**与固件的约定。** 网页只依赖第 3.3 节那套 ASCII 命令和第 5.7 节的遥测 CSV，不假设任何固件内部实现。因此改动固件时只要保持这两个契约不变，网页无需同步修改；反过来，一旦新增命令或调整 CSV 字段，必须同步更新第 3.3 / 5.7 节，网页按表头列名解析，不硬编码列位置。

页面服务**两个目的**：遥控采集数据（供后续视觉/决策任务使用）和闭环调参。分为八块：连接与急停、运动控制（含 `W` 恒速与 `N` 巡线）、**遥控采集**、参数面板、标定向导、调参试验、遥测与曲线、原始终端。

**工作流一：调参试验。**

1. 用「任务→对象→阶段」导航选定要调什么（如 轮速 → 左轮 → P），点「开始试验」；
2. 连接和阶段切换时，网页提前准备本阶段的**最优掩码**（调左轮只传 TL/LV/PL）并按固件 `Q` 回报的真实频率上限设频；
3. 点「开始试验」后，网页先清空本次终端与曲线、记录参数快照，然后立即发激励命令，不再等待遥测准备链；
4. 指标卡明确标注数据来源：直启流程使用 `实时流 xx Hz`，峰值可能漏采、仅供参考；若后续流程使用板载捕获，`板载 100 Hz 捕获` 表示与控制环同频、指标可信；
5. 每条记录可设为基准（虚线叠加对比）、导出 CSV，或一键复制 **AI 调参包**（含控制结构、参数、激励、列说明、自动指标和完整数据），直接粘给 AI 要下一组参数；
6. 在阶段参数区就地微调（`−`/`+` 按钮或直接输入，写入立即生效），再跑下一次对比。

**工作流二：遥控采集。** 启用键盘驾驶后用 <kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> 驾驶（W/S 走恒速命令，A/D 走相对转角），点「开始录制」整段记录遥测流；录制中按 <kbd>1</kbd>~<kbd>5</kbd> 打场景标记（直道/弯道/路口/障碍/异常），标记写入 CSV 的 `mark` 列。停止后可导出整段 CSV，或**按标记分段导出**——每个场景一个文件，便于后续分类使用。<kbd>Esc</kbd> 全局急停。

参数面板连接后自动 `K?` 拉取全表，悬浮显示范围与上电默认值，支持存到浏览器 localStorage、下次上电一键回写（固件参数掉电不保存）。标定向导包装陀螺尺度 `E1`/`Y`/`E0` 流程和里程 `cpm` 换算写入。

几个实现上的取舍：

- **频率上限由固件说了算，网页不各存一份阈值。** 改掩码后网页主动发 `Q` 查询，用固件回报的 `MAX` 自动设频；勾选字段的瞬间另有一份本地复算用于即时显示（公式与 `Telemetry.c` 相同），两者不一致就说明固件与网页失配。
- **指标必须标注来源。** 直启流程优先让车先动，因此默认用实时流指标；实时流受串口带宽限制低于 100 Hz 控制环，阶跃峰值会落在采样点之间，界面必须让用户看见这次结论是基于哪种数据得出的。
- **捕获与实时流分开解析、分开存储。** 前缀 `CH,`/`C,` 与 `H,`/`D,` 互不冲突，两者列集和采样率都不同，混在一起会让指标取到错误的时间基准。
- **舵机滑块节流 100 ms。** 拖动会逐像素触发事件，不节流会瞬间发出几十条命令占满串口，把遥测流挤垮。
- **曲线按量纲分组共享刻度。** `LV/RV/TL/TR` 同为 mm/s、`PL/PR` 同为 PWM、`yaw/navT/navE` 同为度、`LD/RD` 同为 mm，各组内共用一套 min/max，目标与实测才能直接对比；不在分组表里的列仍各自独立归一化。
- **`ms` 和 `mode` 不进曲线选择器。** 前者是横轴，画出来是单调直线；后者是文本。两者仍会写进导出的 CSV。
- **重绘按帧合并。** 每收一行就重绘会卡死页面，所以只在有新数据时排一个渲染帧；暂停按钮只冻结画面，数据仍照常累积。
- **列数与表头不符的行直接丢弃。** 掉电重连和掩码切换瞬间会出现半截行，宁可丢也不猜。

**测试。** 全部测试位于 `tests/`，直接 `node tests/<文件>` 运行，无需依赖：

| 测试 | 覆盖什么 |
|---|---|
| `test_csv_parse.mjs` | 从 `car_debug.html` 抽取解析器与纯函数源码运行——测的是页面里的真代码而非副本，避免两边漂移。含 CSV 解析、捕获行解析、阶跃指标、频率估算、标记分段 |
| `test_page_boot.mjs` | 用最小 DOM 桩**真正执行整段页面脚本**。`node --check` 只查语法，查不出 `let/const` 的初始化顺序错误（函数声明提升而变量不提升），那类错误只有打开页面才暴露；本测试把「打开就白屏」挡在提交前，并交叉校验 HTML 的 `id` 与脚本引用是否对得上 |
| `test_capture_contract.mjs` | 板载捕获与 `X`/`Q` 命令的固件-网页协议：通道位序、dump 前缀、回应格式、RAM 预算、单侧掩码的实际增益 |
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
| `K230Link.h` | `K230_LINK_MESSAGE_READY`、`K230_LINK_MESSAGE_READY_ACK`、`K230_LINK_MESSAGE_TARGET` | `0x01/0x02/0x10` | 消息类型 |
| `DebugDisplay.h` | `DEBUG_DISPLAY_REFRESH_TICKS` | `10U` | OLED 刷新周期，100 ms |

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
extern float MotionLine_TuneMaxAdjustRatio;
extern float MotionLine_TuneWeightKd;

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

`MotionLine_GetLineError()` 当前返回最近一次有效灰度位图得到的离散权重，范围为 `-6~+6`，它不再是 PID 输入误差。两个 `Tune` 变量运行时可调、写入即生效：`TuneMaxAdjustRatio` 对应原 `MOTION_LINE_MAX_ADJUST_RATIO`；`TuneWeightKd` 是权重变化率阻尼（默认 0，行为与纯离散权重差速一致），差速修正总量被限制在 ±巡航速度内。

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

该函数返回 25H 静态只读状态图。当前 `main.c` 调用该接口；题目参数全部位于 `Accomplish/25H.h` 开头。

### 5.6.5 `Accomplish/Test.h`

```c
const Mission_GraphDefinition_t *AccomplishTest_GetMissionGraph(void);
```

该函数返回独立的刹车测试状态图。测试时才在 `main.c` 临时加载；KEY2 会执行“定距直行 -> 短暂刹车 -> 等待”，不影响 25H 的正式流程。

### 5.7 `Application/Debug/DebugDisplay.h`

```c
void DebugDisplay_Init(void);
void DebugDisplay_ShowHeadingCalibration(uint8_t mpuReady);
void DebugDisplay_Update(uint8_t elapsedTicks);
```

### 5.7.1 `Application/Debug/Telemetry.h`

```c
#define TELEMETRY_DEFAULT_RATE_HZ        20U
#define TELEMETRY_RATE_HARD_LIMIT_HZ       100U
#define TELEMETRY_FIELD_YAW      0x01U
#define TELEMETRY_FIELD_SENSOR   0x02U
#define TELEMETRY_FIELD_DISTANCE 0x04U
#define TELEMETRY_FIELD_SPEED    0x08U
#define TELEMETRY_FIELD_MODE     0x10U
#define TELEMETRY_FIELD_K230     0x20U
#define TELEMETRY_FIELD_TARGET   0x40U
#define TELEMETRY_FIELD_PWM      0x80U
#define TELEMETRY_FIELD_NAV      0x100U
#define TELEMETRY_FIELD_LINE     0x200U
#define TELEMETRY_FIELD_ALL      0x3FFU

void Telemetry_Init(void);
void Telemetry_Update(uint8_t elapsedTicks, uint8_t pressedKeys);
uint8_t Telemetry_SetRateHz(uint8_t rateHz);
uint8_t Telemetry_SetFieldMask(uint16_t mask);
uint8_t Telemetry_GetRateHz(void);
uint16_t Telemetry_GetFieldMask(void);
uint8_t Telemetry_GetMaxRateHz(void);
```

**为什么频率上限是动态的：** `Serial1_SendByte()` 使用 `DL_UART_Main_transmitDataBlocking()`，是**阻塞发送**——发送期间主循环完全停止，`Heading_Update`、`Odometry_Update`、`MotionManager_Update` 全部挂起。主循环为 100 Hz（每 tick 10 ms），115200 8N1 每字节需 86.8 µs；若允许全字段（约 160 字节）以 100 Hz 发送，主循环占用率会远超 100%，PID 周期将严重失准。因此上限必须根据当前掩码的**实际行长**动态计算，而非固定为 100。调试时只开 YAW 一个字段（行长 25 字节），安全上限可达 92 Hz；开全部 10 个字段时安全上限降至约 14 Hz。这也符合实际调试习惯：需要高频率的场景（如看 PID 阶跃响应）只开相关字段——网页「调参试验」的预设就是按这个原则生成的。

**带宽常量跟随 SysConfig，不写死。** 计算上限用的两个常量定义在 `Telemetry.c` 内部而非头文件里，其中每秒字节数直接由波特率推导：

```c
/* 8N1 每字节含起始位和停止位共 10 个位时。 */
#define TELEMETRY_UART_BYTES_PER_SECOND  ((uint32_t)BLUETOOTH_UART_BAUD_RATE / 10U)
#define TELEMETRY_MAX_BLOCKING_PERCENT   20U
```

这样改 UART1 波特率时限流会自动跟上。若写死数值，一旦改了波特率却漏改这里，限流就会按错误的带宽计算，而这种失配没有任何编译期提示：波特率被调低时会严重超发，阻塞发送把主循环连同运动控制一起拖垮。反过来，若忘记在 CCS 中重新生成 SysConfig（`Debug/ti_msp_dl_config.h` 仍是旧波特率），遥测只会自动降到很低的频率，不会超发——失效方向是安全的。

`Telemetry_Init()` 也会用 `Telemetry_GetMaxRateHz()` 把默认频率夹一次。当前默认掩码为全字段（安全上限约 14 Hz），默认请求 20 Hz 会在 `Init` 中被自动夹到上限；默认频率、默认掩码、波特率三者任一改动都会改变这个结果，而 `Init` 不经 `Telemetry_SetRateHz()` 校验，越界不会有任何提示，全靠这次显式夹紧。

### 5.7.2 `Application/Debug/Param.h`

```c
void Param_HandleCommand(const char *args);
```

K 命令后端：`args` 为去掉 `K` 前缀的文本参数（`?` 列表 / `<id>?` 读 / `<id>=<float>` 写），直接经 UART1 回应。参数注册表见 3.3.1；表序即协议 id，一经发布不得重排，只能尾部追加。

### 5.7.3 `Application/Debug/Capture.h`

```c
#define CAPTURE_MAX_CHANNELS 4U
#define CAPTURE_BUFFER_BYTES 20480U
#define CAPTURE_CH_TL   0x0001U
#define CAPTURE_CH_LV   0x0002U
#define CAPTURE_CH_PL   0x0004U
#define CAPTURE_CH_TR   0x0008U
#define CAPTURE_CH_RV   0x0010U
#define CAPTURE_CH_PR   0x0020U
#define CAPTURE_CH_YAW  0x0040U
#define CAPTURE_CH_NAVE 0x0080U
#define CAPTURE_CH_LERR 0x0100U
#define CAPTURE_CH_ALL  0x01FFU

typedef enum
{
    CAPTURE_STATE_IDLE = 0,
    CAPTURE_STATE_ARMED,
    CAPTURE_STATE_RUNNING,
    CAPTURE_STATE_FULL,
    CAPTURE_STATE_DUMPING
} Capture_State_t;

void Capture_Init(void);
uint8_t Capture_Arm(uint16_t channelMask);
void Capture_Trigger(void);
void Capture_Update(uint32_t elapsedMs);
void Capture_Stop(void);
uint16_t Capture_StartDump(void);
void Capture_DumpNext(void);
Capture_State_t Capture_GetState(void);
uint16_t Capture_GetSampleCount(void);
uint16_t Capture_GetCapacity(void);
uint16_t Capture_GetChannelMask(void);
```

板载高速捕获（通道 B），设计动机见 3.3.0。几个必须遵守的约束：

- **`Capture_Update()` 必须排在 `MotionManager_Update()` 之后调用。** 目标速度和输出 PWM 都是本拍刚算出的，提前采会记录到上一拍的值，整条阶跃曲线向后偏移一个控制周期（10 ms），上升时间会被算长。
- **`Capture_Trigger()` 由运动命令在启动成功后调用**，仅在 `ARMED` 状态生效，其余状态是空操作。这样捕获的第一个样本恰好是阶跃起点。
- 缓冲写满即停（进入 `FULL`），不做环形覆盖——覆盖会让时间轴失去连续性，分析阶跃时更麻烦。
- 内部逐字节读写缓冲，不按 `float*` 解引用：样本大小随通道数变化，奇数偏移上直接解引用会触发 Cortex-M0+ 的非对齐访问异常。

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

转换数组从前到后就是优先级。动作运行时只检查打断转换，动作完成后只检查正常转换；每个系统节拍最多转换一次。`C0` 不受 `interruptible` 限制，始终停止并复位任务。

### 7.2 Accomplish 状态图入口

```c
const Mission_GraphDefinition_t *Accomplish25E_GetMissionGraph(void); /* 返回 25E 静态状态图。 */
const Mission_GraphDefinition_t *Accomplish25H_GetMissionGraph(void); /* 返回当前 25H 静态状态图。 */
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
| `K230Link.h` | `K230_LINK_MESSAGE_READY/READY_ACK/TARGET` | `0x01U` / `0x02U` / `0x10U` | 消息类型编号 |
| `DebugDisplay.h` | `DEBUG_DISPLAY_REFRESH_TICKS` | `10U` | OLED 10 Hz 刷新间隔 |
| `MotionStraight.h` | `MOTION_STRAIGHT_*` | 见 6.2 | 航向 PD、直线速度规划、减速起点比例和距离允许误差 |
| `MotionManager.h` | `MOTION_MANAGER_BRAKE_*` / `MOTION_MANAGER_SPEED_MAX_MMPS` | 见 6.2.1 | 定距软停后的 PWM 释放与短暂主动刹车时间；W 恒速调试模式速度上限 |
| `MotionWheel.h` | `MOTION_WHEEL_*` | 见 6.1 | MotionStraight、MotionLine 与 Nav 共用的速度 PI、前馈和 PWM 限幅 |
| `MotionLine.h` | `MOTION_LINE_*` | 见 6.3 | 灰度权重、最大速度调整比例、巡线速度上限和丢线确认节拍 |
| `Accomplish/25E.h` | `ACCOMPLISH_25E_*` | 见 6.5 | 25E 启动按键、直线距离与速度、入线确认、巡线速度和转向参数 |
| `Accomplish/25H.h` | `ACCOMPLISH_25H_*` | 见 6.6 | 25H 启动按键、左侧标志掩码、巡线、150 mm 直行和绝对左转参数 |
| `Accomplish/Test.h` | `ACCOMPLISH_TEST_*` | 见 6.7 | KEY2 启动的定距软停与短刹测试参数 |
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
| `MOTION_WHEEL_KP` / `MOTION_WHEEL_KI` / `MOTION_WHEEL_INTEGRAL_LIMIT` | `1.0f` / `0.0f` / `0.0f` |
| `MOTION_WHEEL_FEEDFORWARD_PWM_PER_MMPS` / `MOTION_WHEEL_STATIC_FRICTION_PWM` | `2.0f` / `0.0f` |
| `MOTION_WHEEL_MAX_COMMAND_PWM` | `1000.0f` |

前 5 个公共宏是左右轮独立运行时变量的初始默认值。新调参使用 `K17~K21`（lwkp/lwki/lwil/lwff/lwsf）和 `K22~K26`（rwkp/rwki/rwil/rwff/rwsf）；`K1~K5` 保留为同时写两轮的兼容入口。`MAX_COMMAND_PWM` 保持编译期固定。

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

以下宏位于 `Application/Control/MotionLine.h` 开头。当前 25H 通过 MotionManager 启动巡线，连续丢线确认后把巡线标记为正常完成并返回等待：

| 宏 | 单位 | 当前值 | 作用 |
|---|---:|---:|---|
| `MOTION_LINE_OUTER_WEIGHT` | 无 | `6` | 左右最外侧灰度权重的绝对值，对应最大修正力度 |
| `MOTION_LINE_INNER_WEIGHT` | 无 | `3` | 左右内侧灰度权重的绝对值，对应最大修正力度的一半 |
| `MOTION_LINE_MAX_ADJUST_RATIO` | 比例 | `0.2f` | 权重达到正负 6 时，一侧减去、另一侧增加的巡线速度比例 |
| `MOTION_LINE_MAX_SPEED_MMPS` | mm/s | `1000.0f` | 巡线请求软件上限；应结合公共轮速前馈和最终 PWM 上限设置 |
| `MOTION_LINE_LOST_CONFIRM_TICKS` | 100 Hz 节拍 | `50U` | 连续五路全白达到 50 次后确认丢线，当前约为 500 ms |

`MAX_ADJUST_RATIO` 是运行时变量 `MotionLine_TuneMaxAdjustRatio` 的上电默认值（`K9` lra）；另有仅运行时的权重变化率阻尼 `MotionLine_TuneWeightKd`（`K10` lkd，默认 0，行为与原纯离散权重差速一致）。

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

`Test.c/.h` 只用于上机观察短刹时的滑行量。它已包含在 CCS 工程中，但默认 `main.c` 仍加载 25H；测试时按头文件注释临时改为加载 `AccomplishTest_GetMissionGraph()`，完成后恢复 25H。

| 宏 | 单位 | 当前值 | 作用 |
|---|---:|---:|---|
| `ACCOMPLISH_TEST_START_KEY_MASK` | 按键位图 | `0x02U` | KEY2 的 bit1 掩码；按下沿启动一次测试 |
| `ACCOMPLISH_TEST_BRAKE_DISTANCE_MM` | mm | `300U` | 测试用定距直行距离 |
| `ACCOMPLISH_TEST_BRAKE_SPEED_MMPS` | mm/s | `200.0f` | 测试用巡航速度 |
| `ACCOMPLISH_TEST_BRAKE_END_SPEED_MMPS` | mm/s | `0.0f` | 必须为零，直线完成后才会进入 BRAKE 状态 |

`PID_t` 的 `Kp/Ki/Kd`、`integral`、`prevError`、`outMax` 和 `integralMax` 为 PID 实例的公共状态与参数。除上述公开声明外，其余 `static` 数据和源文件内宏均为模块内部实现。
