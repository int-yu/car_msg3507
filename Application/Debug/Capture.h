#ifndef APPLICATION_DEBUG_CAPTURE_H
#define APPLICATION_DEBUG_CAPTURE_H

#include <stdint.h>

/*
 * 板载高速捕获（可选的超长/零丢包录制缓冲）。
 *
 * 二进制 + DMA 后，实时流本身就能以远超控制环的速率无损直传，因此捕获不再是
 * 调参的必需品，而是两种场景的补充：① 想录得比串口实时率更久（通道少时单通道
 * 可达 26 秒）；② 想要绝对零丢包（防 Web Serial 偶发丢包）。捕获期间一个字节都
 * 不发串口，动作结束、车停稳后再整体 dump（此时阻塞无害，且已 DMA 化）。
 *
 * 存储为可变长度二进制：每样本 = ms(u32) + 各选中通道 float32，通道数可变。
 * dump 发二进制帧 CAP_META / CAP_SAMPLE / CAP_END（见 TelemFrame.h）。
 * 通道定义与掩码语义与 Telemetry 完全共用（TELEMETRY_CH_*）。
 *
 * 用法：Capture_Arm(掩码) 选通道并进入待触发 -> 运动命令自动 Capture_Trigger()
 * -> 每拍 Capture_Update() 写一帧 -> 动作结束后 Capture_StartDump() ->
 * 每拍 Capture_DumpNext() 吐若干帧，直到 Capture_GetState() 回到 IDLE。
 */

/* 同时最多记录的通道数。与 Telemetry 的 12 通道一致；通道越少可录越久：
 * 单通道 8B/样本 -> 24KB 可存约 30 秒；6 通道约 7 秒。 */
#define CAPTURE_MAX_CHANNELS 8U

/* 捕获缓冲字节数。SRAM 32 KB，固件其余约 3.5 KB；取 24 KB 仍留约 4 KB 给栈。 */
#define CAPTURE_BUFFER_BYTES 24576U

typedef enum
{
    CAPTURE_STATE_IDLE = 0,
    CAPTURE_STATE_ARMED,   /* 已选通道，等待运动命令触发 */
    CAPTURE_STATE_RUNNING, /* 正在写缓冲 */
    CAPTURE_STATE_FULL,    /* 缓冲写满，已自动停止 */
    CAPTURE_STATE_DUMPING  /* 正在分批输出 */
} Capture_State_t;

void Capture_Init(void);

/* 选择通道并进入待触发。掩码为 0、通道数超过 CAPTURE_MAX_CHANNELS，
 * 或正在 dump 时返回 0。重新 Arm 会丢弃上一次的数据。 */
uint8_t Capture_Arm(uint16_t channelMask);

/* 由运动命令在启动成功后调用；仅在 ARMED 状态下生效，其余状态忽略。 */
void Capture_Trigger(void);

/* 每个 100 Hz 控制拍调用一次，写入一帧。必须放在 MotionManager_Update()
 * 之后，否则采到的是上一拍的目标值和输出。 */
void Capture_Update(uint32_t elapsedMs);

/* 停止记录但保留数据，可随后 dump。 */
void Capture_Stop(void);

/* 开始分批输出。返回待输出的样本数；无数据时返回 0 且不改变状态。 */
uint16_t Capture_StartDump(void);

/* 每拍调用一次，输出若干行。DUMPING 状态下才有动作，输出完毕自动回 IDLE。 */
void Capture_DumpNext(void);

Capture_State_t Capture_GetState(void);
uint16_t Capture_GetSampleCount(void);
uint16_t Capture_GetCapacity(void);   /* 当前通道数下可存的样本上限 */
uint16_t Capture_GetChannelMask(void);

#endif
