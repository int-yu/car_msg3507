# 在 CCS 里生成 DMA 配置（U1 必需一步）

> 我改了 `main.syscfg` 的文本并写好了 `Serial.c` 的 DMA 发送逻辑，
> 但 DMA 的**初始化代码**（`SYSCFG_DL_DMA_init` 等）必须由 SysConfig 工具生成——
> 这一步我在命令行环境里做不了，需要你在 CCS 里点几下。

## 你需要做什么

### 方式 A：直接打开 SysConfig（推荐，最省事）

1. 在 CCS 里双击打开 `main.syscfg`（会进 SysConfig 图形界面）。
2. SysConfig 会**自动读取**我已经写进 `main.syscfg` 的 DMA 配置——
   左侧 UART → BLUETOOTH_UART 下应能看到已启用 `Enable DMA TX`，
   并挂了一个名为 `DMA_BLUETOOTH_TX` 的 DMA 通道。
3. **不用改任何东西**，直接按 `Ctrl+S` 保存。SysConfig 会重新生成
   `Debug/ti_msp_dl_config.c` 和 `.h`，其中包含 `SYSCFG_DL_DMA_init()` 和
   `DMA_BLUETOOTH_TX_CHAN_ID` 等符号。
4. 右键工程 → **Clean Project**，再 **Build Project**。
5. 若编译报缺 `SYSCFG_DL_DMA_init`，检查 `main.c` 之外是否有地方需要调用它——
   通常 SysConfig 会把它放进 `SYSCFG_DL_init()` 里自动调用，无需手动加。

### 如果 SysConfig 打不开或报错

说明我写的 `main.syscfg` DMA 字段与你的 SDK 版本键名有出入。手动配置：

1. SysConfig 里选 **UART → BLUETOOTH_UART**。
2. 勾选 **Enable DMA TX**（Enable DMA Transmit）。
3. **Enabled Interrupts** 里勾上 **DMA Done TX**（保留原有的 RX）。
4. 在 **DMA TX Trigger** 选 `DL_UART_DMA_INTERRUPT_TX`。
5. 展开新出现的 DMA 通道，命名为 `DMA_BLUETOOTH_TX`，
   Address Mode 选 **b2f**（内存到外设），src/dst Length 都选 **BYTE**。
6. 保存、Clean、Build。

## 验证 DMA 是否真的工作（U1 验收）

烧录后：

1. 打开 `car_debug.html` 连接串口，此时固件**仍在发旧的 ASCII 遥测**
   （U1 只改发送机制，不改协议——这是故意的，方便对照）。
2. 发 `M63` `G50`，看曲线正常滚动、终端无乱码。
3. **关键对比**：跑一次 `F1000`，看 OLED 上运动是否顺畅。
   - DMA 版：主循环不再被发送阻塞，运动应更平滑。
   - 若出现乱码、卡死或发送停止，说明 DMA 环形缓冲有问题，回退这步
     （`git revert` U1 的提交）再排查。
4. 高频压测：`M1` `G90`（只发 yaw，高频），长时间跑，确认不丢字节、不卡死。

## 我改了哪些文件（供你对照）

- `main.syscfg`：UART1 加了 `enableDMATX`、`DMA_CHANNEL_TX`、`DMA_DONE_TX` 中断。
- `Hardware/Comms/Serial.c/.h`：TX 环形缓冲 + DMA 搬运 + 完成中断推进 + 环绕分两次。
  发送接口（`Serial1_SendArray/SendString/SendByte/Printf`）签名不变，
  上层代码全部无需改动。

## 下一步

U1 实车验证通过后，告诉我，我再把遥测协议切成二进制（U3）——
那时 `car_debug.html` 也要同步换成二进制解析（U6），两者必须一起烧/一起用。
在 U3+U6 完成并一起烧录前，**当前网页仍按 ASCII 工作**，不受 U1 影响。
