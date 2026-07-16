#include "Application/Comms/BluetoothDebug.h"
#include "Application/Servo/Servo.h"
#include "Hardware/Comms/Serial.h"
#include "Hardware/Motor/Motor.h"
#include "Hardware/Motor/PWM.h"

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

static uint8_t BluetoothDebug_IsCommand(char value)
{
    return ((value == 'L') || (value == 'R') || (value == 'U') ||
            (value == 'O') || (value == 'D')) ? 1U : 0U;
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
            s_leftCommand = BluetoothDebug_ClampMotorCommand(value);
            Motor_SetLeftPWM(s_leftCommand);
            BluetoothDebug_SendMotorStatus();
            break;

        case 'R':
            s_rightCommand = BluetoothDebug_ClampMotorCommand(value);
            Motor_SetRightPWM(s_rightCommand);
            BluetoothDebug_SendMotorStatus();
            break;

        case 'U':
            s_leftCommand = BluetoothDebug_ClampMotorCommand(value);
            s_rightCommand = s_leftCommand;
            Motor_SetPWM(s_leftCommand, s_rightCommand);
            BluetoothDebug_SendMotorStatus();
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
    Motor_StopAll();
    Serial1_SendString("READY: L/R/U motor, O vertical, D horizontal\r\n");
}

void BluetoothDebug_Update(uint8_t elapsedTicks)
{
    uint8_t byte;
    uint8_t receivedByte = 0U;

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

int16_t BluetoothDebug_GetLeftCommand(void)
{
    return s_leftCommand;
}

int16_t BluetoothDebug_GetRightCommand(void)
{
    return s_rightCommand;
}
