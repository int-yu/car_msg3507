#ifndef APPLICATION_DEBUG_CAPTURE_H
#define APPLICATION_DEBUG_CAPTURE_H

#include <stdint.h>

/*
 * 板载高速捕获（通道 B）。
 *
 * 为什么需要它：主循环是 100 Hz，而 UART 实时流受阻塞发送限制只能跑到
 * 14~57 Hz，低于控制环频率。用低于控制环的采样率去测阶跃响应，超调峰值
 * 会落在采样点之间，算出来的指标不可信。本模块在捕获期间一个字节都不发
 * 串口，只把每个控制拍的数据写进 RAM，动作结束、车停稳之后再整体 dump，
 * 因此能拿到与控制环同频（100 Hz）且零阻塞的无损数据。
 *
 * 用法：Capture_Arm(掩码) 选通道并进入待触发 -> 运动命令自动 Capture_Trigger()
 * -> 每拍 Capture_Update() 写一帧 -> 动作结束后 Capture_StartDump() ->
 * 每拍 Capture_DumpNext() 吐若干行，直到 Capture_GetState() 回到 IDLE。
 */

/* 同时最多记录的通道数。4 通道 = 20 字节/样本 = 1024 样本 = 10.2 秒 @100 Hz。
 * 放宽这个值会按比例缩短可录时长，通道多到一定程度就该改用实时流了。 */
#define CAPTURE_MAX_CHANNELS 4U

/* 捕获缓冲字节数。SRAM 共 32 KB，当前固件仅用约 3.5 KB；取 20 KB 后
 * 仍为栈和后续功能留出约 9 KB 余量。 */
#define CAPTURE_BUFFER_BYTES 20480U

/* 通道位定义。位序即 dump 时的列序，一经发布不得重排，只能在尾部追加。 */
#define CAPTURE_CH_TL   0x0001U /* 左轮目标速度 mm/s */
#define CAPTURE_CH_LV   0x0002U /* 左轮实测速度 mm/s */
#define CAPTURE_CH_PL   0x0004U /* 左轮输出 PWM */
#define CAPTURE_CH_TR   0x0008U /* 右轮目标速度 mm/s */
#define CAPTURE_CH_RV   0x0010U /* 右轮实测速度 mm/s */
#define CAPTURE_CH_PR   0x0020U /* 右轮输出 PWM */
#define CAPTURE_CH_YAW  0x0040U /* 连续累计航向角 度 */
#define CAPTURE_CH_NAVE 0x0080U /* 转向角误差 度 */
#define CAPTURE_CH_LERR 0x0100U /* 巡线权重误差 */
#define CAPTURE_CH_ALL  0x01FFU

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
