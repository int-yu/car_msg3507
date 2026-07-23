#include "Application/Core/App.h"
#include "Application/Comms/BluetoothDebug.h"
#include "Application/Comms/CarLink.h"
#include "Application/Comms/K230Link.h"
#include "Application/Core/CarRole.h"
#include "Application/Control/MotionManager.h"
#include "Application/Debug/DebugDisplay.h"
#include "Application/Debug/Capture.h"
#include "Application/Debug/Telemetry.h"
#include "Application/Servo/Servo.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Board/Beep.h"
#include "Hardware/Board/Key.h"
#include "Hardware/Board/LED.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Motor/Motor.h"
#include "Hardware/Sensors/Graydetect.h"
#include "System/Tick.h"
#include "ti_msp_dl_config.h"
#include <stddef.h>

/* KEY1 在按键位图中是 bit0（Mission 用它启动任务）；KEY2 是 bit1，
 * 在这里作为车载物理急停，与远端 C0 等价。 */
#define APP_STOP_KEY_MASK 0x02U

static uint8_t s_previousKeyMask;
static MotionManager_Error_t s_previousMotionError;

static void App_ReportMotionError(void)
{
    MotionManager_Error_t error = MotionManager_GetError();

    if ((error != MOTION_MANAGER_ERROR_NONE) &&
        (error != s_previousMotionError))
    {
        Beep_Long();
    }
    s_previousMotionError = error;
}

/*
 * 处理从 CarLink 收到的一条对端消息。加新消息类型时在这里加一个 case 即可。
 * 主机、从机各管自己关心的类型，用 CAR_IS_MASTER/SLAVE 区分。
 */
static void App_HandlePeerMessage(const CarLink_Message_t *msg)
{
#if CAR_IS_SLAVE
    switch (msg->type)
    {
        case CAR_LINK_MSG_RELAY_CMD:
        {
            /* 喂进本地命令解析器执行（复用全部命令），并捕获它的 OK/ERR 回应，
             * 打包成 RELAY_REPLY 回传给主机（主机再转给网页）。回应超 32 字节
             * 会被截断——运动命令的回应都很短，够用。 */
            uint8_t reply[CAR_LINK_MAX_PAYLOAD];
            uint16_t replyLen;

            Serial1_BeginCapture(reply, (uint16_t)sizeof(reply));
            BluetoothDebug_FeedExternal(msg->payload, msg->length);
            replyLen = Serial1_EndCapture();
            if (replyLen > 0U)
            {
                (void)CarLink_Send(CAR_LINK_MSG_RELAY_REPLY, reply,
                                   (uint8_t)replyLen);
            }
            break;
        }

        case CAR_LINK_MSG_EVENT:
        {
            /* TODO 备赛：主机满足条件时同步来的事件在此响应。
             * 示例：事件号 0（等价 C0 全局停车）→ 停车；其它 → 鸣笛提示。 */
            uint8_t eventId = (msg->length >= 1U) ? msg->payload[0] : 0U;

            if (eventId == 0U)
            {
                MotionManager_Stop();
                Motor_StopAll();
            }
            else
            {
                Beep_Notify(1U);
            }
            (void)CarLink_Send(CAR_LINK_MSG_ACK, &eventId, 1U);
            break;
        }

        default:
            break;
    }
#else /* 主机：收从机主动发来的消息（回应 / ACK / 以后可扩展状态上报）。 */
    switch (msg->type)
    {
        case CAR_LINK_MSG_RELAY_REPLY:
            /* 从机命令执行回应：原样转给网页，加 SLV 前缀标明来自从机。
             * 载荷通常已含 "\r\n"，网页里独立成行。 */
            Serial1_SendString("SLV ");
            Serial1_SendArray(msg->payload, msg->length);
            break;

        case CAR_LINK_MSG_ACK:
            Serial1_Printf("PEER ACK %u\r\n",
                           (unsigned)((msg->length >= 1U) ?
                                      msg->payload[0] : 0U));
            break;

        default:
        {
            /* 通用兜底：从机以后新增任何消息类型，主机不改也能在网页看到
             * （类型 + 长度 + 载荷十六进制），信息不被静默吞掉。 */
            static const char digits[] = "0123456789ABCDEF";
            char hex[3U * CAR_LINK_MAX_PAYLOAD + 1U];
            uint8_t i;

            for (i = 0U; i < msg->length; i++)
            {
                hex[i * 3U] = digits[msg->payload[i] >> 4U];
                hex[i * 3U + 1U] = digits[(uint8_t)(msg->payload[i] & 0x0FU)];
                hex[i * 3U + 2U] = ' ';
            }
            hex[msg->length * 3U] = '\0';
            Serial1_Printf("PEER t=%u n=%u %s\r\n",
                           (unsigned)msg->type, (unsigned)msg->length, hex);
            break;
        }
    }
#endif
}

void App_Init(void)
{
    __disable_irq();

    SYSCFG_DL_init();
    Tick_Init();

    LED_Init();
    Beep_Init();
    Key_Init();
    Graydetect_Init();
    Motor_Init();
    Servo_Init();
    Serial1_Init();
    /* K230/F32C 暂时停用（UART2 已改接 HC05 主从链路）；代码保留，等挪串口再启用。 */
    /* K230Link_Init(); */
    CarLink_Init();
    Odometry_Init();

    DebugDisplay_Init();
    Heading_Init();
    DebugDisplay_ShowHeadingCalibration(Heading_IsReady());
    Heading_Calibrate();

    /* 标定期间全局中断保持关闭，从正式流程的零时刻重新开始计数。 */
    Tick_Init();
    Odometry_Reset();

    BluetoothDebug_Init();
    Telemetry_Init();
    Capture_Init();
    if (MotionManager_Init() != MOTION_MANAGER_RESULT_OK)
    {
        Beep_Long();
    }

    s_previousKeyMask = Key_GetPressedMask();
    s_previousMotionError = MotionManager_GetError();
    DebugDisplay_Update(DEBUG_DISPLAY_REFRESH_TICKS);
}

uint8_t App_Update(App_UpdateContext_t *context)
{
    uint8_t index;
    uint8_t elapsedTicks;
    uint8_t keyMask;

    if (context == NULL)
    {
        return 0U;
    }

    elapsedTicks = Tick_PollCount();
    if (elapsedTicks == 0U)
    {
        __WFI();
        return 0U;
    }

    context->elapsedTicks = elapsedTicks;
    context->dt = (float)elapsedTicks * TICK_DT;
    context->hasBluetoothSignal = 0U;
    context->bluetoothSignal = 0U;

    Heading_Update(context->dt);
    Odometry_Update(elapsedTicks);
    /* 云台 F32C 停用：UART2 现为 HC05 主从链路，勿在此启用 Gimbal 以免冲突。 */
    /* Gimbal_Update(context->dt); */

    keyMask = Key_GetPressedMask();
    context->pressedKeys = keyMask;
    context->pressedEdges =
        (uint8_t)(keyMask & (uint8_t)~s_previousKeyMask);
    s_previousKeyMask = keyMask;

    CarLink_Update(elapsedTicks);

    BluetoothDebug_Update(
        elapsedTicks, (MotionManager_IsBusy() == 0U) ? 1U : 0U);
    context->hasBluetoothSignal =
        BluetoothDebug_PopSignal(&context->bluetoothSignal);

    /* KEY2 车载物理急停：合成一条 C0 停车信号，让 App 与 Mission 走同一套
     * 全局停车 + 复位到等待的流程，效果与远端 C0 完全一致。 */
    if ((context->pressedEdges & APP_STOP_KEY_MASK) != 0U)
    {
        context->hasBluetoothSignal = 1U;
        context->bluetoothSignal = 0U;
        Beep_Notify(1U);
    }

    /* C0 是不受 Mission 可打断属性限制的全局停车信号。 */
    if ((context->hasBluetoothSignal != 0U) &&
        (context->bluetoothSignal == 0U))
    {
        MotionManager_Stop();
        Motor_StopAll();
        /* (void)Gimbal_Disable();  云台已停用 */
    }

#if CAR_IS_MASTER
    /* 示例「主机满足某条件 → 通知从机」：把本机收到的任务信号(C 命令/急停)
     * 作为事件同步给从机，实现主从一起启动/一起急停。备赛时把触发条件换成
     * 你真正需要的（比如越过某条线、检测到目标）即可，链路层不用动。 */
    if (context->hasBluetoothSignal != 0U)
    {
        (void)CarLink_SendEvent(context->bluetoothSignal, NULL, 0U);
    }
#endif

    /* 处理从机/主机通过 CarLink 收到的对端消息（含转发命令）。放在运动更新前，
     * 让本拍转发来的运动命令当拍生效。 */
    {
        CarLink_Message_t peerMsg;

        while (CarLink_PopMessage(&peerMsg) != 0U)
        {
            App_HandlePeerMessage(&peerMsg);
        }
    }

    MotionManager_Update(context->dt);
    App_ReportMotionError();

    /* K230 拍照 ACK 处理随 K230 一起停用（UART2 已改接主从链路）。
    {
        uint8_t captureOk;
        uint16_t captureIndex;

        if (K230Link_PopCaptureAck(&captureOk, &captureIndex) != 0U)
        {
            if (captureOk != 0U)
            {
                Serial1_Printf("OK CAP %u\r\n", (unsigned)captureIndex);
            }
            else
            {
                Serial1_SendString("ERR CAP FAIL\r\n");
            }
        }
    }
    */

    /* 必须在 MotionManager_Update() 之后：目标速度和输出 PWM 都是本拍算出的，
     * 提前采会记录到上一拍的值，阶跃起点会整体偏移一个控制周期。 */
    Capture_Update((uint32_t)elapsedTicks * 10U);

    Telemetry_Update(elapsedTicks, context->pressedKeys);
    /* dump 与实时流互斥：捕获输出期间车已停稳，让它独占串口。 */
    Capture_DumpNext();

    for (index = 0U; index < elapsedTicks; index++)
    {
        Beep_Tick();
    }

    DebugDisplay_Update(elapsedTicks);
    return 1U;
}
