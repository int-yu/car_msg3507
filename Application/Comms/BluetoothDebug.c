#include "Application/Comms/BluetoothDebug.h"
#include "Application/Comms/CarLink.h"
#include "Application/Comms/K230Link.h"
#include "Application/Core/CarRole.h"
#include "Application/Control/MotionManager.h"
#include "Application/Debug/Capture.h"
#include "Application/Debug/Param.h"
#include "Application/Debug/Telemetry.h"
#include "Application/Servo/Servo.h"
#include "Application/State/Heading.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Motor/Motor.h"
#include "Hardware/Motor/PWM.h"
#include <math.h>
#include <stddef.h>

#define BLUETOOTH_VALUE_ACCUMULATOR_MAX 1000000L

/* K 命令文本参数缓冲：最长形如 "16=-123.4567"，24 字节足够。 */
#define BLUETOOTH_TEXT_ARGS_SIZE 24U

typedef struct
{
    char command;
    int32_t magnitude;
    uint8_t hasCommand;
    uint8_t hasDigits;
    uint8_t isNegative;
    uint8_t idleTicks;
    char textArgs[BLUETOOTH_TEXT_ARGS_SIZE];
    uint8_t textLength;
    uint8_t textOverflow;
    uint8_t relayMode; /* 主机 '@' 前缀：正在收集要转发给从机的命令行 */
} BluetoothParser_t;

static BluetoothParser_t s_parser;
static int16_t s_leftCommand;
static int16_t s_rightCommand;
static uint8_t s_manualMotorEnabled;
static uint8_t s_signalPending;
static uint8_t s_pendingSignal;
static int16_t s_debugSpeedMMps;

static uint8_t BluetoothDebug_IsCommand(char value)
{
    return ((value == 'L') || (value == 'R') || (value == 'U') ||
            (value == 'C') ||
            (value == 'O') || (value == 'D') ||
            (value == 'G') || (value == 'M') ||
            (value == 'V') || (value == 'F') || (value == 'B') ||
            (value == 'T') || (value == 'A') || (value == 'Z') ||
            (value == 'P') ||
            (value == 'K') || (value == 'W') || (value == 'N') ||
            (value == 'E') || (value == 'Y') ||
            (value == 'X') || (value == 'Q') || (value == 'J')) ? 1U : 0U;
}

static uint8_t BluetoothDebug_IsTerminator(char value)
{
    return ((value == '\r') || (value == '\n') || (value == ' ') ||
            (value == ',') || (value == ';') || (value == '\t')) ? 1U : 0U;
}

static char BluetoothDebug_ToUpper(char value)
{
    if ((value >= 'a') && (value <= 'z'))
    {
        value = (char)(value - ('a' - 'A'));
    }
    return value;
}

static int16_t BluetoothDebug_ClampMotorCommand(int32_t value)
{
    if (value > (int32_t)PWM_MAX_DUTY)
    {
        return (int16_t)PWM_MAX_DUTY;
    }
    if (value < -(int32_t)PWM_MAX_DUTY)
    {
        return -(int16_t)PWM_MAX_DUTY;
    }
    return (int16_t)value;
}

static uint16_t BluetoothDebug_ClampServoAngle(int32_t value,
                                               uint16_t minimum,
                                               uint16_t maximum)
{
    if (value < (int32_t)minimum)
    {
        return minimum;
    }
    if (value > (int32_t)maximum)
    {
        return maximum;
    }
    return (uint16_t)value;
}

static void BluetoothDebug_ResetParser(void)
{
    s_parser.command = '\0';
    s_parser.magnitude = 0;
    s_parser.hasCommand = 0U;
    s_parser.hasDigits = 0U;
    s_parser.isNegative = 0U;
    s_parser.idleTicks = 0U;
    s_parser.textLength = 0U;
    s_parser.textOverflow = 0U;
    s_parser.relayMode = 0U;
}

#if CAR_IS_MASTER
/* '@' 转发收尾：把收集到的命令行打包成 CarLink RELAY_CMD 发给从机。 */
static void BluetoothDebug_FinalizeRelay(void)
{
    if (s_parser.textOverflow != 0U)
    {
        Serial1_SendString("ERR @ FORMAT\r\n");
    }
    else if (s_parser.textLength == 0U)
    {
        Serial1_SendString("ERR @ EMPTY\r\n");
    }
    else
    {
        s_parser.textArgs[s_parser.textLength] = '\0';
        if (CarLink_SendCommand(s_parser.textArgs) != 0U)
        {
            Serial1_Printf("OK @%s\r\n", s_parser.textArgs);
        }
        else
        {
            Serial1_SendString("ERR @ LINK\r\n");
        }
    }
    BluetoothDebug_ResetParser();
}
#endif

static void BluetoothDebug_SendMotorStatus(void)
{
    Serial1_Printf("OK L=%d R=%d\r\n", s_leftCommand, s_rightCommand);
}

static uint8_t BluetoothDebug_PublishSignal(uint8_t signal)
{
    if (signal == 0U)
    {
        /* C0 具有最高优先级，待 Mission 读取前不会被普通信号覆盖。 */
        s_pendingSignal = 0U;
        s_signalPending = 1U;
        return 1U;
    }

    if ((s_signalPending != 0U) && (s_pendingSignal == 0U))
    {
        return 0U;
    }

    /* 普通信号不排队；同一系统拍内只保留最后收到的一条。 */
    s_pendingSignal = signal;
    s_signalPending = 1U;
    return 1U;
}

/* 网页运动命令与按键任务共用运动层；忙时一律拒绝，由按键保持优先。 */
static uint8_t BluetoothDebug_RejectIfBusy(void)
{
    if (MotionManager_IsBusy() != 0U)
    {
        Serial1_SendString("ERR BUSY\r\n");
        return 1U;
    }
    return 0U;
}

static void BluetoothDebug_ReportMotionResult(MotionManager_Result_t result)
{
    if (result == MOTION_MANAGER_RESULT_OK)
    {
        /* 捕获在动作真正开始的这一刻起算，样本第一拍即阶跃起点。
         * 未 arm 时该调用无副作用。 */
        Capture_Trigger();
    }
    if (result == MOTION_MANAGER_RESULT_OK)
    {
        Serial1_SendString("OK\r\n");
    }
    else
    {
        Serial1_Printf("ERR MOTION %d\r\n", (int)result);
    }
}

/* 从文本游标读一个带符号十进制整数；累加限幅避免溢出。返回是否读到数字。 */
static uint8_t BluetoothDebug_ParseInt(const char **cursor, int32_t *out)
{
    const char *scan = *cursor;
    int32_t sign = 1;
    int32_t magnitude = 0;
    uint8_t hasDigit = 0U;

    if (*scan == '-')
    {
        sign = -1;
        scan++;
    }
    while ((*scan >= '0') && (*scan <= '9'))
    {
        if (magnitude < BLUETOOTH_VALUE_ACCUMULATOR_MAX)
        {
            magnitude = magnitude * 10L + (int32_t)(*scan - '0');
            if (magnitude > BLUETOOTH_VALUE_ACCUMULATOR_MAX)
            {
                magnitude = BLUETOOTH_VALUE_ACCUMULATOR_MAX;
            }
        }
        hasDigit = 1U;
        scan++;
    }
    *cursor = scan;
    *out = sign * magnitude;
    return hasDigit;
}

/* J 差速驾驶：文本参数形如 "<左>/<右>"（mm/s，可负）。0/0 直接释放退出驾驶。 */
static void BluetoothDebug_HandleDrive(const char *args)
{
    const char *cursor = args;
    int32_t leftMMps = 0;
    int32_t rightMMps = 0;

    if ((BluetoothDebug_ParseInt(&cursor, &leftMMps) == 0U) ||
        (*cursor != '/'))
    {
        Serial1_SendString("ERR J FORMAT\r\n");
        return;
    }
    cursor++;
    if ((BluetoothDebug_ParseInt(&cursor, &rightMMps) == 0U) ||
        (*cursor != '\0'))
    {
        Serial1_SendString("ERR J FORMAT\r\n");
        return;
    }
    if ((leftMMps > BLUETOOTH_DEBUG_MAX_SPEED_MMPS) ||
        (leftMMps < -BLUETOOTH_DEBUG_MAX_SPEED_MMPS) ||
        (rightMMps > BLUETOOTH_DEBUG_MAX_SPEED_MMPS) ||
        (rightMMps < -BLUETOOTH_DEBUG_MAX_SPEED_MMPS))
    {
        Serial1_SendString("ERR RANGE\r\n");
        return;
    }

    /* 只在空闲或已处于驾驶模式时接受，绝不打断按键任务或其他运动。 */
    if ((MotionManager_IsBusy() != 0U) &&
        (MotionManager_GetMode() != MOTION_MANAGER_MODE_DRIVE))
    {
        Serial1_SendString("ERR BUSY\r\n");
        return;
    }

    if ((leftMMps == 0) && (rightMMps == 0))
    {
        /* 松开所有方向键：退出驾驶模式并释放电机。 */
        MotionManager_Stop();
        Serial1_SendString("OK J=0/0\r\n");
        return;
    }

    if (MotionManager_SetDrive((float)leftMMps, (float)rightMMps) ==
        MOTION_MANAGER_RESULT_OK)
    {
        Serial1_Printf("OK J=%d/%d\r\n", (int)leftMMps, (int)rightMMps);
    }
    else
    {
        Serial1_SendString("ERR J MOTION\r\n");
    }
}

static void BluetoothDebug_ExecuteCommand(void)
{
    int32_t value;

    /* K 使用文本参数（可含小数与 '?'），不适用 hasDigits 检查。 */
    if ((s_parser.hasCommand != 0U) && (s_parser.command == 'K'))
    {
        if (s_parser.textOverflow != 0U)
        {
            Serial1_SendString("ERR K FORMAT\r\n");
        }
        else
        {
            s_parser.textArgs[s_parser.textLength] = '\0';
            Param_HandleCommand(s_parser.textArgs);
        }
        BluetoothDebug_ResetParser();
        return;
    }

    /* J 与 K 一样走文本参数（含 '/' 分隔与负号），不适用 hasDigits 检查。 */
    if ((s_parser.hasCommand != 0U) && (s_parser.command == 'J'))
    {
        if (s_parser.textOverflow != 0U)
        {
            Serial1_SendString("ERR J FORMAT\r\n");
        }
        else
        {
            s_parser.textArgs[s_parser.textLength] = '\0';
            BluetoothDebug_HandleDrive(s_parser.textArgs);
        }
        BluetoothDebug_ResetParser();
        return;
    }

    /* Q 是纯查询，写成裸 "Q" 最自然，不该强迫用户补一个无意义的数字。 */
    if ((s_parser.hasCommand != 0U) && (s_parser.command == 'Q'))
    {
        s_parser.magnitude = 0;
        s_parser.hasDigits = 1U;
    }

    if ((s_parser.hasCommand == 0U) || (s_parser.hasDigits == 0U))
    {
        Serial1_SendString("ERR FORMAT\r\n");
        BluetoothDebug_ResetParser();
        return;
    }

    value = s_parser.isNegative ? -s_parser.magnitude : s_parser.magnitude;

    switch (s_parser.command)
    {
        case 'L':
            if (s_manualMotorEnabled == 0U)
            {
                Serial1_SendString("ERR BUSY\r\n");
                break;
            }
            s_leftCommand = BluetoothDebug_ClampMotorCommand(value);
            Motor_SetLeftPWM(s_leftCommand);
            BluetoothDebug_SendMotorStatus();
            break;

        case 'R':
            if (s_manualMotorEnabled == 0U)
            {
                Serial1_SendString("ERR BUSY\r\n");
                break;
            }
            s_rightCommand = BluetoothDebug_ClampMotorCommand(value);
            Motor_SetRightPWM(s_rightCommand);
            BluetoothDebug_SendMotorStatus();
            break;

        case 'U':
            if (s_manualMotorEnabled == 0U)
            {
                Serial1_SendString("ERR BUSY\r\n");
                break;
            }
            s_leftCommand = BluetoothDebug_ClampMotorCommand(value);
            s_rightCommand = s_leftCommand;
            Motor_SetPWM(s_leftCommand, s_rightCommand);
            BluetoothDebug_SendMotorStatus();
            break;

        case 'C':
            if ((s_parser.isNegative != 0U) ||
                (s_parser.magnitude > (int32_t)BLUETOOTH_TASK_SIGNAL_MAX))
            {
                Serial1_SendString("ERR RANGE\r\n");
            }
            else
            {
                if (BluetoothDebug_PublishSignal(
                        (uint8_t)s_parser.magnitude) != 0U)
                {
                    Serial1_Printf("OK C=%u\r\n",
                                   (unsigned)s_parser.magnitude);
                }
                else
                {
                    Serial1_SendString("ERR STOP PENDING\r\n");
                }
            }
            break;

        case 'O':
            Servo_SetVerticalAngle(BluetoothDebug_ClampServoAngle(
                value, SERVO_VERTICAL_MIN_ANGLE, SERVO_VERTICAL_MAX_ANGLE));
            Serial1_Printf("OK O=%u\r\n",
                           (unsigned)Servo_GetVerticalAngle());
            break;

        case 'D':
            Servo_SetHorizontalAngle(BluetoothDebug_ClampServoAngle(
                value, SERVO_HORIZONTAL_MIN_ANGLE, SERVO_HORIZONTAL_MAX_ANGLE));
            Serial1_Printf("OK D=%u\r\n",
                           (unsigned)Servo_GetHorizontalAngle());
            break;

        case 'G':
            /* 范围检查必须在 int32_t 上做：(uint8_t)256 == 0，而 G0 是合法值（关闭遥测），
               只靠被调函数的 uint8_t 参数检查会让 G256 被静默当成 G0 执行。
               超限时带上当前安全上限，便于用户知道当前掩码下到底能设多少。 */
            if ((s_parser.isNegative != 0U) ||
                (value > (int32_t)TELEMETRY_RATE_HARD_LIMIT_HZ) ||
                (Telemetry_SetRateHz((uint8_t)value) == 0U))
            {
                Serial1_Printf("ERR RANGE MAX=%u\r\n",
                               (unsigned)Telemetry_GetMaxRateHz());
            }
            else
            {
                Serial1_Printf("OK G=%u\r\n",
                               (unsigned)Telemetry_GetRateHz());
            }
            break;

        case 'M':
            /* M 现在设置二进制遥测的通道掩码（见 Telemetry.h TELEMETRY_CH_*）。
               改掩码可能触发自动降频（帧变长，上限下降），成功时同时回报新频率。 */
            if ((s_parser.isNegative != 0U) ||
                (value > (int32_t)TELEMETRY_CH_ALL) ||
                (Telemetry_SetFieldMask((uint16_t)value) == 0U))
            {
                Serial1_SendString("ERR RANGE\r\n");
            }
            else
            {
                Serial1_Printf("OK M=%u G=%u\r\n",
                               (unsigned)Telemetry_GetFieldMask(),
                               (unsigned)Telemetry_GetRateHz());
            }
            break;

        case 'V':
            if ((s_parser.isNegative != 0U) ||
                (value < BLUETOOTH_DEBUG_MIN_SPEED_MMPS) ||
                (value > BLUETOOTH_DEBUG_MAX_SPEED_MMPS))
            {
                Serial1_SendString("ERR RANGE\r\n");
            }
            else
            {
                s_debugSpeedMMps = (int16_t)value;
                Serial1_Printf("OK V=%d\r\n", (int)s_debugSpeedMMps);
            }
            break;

        case 'F':
            if (BluetoothDebug_RejectIfBusy() != 0U)
            {
                break;
            }
            if ((s_parser.isNegative != 0U) || (value <= 0) ||
                (value > BLUETOOTH_DEBUG_MAX_DISTANCE_MM))
            {
                Serial1_SendString("ERR RANGE\r\n");
                break;
            }
            BluetoothDebug_ReportMotionResult(MotionManager_StartForward(
                (uint32_t)value, (float)s_debugSpeedMMps, 0.0f));
            break;

        case 'B':
            if (BluetoothDebug_RejectIfBusy() != 0U)
            {
                break;
            }
            if ((s_parser.isNegative != 0U) || (value <= 0) ||
                (value > BLUETOOTH_DEBUG_MAX_DISTANCE_MM))
            {
                Serial1_SendString("ERR RANGE\r\n");
                break;
            }
            BluetoothDebug_ReportMotionResult(MotionManager_StartBackward(
                (uint32_t)value, (float)s_debugSpeedMMps, 0.0f));
            break;

        case 'T':
            if (BluetoothDebug_RejectIfBusy() != 0U)
            {
                break;
            }
            if ((value < -BLUETOOTH_DEBUG_MAX_ANGLE_DEG) ||
                (value > BLUETOOTH_DEBUG_MAX_ANGLE_DEG))
            {
                Serial1_SendString("ERR RANGE\r\n");
                break;
            }
            BluetoothDebug_ReportMotionResult(MotionManager_TurnBy(
                (float)value, (float)s_debugSpeedMMps));
            break;

        case 'A':
            if (BluetoothDebug_RejectIfBusy() != 0U)
            {
                break;
            }
            if ((value < -BLUETOOTH_DEBUG_MAX_ANGLE_DEG) ||
                (value > BLUETOOTH_DEBUG_MAX_ANGLE_DEG))
            {
                Serial1_SendString("ERR RANGE\r\n");
                break;
            }
            BluetoothDebug_ReportMotionResult(MotionManager_TurnTo(
                (float)value, (float)s_debugSpeedMMps));
            break;

        case 'Z':
            /* 运动期间清零或重新采样都会改掉航向基准，必须拒绝。 */
            if (BluetoothDebug_RejectIfBusy() != 0U)
            {
                break;
            }
            if (value == 1)
            {
                /* Z1 只改角度基准，不重新估计陀螺仪零漂。 */
                Heading_SetYaw(0.0f);
                Serial1_SendString("OK Z=1\r\n");
            }
            else if (value == 2)
            {
                /* Z2 会阻塞约 0.8 秒；此期间车辆必须完全静止。 */
                if (Heading_IsReady() == 0U)
                {
                    Serial1_SendString("ERR Z OFFLINE\r\n");
                    break;
                }
                if (Heading_IsScaleCalibActive() != 0U)
                {
                    Serial1_SendString("ERR Z CALIBRATING\r\n");
                    break;
                }

                Heading_Calibrate();
                if (Heading_IsReady() != 0U)
                {
                    Serial1_SendString("OK Z=2\r\n");
                }
                else
                {
                    Serial1_SendString("ERR Z OFFLINE\r\n");
                }
            }
            else
            {
                Serial1_SendString("ERR RANGE\r\n");
            }
            break;

        case 'W':
        {
            MotionManager_Result_t result;

            if (value == 0)
            {
                /* W0 只停恒速模式，不打断按键任务启动的其他运动。 */
                MotionManager_Mode_t mode = MotionManager_GetMode();

                if ((mode != MOTION_MANAGER_MODE_SPEED) &&
                    (mode != MOTION_MANAGER_MODE_IDLE))
                {
                    Serial1_SendString("ERR BUSY\r\n");
                    break;
                }
                MotionManager_Stop();
                Serial1_SendString("OK W=0\r\n");
                break;
            }
            if ((value < -BLUETOOTH_DEBUG_MAX_SPEED_MMPS) ||
                (value > BLUETOOTH_DEBUG_MAX_SPEED_MMPS))
            {
                Serial1_SendString("ERR RANGE\r\n");
                break;
            }
            if ((MotionManager_IsBusy() != 0U) &&
                (MotionManager_GetMode() != MOTION_MANAGER_MODE_SPEED))
            {
                Serial1_SendString("ERR BUSY\r\n");
                break;
            }
            result = MotionManager_StartSpeed((float)value);
            if (result == MOTION_MANAGER_RESULT_OK)
            {
                Capture_Trigger();
                Serial1_Printf("OK W=%d\r\n", (int)value);
            }
            else
            {
                Serial1_Printf("ERR MOTION %d\r\n", (int)result);
            }
            break;
        }

        case 'N':
        {
            MotionManager_Result_t result;

            if (value == 0)
            {
                /* N0 只停巡线模式，与 W0 语义一致。 */
                MotionManager_Mode_t mode = MotionManager_GetMode();

                if ((mode != MOTION_MANAGER_MODE_LINE) &&
                    (mode != MOTION_MANAGER_MODE_IDLE))
                {
                    Serial1_SendString("ERR BUSY\r\n");
                    break;
                }
                MotionManager_Stop();
                Serial1_SendString("OK N=0\r\n");
                break;
            }
            if ((s_parser.isNegative != 0U) ||
                (value < BLUETOOTH_DEBUG_MIN_SPEED_MMPS) ||
                (value > BLUETOOTH_DEBUG_MAX_SPEED_MMPS))
            {
                Serial1_SendString("ERR RANGE\r\n");
                break;
            }
            if ((MotionManager_IsBusy() != 0U) &&
                (MotionManager_GetMode() != MOTION_MANAGER_MODE_LINE))
            {
                Serial1_SendString("ERR BUSY\r\n");
                break;
            }
            result = MotionManager_StartLine((float)value);
            if (result == MOTION_MANAGER_RESULT_OK)
            {
                Capture_Trigger();
                Serial1_Printf("OK N=%d\r\n", (int)value);
            }
            else
            {
                Serial1_Printf("ERR MOTION %d\r\n", (int)result);
            }
            break;
        }

        case 'E':
            if ((s_parser.isNegative != 0U) || (value > 1))
            {
                Serial1_SendString("ERR RANGE\r\n");
                break;
            }
            if (value == 0)
            {
                Heading_ScaleCalibCancel();
                Serial1_SendString("OK E=0\r\n");
                break;
            }
            /* 标定要求手动原地旋转，自动运动期间的角度会污染积分。 */
            if (BluetoothDebug_RejectIfBusy() != 0U)
            {
                break;
            }
            if (Heading_IsReady() == 0U)
            {
                Serial1_SendString("ERR CAL OFFLINE\r\n");
                break;
            }
            Heading_ScaleCalibStart();
            Serial1_SendString("OK E=1\r\n");
            break;

        case 'Y':
            if ((s_parser.isNegative != 0U) || (value < 1) ||
                (value > 20))
            {
                Serial1_SendString("ERR RANGE\r\n");
                break;
            }
            if (Heading_IsScaleCalibActive() == 0U)
            {
                Serial1_SendString("ERR CAL IDLE\r\n");
                break;
            }
            {
                float rawAngle = Heading_GetCalibAngle();

                /* 累计角过小说明没转起来，套用会得到荒谬的尺度因子。 */
                if (fabsf(rawAngle) <= 1.0f)
                {
                    Heading_ScaleCalibCancel();
                    Serial1_SendString("ERR CAL SMALL\r\n");
                    break;
                }
                Serial1_Printf(
                    "OK Y SCALE=%.4f RAW=%.1f\r\n",
                    (double)Heading_ScaleCalibFinish((uint16_t)value),
                    (double)rawAngle);
            }
            break;

        case 'X':
            /* X0 停止并整体输出；X<mask> 选通道并等待下一条运动命令触发。 */
            if (s_parser.isNegative != 0U)
            {
                Serial1_SendString("ERR RANGE\r\n");
                break;
            }
            if (value == 0)
            {
                uint16_t count;

                Capture_Stop();
                count = Capture_StartDump();
                if (count == 0U)
                {
                    Serial1_SendString("ERR X EMPTY\r\n");
                    break;
                }
                Serial1_Printf("OK X DUMP=%u\r\n", (unsigned)count);
                break;
            }
            if ((value > (int32_t)TELEMETRY_CH_ALL) ||
                (Capture_Arm((uint16_t)value) == 0U))
            {
                /* 通道数超限和掩码非法都归到这里，附带上限便于自查。 */
                Serial1_Printf("ERR X MASK MAXCH=%u\r\n",
                               (unsigned)CAPTURE_MAX_CHANNELS);
                break;
            }
            Serial1_Printf("OK X ARM=%u CAP=%u\r\n",
                           (unsigned)Capture_GetChannelMask(),
                           (unsigned)Capture_GetCapacity());
            break;

        case 'Q':
            /* 上位机据此自适应频率，不再各存一份阈值。数值本身与 M/G 无关，
             * 任何参数都接受，一律回报当前状态。 */
            Serial1_Printf(
                "OK Q MAX=%u MASK=%u RATE=%u CAPST=%u CAPN=%u CAPMAX=%u\r\n",
                (unsigned)Telemetry_GetMaxRateHz(),
                (unsigned)Telemetry_GetFieldMask(),
                (unsigned)Telemetry_GetRateHz(),
                (unsigned)Capture_GetState(),
                (unsigned)Capture_GetSampleCount(),
                (unsigned)Capture_GetCapacity());
            break;

        case 'P':
            if ((s_parser.isNegative != 0U) || (value <= 0) ||
                (value > (int32_t)K230_LINK_CAPTURE_MAX_COUNT))
            {
                Serial1_SendString("ERR RANGE\r\n");
                break;
            }
            if (K230Link_IsReady() == 0U)
            {
                Serial1_SendString("ERR CAP NOLINK\r\n");
                break;
            }
            if (K230Link_RequestCapture((uint8_t)value) == 0U)
            {
                Serial1_SendString("ERR CAP BUSY\r\n");
                break;
            }
            Serial1_Printf("OK CAP REQ %d\r\n", (int)value);
            break;

        default:
            Serial1_SendString("ERR COMMAND\r\n");
            break;
    }

    BluetoothDebug_ResetParser();
}

static void BluetoothDebug_StartCommand(char command)
{
    if (s_parser.hasCommand != 0U)
    {
        BluetoothDebug_ExecuteCommand();
    }

    s_parser.command = command;
    s_parser.magnitude = 0;
    s_parser.hasCommand = 1U;
    s_parser.hasDigits = 0U;
    s_parser.isNegative = 0U;
    s_parser.idleTicks = 0U;
    s_parser.textLength = 0U;
    s_parser.textOverflow = 0U;
}

static void BluetoothDebug_ProcessByte(uint8_t byte)
{
    char value = BluetoothDebug_ToUpper((char)byte);

#if CAR_IS_MASTER
    /* '@' 前缀：把该行其余字符打包成 RELAY_CMD 转发给从机（网页控从机用）。
     * 例：网页发 "@J100/100" → 从机执行 "J100/100"，主机自身不动作。 */
    if (s_parser.relayMode != 0U)
    {
        if (BluetoothDebug_IsTerminator(value) != 0U)
        {
            BluetoothDebug_FinalizeRelay();
            return;
        }
        if (s_parser.textLength < (BLUETOOTH_TEXT_ARGS_SIZE - 1U))
        {
            s_parser.textArgs[s_parser.textLength] = value;
            s_parser.textLength++;
        }
        else
        {
            s_parser.textOverflow = 1U;
        }
        return;
    }
    if ((value == '@') && (s_parser.hasCommand == 0U))
    {
        BluetoothDebug_ResetParser();
        s_parser.relayMode = 1U;
        return;
    }
#endif

    if (BluetoothDebug_IsCommand(value) != 0U)
    {
        BluetoothDebug_StartCommand(value);
        return;
    }

    /* K 命令走独立的文本参数收集：值可以是小数，还要接受 '=' 和 '?'，
     * 不能让数字落进下面按整数累加的通用路径。 */
    if ((s_parser.hasCommand != 0U) && (s_parser.command == 'K'))
    {
        if (BluetoothDebug_IsTerminator(value) != 0U)
        {
            BluetoothDebug_ExecuteCommand();
            return;
        }
        if (((value >= '0') && (value <= '9')) ||
            (value == '.') || (value == '-') ||
            (value == '=') || (value == '?'))
        {
            if (s_parser.textLength < (BLUETOOTH_TEXT_ARGS_SIZE - 1U))
            {
                s_parser.textArgs[s_parser.textLength] = value;
                s_parser.textLength++;
            }
            else
            {
                s_parser.textOverflow = 1U;
            }
            return;
        }
        BluetoothDebug_ResetParser();
        Serial1_SendString("ERR CHARACTER\r\n");
        return;
    }

    /* J 差速驾驶收集 "<左>/<右>" 文本：接受数字、负号与 '/' 分隔符。 */
    if ((s_parser.hasCommand != 0U) && (s_parser.command == 'J'))
    {
        if (BluetoothDebug_IsTerminator(value) != 0U)
        {
            BluetoothDebug_ExecuteCommand();
            return;
        }
        if (((value >= '0') && (value <= '9')) ||
            (value == '-') || (value == '/'))
        {
            if (s_parser.textLength < (BLUETOOTH_TEXT_ARGS_SIZE - 1U))
            {
                s_parser.textArgs[s_parser.textLength] = value;
                s_parser.textLength++;
            }
            else
            {
                s_parser.textOverflow = 1U;
            }
            return;
        }
        BluetoothDebug_ResetParser();
        Serial1_SendString("ERR CHARACTER\r\n");
        return;
    }

    if ((value >= '0') && (value <= '9'))
    {
        if (s_parser.hasCommand == 0U)
        {
            Serial1_SendString("ERR COMMAND\r\n");
            return;
        }

        if (s_parser.magnitude < BLUETOOTH_VALUE_ACCUMULATOR_MAX)
        {
            s_parser.magnitude = s_parser.magnitude * 10L +
                                 (int32_t)(value - '0');
            if (s_parser.magnitude > BLUETOOTH_VALUE_ACCUMULATOR_MAX)
            {
                s_parser.magnitude = BLUETOOTH_VALUE_ACCUMULATOR_MAX;
            }
        }
        s_parser.hasDigits = 1U;
        return;
    }

    if (((value == '-') || (value == '+')) &&
        (s_parser.hasCommand != 0U) && (s_parser.hasDigits == 0U))
    {
        s_parser.isNegative = (value == '-') ? 1U : 0U;
        return;
    }

    if ((value == '\r') || (value == '\n') || (value == ' ') ||
        (value == ',') || (value == ';') || (value == '\t'))
    {
        if (s_parser.hasCommand != 0U)
        {
            BluetoothDebug_ExecuteCommand();
        }
        return;
    }

    BluetoothDebug_ResetParser();
    Serial1_SendString("ERR CHARACTER\r\n");
}

void BluetoothDebug_Init(void)
{
    BluetoothDebug_ResetParser();
    s_leftCommand = 0;
    s_rightCommand = 0;
    s_manualMotorEnabled = 1U;
    s_signalPending = 0U;
    s_pendingSignal = 0U;
    s_debugSpeedMMps = BLUETOOTH_DEBUG_DEFAULT_SPEED_MMPS;
    Motor_StopAll();
    Serial1_SendString(
        "READY: C task, L/R/U pwm, W speed, J drive, N line, K param, "
        "O vertical, D horizontal\r\n");
}

void BluetoothDebug_Update(uint8_t elapsedTicks,
                           uint8_t manualMotorEnabled)
{
    uint8_t byte;
    uint8_t receivedByte = 0U;

    s_manualMotorEnabled = manualMotorEnabled;

    while (Serial1_ReadByte(&byte) != 0U)
    {
        receivedByte = 1U;
        BluetoothDebug_ProcessByte(byte);
    }

    {
        uint8_t pending = s_parser.hasCommand;
#if CAR_IS_MASTER
        pending = (uint8_t)(pending | s_parser.relayMode);
#endif
        if (pending == 0U)
        {
            return;
        }
    }

    if (receivedByte != 0U)
    {
        s_parser.idleTicks = 0U;
        return;
    }

    if ((uint16_t)s_parser.idleTicks + elapsedTicks >=
        BLUETOOTH_COMMAND_IDLE_TICKS)
    {
#if CAR_IS_MASTER
        if (s_parser.relayMode != 0U)
        {
            BluetoothDebug_FinalizeRelay();
        }
        else
#endif
        {
            BluetoothDebug_ExecuteCommand();
        }
    }
    else
    {
        s_parser.idleTicks = (uint8_t)(s_parser.idleTicks + elapsedTicks);
    }
}

/*
 * 把外部字节流（从机收到的 CarLink RELAY_CMD 载荷）喂进同一套命令解析器，
 * 从而复用全部现有命令词汇（J/W/F/T/L/R...）。收尾补一个换行冲刷末条命令。
 * 从机执行后的 OK/ERR 回应发往自身 Serial1（未接线，直接丢弃）。
 */
void BluetoothDebug_FeedExternal(const uint8_t *data, uint16_t length)
{
    uint16_t index;

    if (data == NULL)
    {
        return;
    }
    for (index = 0U; index < length; index++)
    {
        BluetoothDebug_ProcessByte(data[index]);
    }
    BluetoothDebug_ProcessByte((uint8_t)'\n');
}

uint8_t BluetoothDebug_PopSignal(uint8_t *signal)
{
    if (signal == NULL)
    {
        return 0U;
    }

    if (s_signalPending == 0U)
    {
        return 0U;
    }

    *signal = s_pendingSignal;
    s_signalPending = 0U;
    return 1U;
}

int16_t BluetoothDebug_GetLeftCommand(void)
{
    return s_leftCommand;
}

int16_t BluetoothDebug_GetRightCommand(void)
{
    return s_rightCommand;
}
