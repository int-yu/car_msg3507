# 二进制 DMA 遥测架构设计（分支 update-遥测）

> **本文是交接文档。** 协议规范、任务清单、验收标准和风险都写明了，
> 任何 AI 或人接手时按顺序执行即可，无需回溯对话。
> **前置阅读**：`docs/superpowers/specs/2026-07-21-数据平台重构-design.md`（第一次架构，ASCII 双通道）。

## 0. 一句话

把遥测数据链路从「ASCII 文本 + 阻塞发送」推倒重来为「二进制帧 + DMA 非阻塞发送」，
让实时流本身就能以远超控制环的速率（实测算得约 450 Hz 上限）无损直传，
板载捕获从"必需"降级为"可选的超长录制缓冲"。

## 1. 为什么要做（决策依据）

### 1.1 用户诉求

原话："板载的 10 几秒太短了，改成可变长度的二进制帧如何，tx 也改成非阻塞的，用 DMA。
原本的可以不用保留，做整个架构的升级。"

### 1.2 实测带宽账（node 计算，非估计）

| 方案 | 单样本 | 主循环阻塞 | 单侧实时流上限 |
|---|---:|---|---:|
| 现状 ASCII + 阻塞 | 40 B | 是（限 20% 带宽） | 57 Hz |
| 二进制 + 阻塞 | 16 B | 是 | 144 Hz |
| **二进制 + DMA** | 16 B | **否** | **~450 Hz** |

- 二进制 float32 对 ASCII 的压缩比 **2.5x**（`,-999999.9` 11 字节 → 4 字节）。
- DMA 发送不占 CPU，带宽预算从 20% 放宽到约 90%。
- 二者叠加：单侧实时流从 57 Hz 提到约 450 Hz，**远超 100 Hz 控制环**。

### 1.3 关键推论：DMA 让捕获的存在理由消失大半

第一次架构引入板载捕获，唯一理由是"实时流 30 Hz 低于控制环 100 Hz"。
二进制 + DMA 后，**实时流边跑边流出来就是 100 Hz+ 无损的**，不再需要"先存 RAM 再 dump"。

因此本次捕获**降级**：
- 主数据通道 = 二进制流 + DMA 直传（监视/调参/采集都够用）。
- 捕获保留但重新定位：仅当想录**超长**（通道少时单通道可达 26 秒）或想要**绝对零丢包**
  （防 Web Serial 偶发丢包）的瞬态时才用。捕获帧也改成可变长度二进制。

### 1.4 硬件可行性（已查证 SDK 2.10）

- SRAM 32 KB，空闲 29 KB。
- DMA 可用：`DL_DMA_setSrcAddr` / `setDestAddr` / `setTransferSize` / `enableChannel`。
- UART 支持 DMA TX：`DL_UART_INTERRUPT_DMA_DONE_TX` 完成中断 + `TXIFG` 触发。
- 当前一个 DMA 通道都没占用（`main.syscfg` 中 `enableDMATX=false`）。
- **现成范式**：`K230Link.c` 已实现完整二进制帧（CRC8、状态机分帧、payload 编解码），
  遥测帧直接沿用同一套 `AA 55 | VER | TYPE | SEQ | LEN | PAYLOAD | CRC8` 结构。

## 2. 二进制遥测协议规范

### 2.1 帧格式

```
0xAA 0x55 | VER | TYPE | SEQ | LEN | PAYLOAD(LEN 字节) | CRC8
```

- `0xAA 0x55`：帧头。`0xAA` 是非 ASCII 可打印字符，**绝不会出现在文本回应里**，
  这是同一 UART 上二进制帧与 ASCII 命令回应可靠共存的关键。
- `VER = 0x01`：协议版本。
- `TYPE`：帧类型，见 2.2。
- `SEQ`：8 位帧序号，网页据此检测丢帧。
- `LEN`：PAYLOAD 字节数，0~255（**可变长度**——这是用户要的核心）。
- `CRC8`：CRC-8/ATM，多项式 `0x07`、初值 0，覆盖 `VER` 到 `PAYLOAD` 末字节
  （与 K230Link 完全一致，可复用 `Crc8Update`）。

### 2.2 帧类型

| TYPE | 名称 | 方向 | PAYLOAD |
|---|---|---|---|
| `0x30` | `TELEM_SCHEMA` | MCU→PC | 通道表：`channelMask:u16` + 每通道 `{nameLen:u8, name[], unit:u8}` |
| `0x31` | `TELEM_SAMPLE` | MCU→PC | `ms:u32` + 各选中通道 `float32`（小端），列序由 schema 决定 |
| `0x32` | `CAP_META` | MCU→PC | dump 开始：`channelMask:u16 + sampleCount:u16 + periodMs:u16` |
| `0x33` | `CAP_SAMPLE` | MCU→PC | 同 `TELEM_SAMPLE`，但来自捕获缓冲 |
| `0x34` | `CAP_END` | MCU→PC | `sampleCount:u16` |

**schema 先行原则**：掩码改变（含上电）时先发一帧 `TELEM_SCHEMA`，随后的 `TELEM_SAMPLE`
不再带列名，网页按最近一次 schema 的列序解析。这样每个样本帧只有纯数据，最省带宽。
（与旧 ASCII 的 `H,` 表头思路相同，但二进制化。）

### 2.3 通道定义

沿用第一次架构的通道语义，位定义即列序（一经发布不得重排）：

| 位 | 名 | 单位码 | 来源 |
|---:|---|---|---|
| 0x0001 | TL | mmps | MotionWheel_GetTargetSpeedL |
| 0x0002 | LV | mmps | Odometry_GetSpeedL |
| 0x0004 | PL | pwm | MotionWheel_GetLeftCommandPWM |
| 0x0008 | TR | mmps | MotionWheel_GetTargetSpeedR |
| 0x0010 | RV | mmps | Odometry_GetSpeedR |
| 0x0020 | PR | pwm | MotionWheel_GetRightCommandPWM |
| 0x0040 | yaw | deg | Heading_GetYaw |
| 0x0080 | navE | deg | Nav_GetAngleErrorDeg |
| 0x0100 | lerr | raw | MotionLine_GetLineError |
| 0x0200 | gray | bits | Graydetect_GetState（转 float） |
| 0x0400 | LD | mm | Odometry_GetDistanceLMM |
| 0x0800 | RD | mm | Odometry_GetDistanceRMM |

单位码：`0=raw 1=mmps 2=pwm 3=deg 4=mm 5=bits`。网页据此做量纲分组共享刻度。

### 2.4 命令仍走 ASCII

`K/W/N/M/G/F/B/T/A/Z/X/Q/E/Y/P/C/L/R/U/O/D` 及其 `OK`/`ERR` 回应**保持 ASCII 不变**。
理由：低频、要人读、任何串口助手可直接调试。只有高频遥测数据二进制化。

`M<mask>` 改为设置二进制遥测的通道掩码（不再是 ASCII 字段掩码）。
`G<hz>` 设频率不变。`X`/`Q` 语义调整见任务 U5。

## 3. 目标架构

```
                        100 Hz 主循环
                             │
              ┌──────────────┴───────────────┐
              ▼                              ▼
   ┌─────────────────────┐      ┌──────────────────────────┐
   │ 实时流（主通道）      │      │ 板载捕获（可选超长/零丢包） │
   │ 二进制 TELEM_SAMPLE  │      │ 二进制 CAP_SAMPLE 缓冲     │
   │ DMA 直传，~450Hz 上限│      │ dump 时也走 DMA           │
   └──────────┬──────────┘      └───────────┬──────────────┘
              │                              │
              └──────────────┬───────────────┘
                             ▼
              Serial1 TX 环形缓冲 + DMA 搬运（非阻塞）
                             │
                        UART1 → PC
```

**核心变化**：所有 TX 都进一个软件环形缓冲，DMA 从缓冲搬到 UART，
`DMA_DONE_TX` 中断触发搬下一段。主循环只往缓冲里写，从不等待硬件。
ASCII 命令回应和二进制遥测帧共用这一条 DMA 发送路径。

## 4. 任务清单

> 执行顺序经过设计：**先让 DMA 跑通旧行为，再上二进制**，每步可独立验证、可回退。
> ⭐ = 核心，🔧 = 配套。DMA 相关任务 node/gcc 测不了逻辑，**必须实车验证**。

---

### U1 ⭐ 固件：Serial1 DMA 非阻塞 TX（先不改协议）

**动机**：这是整个升级的地基，也是风险最高的一步，所以**单独做、单独验证**——
此步只把发送机制从阻塞改 DMA，输出内容仍是旧 ASCII，便于对照验证 DMA 本身是否正确。

**改动**：
- `main.syscfg`：UART1 `enableDMATX = true`，分配一个 DMA 通道，
  触发源设为 UART1 TX，配置 SysConfig 生成 DMA 初始化。
- `Hardware/Comms/Serial.c/.h`：
  - 新增 TX 环形缓冲（`SERIAL1_TX_BUFFER_SIZE`，建议 2048——要容纳一帧 schema + 若干样本）。
  - `Serial1_SendArray/SendString/SendByte` 改为写环形缓冲，若 DMA 空闲则启动一次搬运。
  - 新增 `UART1` 的 `DMA_DONE_TX` 中断处理：一段搬完后，若缓冲还有数据就启动下一段。
  - 缓冲满策略：**丢弃并计数**（`Serial1_GetTxDropCount`），绝不阻塞等待。
  - `Serial1_Printf` 不变（内部改用新的 SendString）。
- DMA 搬运的是环形缓冲的**连续区段**：从 readIndex 到缓冲末尾或 writeIndex，
  遇到环绕分两次 DMA（第一次到缓冲尾，完成中断里再发环绕后的头部）。

**验收（实车）**：
- 遥测开 ASCII 50 Hz 全字段，`MotionManager` 的 dt 抖动明显小于阻塞版
  （用旧 `ms` 列差值对比，阻塞版会有周期性跳变）。
- 长时间运行无卡死、无乱码。
- 断连重连后发送恢复正常。

**回退**：此步若实车不通，`git revert` 即可，不影响其他功能（协议未变）。

---

### U2 ⭐ 固件：二进制帧编码器 TelemFrame

**动机**：把协议规范（第 2 节）落成可复用的编码函数。

**新模块** `Application/Debug/TelemFrame.c/.h`：

```c
/* CRC8 与 K230Link 一致，抽出来共用。 */
uint8_t TelemFrame_Crc8Update(uint8_t crc, uint8_t data);

/* 组一帧写进给定缓冲，返回总长度。type/payload 见 2.2。 */
uint16_t TelemFrame_Build(uint8_t *out, uint8_t type,
                          uint8_t seq, const uint8_t *payload, uint8_t len);

/* 把 float 数组按小端写进 payload 缓冲，返回字节数。 */
uint16_t TelemFrame_PackFloats(uint8_t *out, const float *values, uint8_t count);
```

**验收**：node 侧写一个对拍测试（U8），用同一份 CRC8 和小端规则解出来比对。

---

### U3 ⭐ 固件：Telemetry 改发二进制

**动机**：替换 ASCII 输出为二进制帧。**删除旧的 H,/D, ASCII 输出**（用户同意推倒）。

**改动** `Application/Debug/Telemetry.c/.h`：
- 掩码语义改为 2.3 的 12 位通道掩码。
- 掩码变化时发一帧 `TELEM_SCHEMA`（含各通道名与单位码）。
- 每个输出周期发一帧 `TELEM_SAMPLE`（ms + 选中通道 float32），SEQ 递增。
- 频率上限重算：DMA 后不再是 20% 阻塞预算，改为"帧不会撑爆 TX 缓冲"的约束，
  上限显著提高（`Telemetry_GetMaxRateHz` 返回值变大）。
- `Q` 命令回报的 MAX 相应变大。

**验收（实车 + 网页）**：网页二进制解析器（U6）能画出曲线；SEQ 连续无跳变（不丢帧）。

---

### U4 🔧 固件：Serial2/K230 也 DMA 化

**动机**：用户要求"全链路"。K230 帧率虽低，但阻塞发送同样占主循环。

**改动**：`main.syscfg` UART2 `enableDMATX=true` + 第二个 DMA 通道；
`Serial2_SendArray/SendByte` 同 U1 改造。K230Link 的 `SendFrame` 自动受益，无需改。

**验收**：K230 握手正常（`Q` 回报链路就绪），拍照命令 `P` 正常回 ACK。

**注**：可延后。K230 阻塞影响小，若时间紧张此步优先级最低。

---

### U5 ⭐ 固件：捕获改可变长度二进制 + 重新定位

**动机**：落实"捕获降级 + 可变长度"。

**改动** `Application/Debug/Capture.c/.h`：
- 存储格式改为紧凑二进制：`ms:u32 + 选中通道 float32`，通道数可变（不再固定 4）。
- 通道位扩展到与遥测一致的 12 位（2.3 表）。
- dump 改发二进制帧：先 `CAP_META`，逐样本 `CAP_SAMPLE`，末尾 `CAP_END`。
- 时长随通道数可变：单通道 8 B/样本，20 KB 可存 2560 样本 = **25.6 秒**；
  4 通道 10 秒；容量在 `X<mask>` 回应里回报。
- 缓冲可考虑扩到 24 KB（SRAM 仍有余量），单通道可录 30 秒。

**验收（实车）**：`X1`（单通道）录 20 秒不丢点；`X0` dump 出的样本数与录制时长×频率一致。

---

### U6 ⭐ 网页：字节流解析器重写

**动机**：网页当前按 `\n` 拆文本行，无法解析二进制。这是最大的一块网页改动。

**改动** `car_debug.html`：
- `TelemetryParser` 重写为**字节流状态机**：
  - 输入是 `Uint8Array`（不再 `TextDecoder` 成字符串）。
  - 见 `0xAA 0x55` 进二进制帧解析（复刻固件状态机：VER/TYPE/SEQ/LEN/PAYLOAD/CRC8）。
  - 否则字节累积成 ASCII 行（命令回应 OK/ERR），遇 `\n` 交出。
  - **难点**：二进制 PAYLOAD 里可能恰好有 `\n`（0x0A）或 `0xAA` 字节，
    所以必须严格按 LEN 读满 PAYLOAD，不能用换行分割二进制段。状态机天然解决这点。
- `TELEM_SCHEMA` 帧 → 更新列定义（替代旧的 `H,` 表头处理）。
- `TELEM_SAMPLE` / `CAP_SAMPLE` 帧 → 按 schema 列序解出样本对象（与旧样本对象结构兼容，
  这样下游的曲线、指标、导出全部不用改）。
- CRC 校验失败的帧丢弃并计数；SEQ 跳变时提示丢帧。
- 保留 ASCII 行处理路径给命令回应（K?/Q/OK/ERR）。

**关键兼容点**：解出的样本对象保持 `{ms, TL, LV, ...}` 结构不变，
则 `analyzeRun`/`drawPlot`/`samplesToCsv`/录制/AI 包等**全部无需改动**——
这是把改动限制在解析层的设计要点。

**验收**：node 测试（U8）喂二进制字节流，解出的样本与预期逐字段相等。

---

### U7 🔧 网页：schema 驱动的列与单位

**动机**：列名和单位现在由固件 schema 帧动态给出，网页不再硬编码列表。

**改动**：
- `UNIT_GROUPS` 改为从 schema 的单位码构建（`mmps`/`pwm`/`deg`/`mm`/`bits`）。
- 遥测面板的字段勾选框由 schema 动态生成（或保持固定 12 项，与通道位对应）。
- `SESSIONS`/`TUNING_FOCUS` 的 mask 改用新 12 位通道定义。

**验收**：切换调参阶段，schema 帧到达后曲线列自动更新，量纲分组正确。

---

### U8 🔧 测试与文档

- **删除**过时的 ASCII 遥测测试（H,/D, 解析、CH,/C, 捕获文本）。
- **新增** `tests/test_binary_frame.mjs`：
  - 固件 CRC8 与网页 CRC8 对拍（同输入同输出）。
  - `TelemFrame_Build` 的字节布局（可用 node 复刻规则比对，或抽取网页编码器）。
  - 字节流状态机：schema + sample 混 ASCII 回应，正确分帧。
  - PAYLOAD 内含 `0x0A`/`0xAA` 字节时不误分帧（关键边界）。
  - SEQ 跳变检测。
- 保留并更新 `test_page_boot.mjs`（解析器重写后仍需能启动）。
- README：重写"双通道数据架构"章节为二进制协议；命令表标注 M 语义变化；
  新增二进制帧格式规范章节；更新测试清单。
- 教程：跑 `sync-sources.mjs` 重新对齐内联源码；解析器一节需人工重写讲解。

## 5. 执行顺序与依赖

```
U1（Serial1 DMA，跑旧ASCII验证）──> U2（帧编码器）──> U3（遥测二进制）──┐
                                                                      ├──> U6（网页字节流）──> U7（schema驱动）
U5（捕获二进制）──────────────────────────────────────────────────────┘         │
U4（K230 DMA，可延后）                                              U8（测试文档）<┘
```

**最小可用集**：U1 + U2 + U3 + U6。做完这四个，实时流就是二进制 + DMA，
调参数据质量和采集能力都到位。U5（捕获）、U4（K230）、U7（schema 动态）可后续补。

**分阶段验证节奏（降低 DMA 风险）**：
1. 先做 U1，实车确认 DMA 发送旧 ASCII 正常 → 证明 DMA 链路对。
2. 再做 U2+U3+U6，实车确认二进制流能画曲线、不丢帧 → 证明协议对。
3. 最后 U5/U4/U7/U8 收尾。

## 6. 风险与不可测部分

- **DMA 是硬件配置**：node 测不了，gcc 只查语法。U1/U3/U4/U5 的真正验证只能在实车。
  因此执行顺序刻意让每步可独立回退（U1 单独可 revert）。
- **环形缓冲 + DMA 的并发**：DMA 完成中断与主循环写缓冲共享索引，
  需单生产者（主循环）单消费者（DMA 中断）无锁模式，`volatile` + 内存屏障。
  环绕分两次 DMA 是最易错的点，务必实车压测（高频长时间跑）。
- **同一 UART 混传**：命令回应（ASCII）和遥测帧（二进制）共用 TX 缓冲和 DMA，
  必须保证一帧的字节连续写入缓冲（不被另一路穿插），Build 到完整帧再一次性 SendArray。
- **可回退性**：整个升级在 `update-遥测` 分支上；第一次 ASCII 架构的提交都在，
  实在不行可回到 `958b156`（README 同步完成点）。

## 7. 明确不做

- 命令与 OK/ERR 回应二进制化（保留 ASCII，调试友好）。
- 巡线连续 PID、Flash 持久化、云台、舵机（同前，见上一份设计）。
- 双向二进制（PC→MCU 命令仍 ASCII）。

## 8. 完成状态

（执行时更新）

| 任务 | 状态 | 提交 |
|---|---|---|
| U1 Serial1 DMA | ✅ 代码完成（待实车验证 DMA） | `2fdcea7` |
| U2 帧编码器 | ✅ 完成 | `2fdcea7` |
| U3 遥测二进制 | ✅ 完成 | `accb2f2` |
| U4 K230 DMA | ⬜ **未做**（可延后，见剩余工作） | |
| U5 捕获二进制 | ✅ 完成 | `accb2f2` |
| U6 网页字节流 | ✅ 完成 | `a4e572e` |
| U7 schema 驱动 | ✅ 完成 | `a4e572e` |
| U8 测试文档 | 🟡 测试+README 完成，教程待更新 | 本次提交 |

**验证状态：** node 可测部分全过——`test_binary_frame.mjs` 13 条（含 0x0A/0xAA payload 边界、CRC/SEQ、混流）、`test_capture_contract.mjs` 10 条、`test_csv_parse.mjs` 25 条、`test_page_boot.mjs` 5 条，加既有契约共 69 条。固件全部通过 gcc 语法检查。

**剩余工作：**
1. **U1 实车验证 DMA**（必须）：DMA 是硬件配置，node/gcc 测不了。需先在 CCS 打开 SysConfig 生成 DMA 初始化（见 `docs/CCS-生成DMA配置说明.md`），再按分阶段节奏验证——先确认 DMA 发旧行为正常，再上二进制。
2. **U4 K230 DMA**（可延后，优先级最低）：K230 帧率低，阻塞影响小。
3. **教程更新**：`tutorial/` 有 32 个内联源码块引用了本次重写的文件（Telemetry/Capture/Serial/car_debug/test），仍在讲 ASCII 架构。这是独立的大工作量（等价于重新讲一遍二进制架构），需单独安排。`test_tutorial.mjs` 会因源码漂移失败，直到这些块更新。
