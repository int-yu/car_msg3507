# 主从双车通信（CarLink）说明

两辆车各接一个 HC05 在 **PB6/PB7（UART1）** 上，互为主从、点对点通信。
链路用二进制帧 + DMA 非阻塞发送，复用工程里已验证过的 K230 帧范式，
留足扩展点——**以后要传什么内容，基本只加“消息类型 + 一个 case”即可**。

## 一、串口分工（改动后）

| 逻辑口 | UART | 引脚 | 现在接谁 | 发送方式 |
|---|---|---|---|---|
| Serial1 | UART2 | PA21/PA22 | DAPLink → 电脑/网页 | DMA TX (CH0) |
| Serial2 | UART1 | **PB6/PB7** | **HC05 主从链路（CarLink）** | **DMA TX (CH1)** |
| Serial3 | UART3 | PA14/PA25 | K230 视觉链路（与 CarLink 独立） | DMA TX (CH2) |

> `Serial1`/`Serial2` 是稳定的逻辑接口名，不等于物理外设编号：
> `BLUETOOTH_UART` 当前落在 UART2，`BRUSHLESS_UART` 当前落在 UART1。
> K230Link 当前使用独立 `Serial3`/UART3；PA25 RX 从 PinMux 阶段上拉，K230 未连接时保持离线，不占用 CarLink。F32C/Gimbal 仍停用。

## 二、主从身份：编译期二选一（两套固件）

见 `Application/Core/CarRole.h`：

- **主车**（连 daplink 那台）：默认就是 `CAR_ROLE_MASTER`，直接编译烧录。
- **从车**：把 `CarRole.h` 里默认值改成 `CAR_ROLE_SLAVE` 再编译；
  或在工程编译选项加 `-DCAR_ROLE=CAR_ROLE_SLAVE`（-D 优先，不必改文件）。

身份只影响“谁转发网页命令 / 谁执行被转发的命令 / 谁在满足条件时触发事件”。
链路本身对称：两车都跑同一份 `CarLink`，都能主动收发。

## 三、编译前必须重新生成 SysConfig

UART 实例、PinMux、DMA 触发和 IRQ 宏都由 SysConfig 生成；交换映射后必须重新生成，不能沿用旧的 `Debug/ti_msp_dl_config.*`：

1. CCS 里双击打开 `main.syscfg`。
2. 确认 **BLUETOOTH_UART = UART2 / PA21/PA22 / DMA_CH0**，
   **BRUSHLESS_UART = UART1 / PB6/PB7 / DMA_CH1**，
   **K230_UART = UART3 / PA14/PA25 / DMA_CH2**；三路中断都包含 **DMA Done TX + RX**。
3. `Ctrl+S` 保存 → 重新生成 `Debug/ti_msp_dl_config.c/.h`（会多出 `DMA_PEERLINK_TX_CHAN_ID` 等符号）。
4. 右键工程 → **Clean Project** → **Build Project**。

> 生成头应显示 `BLUETOOTH_UART_INST = UART2`、`BRUSHLESS_UART_INST = UART1`；
> `K230_UART_INST = UART3`；对应 ISR 宏分别展开为 `UART2_IRQHandler`、
> `UART1_IRQHandler` 与 `UART3_IRQHandler`。

## 四、帧协议

```
0xAA 0x55 | 版本(0x01) | 类型 | 序号 | 长度 | 载荷...(≤32) | CRC8
```
- 双字节魔数 `0xAA 0x55` 起头抗噪；CRC8（多项式 0x07）覆盖 版本..载荷。
- 与 `K230Link` 完全同源的状态机，逐字节解析，坏帧自动重同步。

消息类型（`CarLink.h` 的 `CarLink_MsgType_t`）：

| 类型 | 值 | 载荷 | 用途 |
|---|---|---|---|
| `HEARTBEAT` | 0x01 | 无 | 保活，判断对端在线（不进消息队列） |
| `RELAY_CMD` | 0x10 | ASCII 命令串 | 网页→主机→从机 的运动命令转发 |
| `RELAY_REPLY` | 0x11 | ASCII 回应文本 | 从机执行结果→主机→网页（`SLV ` 前缀显示） |
| `EVENT` | 0x20 | [0]=事件号,余为参数 | 主机满足条件时同步给从机 |
| `ACK` | 0x30 | [0]=被确认的号 | 从机回执 |

## 五、网页控制从机

驾驶采集面板新增**目标下拉框：主车 / 从车 @ / 同时**：
- **主车**：命令本地执行（原行为）。
- **从车**：命令加 `@` 前缀发出（如 `@J100/100`）。主机收到 `@` → 把后面的命令
  原样打包成 `RELAY_CMD` 发给从机 → 从机喂进自己的命令解析器执行
  （**复用全部现有命令** J/W/F/T/L/R…）。
- **同时**：主车发 `J...`、从车发 `@J...`，两车做同一动作（编队/并行）。
- 切换目标时，网页给两车各补一帧 `J0/0`，避免旧目标车失控。

**回传**：主机收到 `@` 命令先回 `OK @J100/100`（表示“已转发”）；从机真正执行后的
`OK/ERR` 会经 `RELAY_REPLY` 传回主机，网页里以 **`SLV ` 前缀**独立成行显示
（如 `SLV OK J=100/100`）。这样能真正确认从车收没收到、执行成没成功。

> 从机→主机是通用回传通道：从机 `CarLink_Send(任意类型, 数据, 长度)`，主机
> `App_HandlePeerMessage` 认识的类型走专门显示，不认识的兜底打成
> `PEER t=.. n=.. <十六进制>` 到网页——**从机以后加新上报类型，主机不改也能看到**。

## 五·五、从车遥测（两个曲线面板）

从车的遥测经主车转发到网页，网页用**独立的第二个曲线面板**显示，掩码/频率与主车独立。

**链路（零额外封装，省带宽）**：从车遥测帧格式与主车完全相同，只把**首字节 0xAA 换成
0xAB**（CRC 只覆盖 VER..PAYLOAD 不含 magic，改首字节不破坏校验）。
- 从车：`Telemetry_Emit()` 把帧发到 Serial2（HC05），首字节 0xAB。
- 主车：`CarLink` 里一个独立透传状态机识别 0xAB 帧，按帧长收齐后**原样转发**给电脑，
  不二次封装、不重算 CRC。与 0xAA 控制帧靠首字节区分。
- 网页：顶层 `routeStream` 按 0xAA/0xAB 分流到两个 `TelemetryParser`，各画各的面板。

**默认关闭**：从车遥测要与主车遥测共用主车 `Serial1`（物理 UART2）上行带宽，所以**从车默认 `rate=0`**，
网页从车面板发 `@G<rate>` 开启、`@M<mask>` 选通道。面板实时显示估算带宽占用。

**带宽/100Hz**：一帧 = `11 + 4×通道数` 字节。两车都选 6 通道 @50Hz ≈ 各 1750 B/s，
合计约占主车 `Serial1` 的 30%，**两车都能 100Hz**（全程 DMA 非阻塞）。别把从车遥测通道
开太多/太高频，网页带宽条会提醒。

**独立驾驶**：网页 `WASD` 控主车、`方向键` 控从车，可同时；点击驾驶盘按目标下拉框
（主车/从车/同时）走。

## 六、怎么加一种新的传输内容（这是重点）

以“主机把某个传感器读数发给从机”为例：

1. `CarLink.h` 的 `CarLink_MsgType_t` 加一个值，如 `CAR_LINK_MSG_SENSOR = 0x40`。
2. 发送端（主机某处满足条件时）：
   ```c
   uint8_t buf[2] = { hi, lo };
   CarLink_Send(CAR_LINK_MSG_SENSOR, buf, 2);
   ```
3. 接收端 `App.c` 的 `App_HandlePeerMessage()` 里加一个 `case CAR_LINK_MSG_SENSOR:`。

帧封装/解析/CRC/DMA 全不用动。

“主机触发某条件 → 通知从机”的**示例**已经接好：`App.c` 里主机把收到的任务信号
（C 命令 / 急停）作为 `EVENT` 同步给从机（一起启动 / 一起急停）。
备赛时把 `#if CAR_IS_MASTER` 那段的触发条件换成你真正要的判断即可。

## 七、涉及文件

- 新增：`Application/Core/CarRole.h`、`Application/Comms/CarLink.{h,c}`
- 改：`main.syscfg`（交换两路物理 UART 角色并新增独立 UART3/DMA CH2）、`Hardware/Comms/Serial.{h,c}`（ISR 随生成实例映射）、
  `Application/Comms/BluetoothDebug.{h,c}`（`@` 转发 + `FeedExternal`）、
  `Application/Core/App.c`（接入 CarLink 与独立 K230Link、停用云台）、`car_debug.html`（发给从机开关）
- 启用：`K230Link`（独立 UART3）；停用（保留代码）：`F32C`/`Gimbal`

## 八、实车验证建议（分阶段）

1. **先验网页链路**：只烧主车，通过 PA21/PA22 的 DAPLink 连接网页，看遥测/命令是否正常，
   确认 `Serial1`/UART2 的 RX、DMA TX 和 IRQ 都工作。此时从车可不上电。
2. **验链路**：两车都烧（一主一从）、都上电。主车网页发 `@W200`，看从车是否走；
   发 `C0` 看两车是否一起停（EVENT 示例）。
3. **验掉线**：断从车电，主车 `CarLink_IsPeerAlive()` 应在 ~2s 后转为离线。
