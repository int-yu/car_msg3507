#include "Application/Comms/BluetoothDebug.h"
#include "Application/Debug/Telemetry.h"
#include "Application/Servo/Servo.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Motor/Motor.h"
#include "Hardware/Motor/PWM.h"
#include <stddef.h>

#define BLUETOOTH_VALUE_ACCUMULATOR_MAX 1000000L

typedef struct
{
    char command;
    int32_t magnitude;
    uint8_t hasCommand;
    uint8_t hasDigits;
    uint8_t isNegative;
    uint8_t idleTicks;
} BluetoothParser_t;

static BluetoothParser_t s_parser;
static int16_t s_leftCommand;
static int16_t s_rightCommand;
static uint8_t s_manualMotorEnabled;
static uint8_t s_signalPending;
static uint8_t s_pendingSignal;

static uint8_t BluetoothDebug_IsCommand(char value)
{
    return ((value == 'L') || (value == 'R') || (value == 'U') ||
            (value == 'C') ||
            (value == 'O') || (value == 'D') ||
            (value == 'G') || (value == 'M')) ? 1U : 0U;
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
}

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

static void BluetoothDebug_ExecuteCommand(void)
{
    int32_t value;

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
            /* 改掩码可能触发自动降频（字段增多行变长，安全上限下降），
               因此成功时同时回报新频率，用户能立即看到是否被限速。 */
            if ((s_parser.isNegative != 0U) ||
                (value > (int32_t)TELEMETRY_FIELD_ALL) ||
                (Telemetry_SetFieldMask((uint8_t)value) == 0U))
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
}

static void BluetoothDebug_ProcessByte(uint8_t byte)
{
    char value = BluetoothDebug_ToUpper((char)byte);

    if (BluetoothDebug_IsCommand(value) != 0U)
    {
        BluetoothDebug_StartCommand(value);
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
    Motor_StopAll();
    Serial1_SendString(
        "READY: C task, L/R/U motor, O vertical, D horizontal\r\n");
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

    if (s_parser.hasCommand == 0U)
    {
        return;
    }

    if (receivedByte != 0U)
    {
        s_parser.idleTicks = 0U;
        return;
    }

    if ((uint16_t)s_parser.idleTicks + elapsedTicks >=
        BLUETOOTH_COMMAND_IDLE_TICKS)
    {
        BluetoothDebug_ExecuteCommand();
    }
    else
    {
        s_parser.idleTicks = (uint8_t)(s_parser.idleTicks + elapsedTicks);
    }
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
