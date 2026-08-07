# MSPM0G3507 天猛星小车：资源与引脚定义

`main.syscfg` 是 PinMux、时钟和外设实例的唯一配置源。修改硬件连接、引脚、外设实例、时钟或波特率时，必须同步更新本文件；`Debug/`、`Release/` 和 `ti_msp_dl_config.*` 均为生成内容。

## 资源定义

| 资源 | 当前配置 | 对应硬件/模块 |
|---|---|---|
| CPUCLK / SYSCLK | 32 MHz | 全工程的时钟基准 |
| SysTick | 32 MHz / 320000 = 100 Hz | `System/Tick`，10 ms 系统节拍 |
| TIMG8 | 32 MHz，周期 1600，20 kHz | 双轮 PWM：`PB15` 右轮、`PB16` 左轮 |
| TIMG7 | 1 MHz，周期 20000，50 Hz | 普通舵机 PWM：`PA26` 横向、`PA27` 纵向 |
| TIMA0 CCP0 | BUSCLK / 8 = 4 MHz | MS42CG 步进脉冲输出，`PB8` |
| TIMG12 CCP0 | 组合捕获模式 | MT6816 单圈绝对角 PWM 捕获，`PB13` |
| I2C0 | 400 kHz | OLED，`PA28` SDA、`PA31` SCL |
| UART0 | 115200、RX 中断、DMA TX CH2 | K230 视觉链路（`Serial3`），`PA10/PA11` |
| UART1 | 未在 SysConfig 实例化 | `Serial2`/CarLink 保留为无物理链路的软件桩；`PB6/PB7` 空闲 |
| UART2 | 115200、RX 中断、DMA TX CH0 | PC/DAPLink 调试链路（`Serial1`），`PA21/PA22` |
| 软件 I2C | 约 100 kHz、开漏式 GPIO | 六路红外：`PA25` SDA、`PA14` SCL，地址 `0x5C`、状态寄存器 `0x05` |
| 软件 I2C | GPIO 开漏时序 | MPU6050：`PB2` SCL、`PB3` SDA |
| GPIOA GROUP1 IRQ | 双边沿、共享入口 | 左右轮正交编码器：`PA15/PA16/PA17/PA24`；步进 AB 反馈：`PA13/PA29` |
| SWD | 调试专用 | `PA19` SWDIO、`PA20` SWCLK |

### 未启用资源

- F32C/Gimbal 库保留，但当前主流程不初始化也不更新该模块。
- `Serial2`/CarLink 协议层保留；由于 UART1 未实例化，它不占用引脚或 DMA。
- 旧八路灰度输入已停用；`PB5`、`PB26` 保持未分配。

## 引脚定义

| Pin | 方向 / 复用 | 占用对象 | 说明 |
|---|---|---|---|
| PA10 | UART0 TX | K230 RX | `Serial3`，115200 baud |
| PA11 | UART0 RX、上拉 | K230 TX | `Serial3`，115200 baud |
| PA12 | GPIO 输出 | 右电机 AIN2 | TB6612 A 通道方向 |
| PA13 | GPIO 输入、上拉 | MS42CG AB-A | 步进反馈 A |
| PA14 | GPIO 输入 / 软件开漏 | 六路红外 SCL | 仅拉低或释放 |
| PA15 | GPIO 输入、上拉、双边沿 IRQ | 右编码器 A | 右轮正交反馈 |
| PA16 | GPIO 输入、上拉、双边沿 IRQ | 右编码器 B | 右轮正交反馈 |
| PA17 | GPIO 输入、上拉、双边沿 IRQ | 左编码器 A | 左轮正交反馈 |
| PA19 | SWDIO | 下载调试 | 不作普通 GPIO 使用 |
| PA20 | SWCLK | 下载调试 | 不作普通 GPIO 使用 |
| PA21 | UART2 TX | DAPLink RX | `Serial1`，115200 baud |
| PA22 | UART2 RX、上拉 | DAPLink TX | `Serial1`，115200 baud |
| PA24 | GPIO 输入、上拉、双边沿 IRQ | 左编码器 B | 左轮正交反馈 |
| PA25 | GPIO 输入 / 软件开漏 | 六路红外 SDA | 仅拉低或释放 |
| PA26 | TIMG7 CCP0 | 横向舵机 PWM | 50 Hz |
| PA27 | TIMG7 CCP1 | 纵向舵机 PWM | 50 Hz |
| PA28 | I2C0 SDA | OLED | 400 kHz |
| PA29 | GPIO 输入、上拉 | MS42CG AB-B | 步进反馈 B |
| PA30 | GPIO 输入、上拉 | KEY1 | 低电平按下，bit0 |
| PA31 | I2C0 SCL | OLED | 400 kHz |
| PB0 | GPIO 输出 | 左电机 BIN1 | TB6612 B 通道方向 |
| PB1 | GPIO 输出 | 左电机 BIN2 | TB6612 B 通道方向 |
| PB2 | 开漏式 GPIO | MPU6050 SCL | 软件 I2C 时钟 |
| PB3 | 开漏式 GPIO | MPU6050 SDA | 软件 I2C 数据 |
| PB5 | 未分配 | 保留 | 原八路灰度 CH7 已停用 |
| PB6 | 未分配 | 保留 | UART1/Serial2 预留 |
| PB7 | 未分配 | 保留 | UART1/Serial2 预留 |
| PB8 | TIMA0 CCP0 输出 | MS42CG ST | 步进脉冲输出，3200 ST/rev |
| PB9 | GPIO 输出 | MS42CG DIR | 步进方向 |
| PB10 | GPIO 输入、上拉 | KEY4 | 低电平按下，bit3 |
| PB11 | GPIO 输入、上拉 | KEY2 | 低电平按下，bit1 |
| PB12 | GPIO 输出 | MS42CG EN | 高电平使能 |
| PB13 | TIMG12 CCP0 捕获 | MT6816 PWM | MS42CG 单圈绝对角 |
| PB14 | GPIO 输入、上拉 | KEY3 | 低电平按下，bit2 |
| PB15 | TIMG8 CCP0 | 右电机 PWM | TB6612 A 通道，20 kHz |
| PB16 | TIMG8 CCP1 | 左电机 PWM | TB6612 B 通道，20 kHz |
| PB17 | GPIO 输出 | 蜂鸣器 | 低电平有效 |
| PB23 | GPIO 输出 | LED1 | 高电平点亮 |
| PB25 | GPIO 输出 | 右电机 AIN1 | TB6612 A 通道方向 |
| PB26 | 未分配 | 保留 | 原八路灰度 CH1 已停用 |
| PB27 | GPIO 输出 | LED2 | 高电平点亮 |

## 连接与冲突约束

- 当前 `main.syscfg` 中 UART0、UART2、I2C0、TIMA0、TIMG7、TIMG8、TIMG12、两组软件 I2C 与 SWD 的 PinMux 无重复分配。
- 六路红外接线为 `+5V -> +5V`、`GND -> GND`、`SDA -> PA25`、`SCL -> PA14`。SDA/SCL 的高电平必须为 **3.3 V**；板载上拉至 5 V 时须加双向 I2C 电平转换器。
- PA13、PA29、PB8、PB9、PB12、PB13 是当前工作的 MS42CG 专用资源，不可重新分配给旧灰度或其他传感器。
- 六路红外安装方向：探头朝前、CH1 在左时使用 `GRAYDETECT_CHANNEL1_IS_RIGHT = 0`；翻面安装才改为 `1`。
