#include "Application/Comms/BluetoothDebug.h"
#include "Application/Control/MotionManager.h"
#include "Application/Servo/Servo.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Comms/Serial.h"
#include <math.h>
#include <string.h>

typedef enum
{
    BLUETOOTH_PARSER_WAIT_HEAD = 0,
    BLUETOOTH_PARSER_FIXED_FIELDS,
    BLUETOOTH_PARSER_PAYLOAD,
    BLUETOOTH_PARSER_CRC_LOW,
    BLUETOOTH_PARSER_CRC_HIGH,
    BLUETOOTH_PARSER_TAIL
} BluetoothParser_State_t;

typedef struct
{
    BluetoothParser_State_t state;
    uint8_t fixedFields[6];
    uint8_t fixedIndex;
    uint8_t payload[BLUETOOTH_DEBUG_MAX_PAYLOAD_LENGTH];
    uint16_t payloadLength;
    uint16_t payloadIndex;
    uint16_t calculatedCRC;
    uint16_t receivedCRC;
} BluetoothParser_t;

typedef struct
{
    int16_t joystickXMMps;
    int16_t joystickYMMps;
    int32_t forwardSpeedMMps;
    int32_t forwardDistanceMM;
    int16_t turnAngleDeg;
    uint8_t startForward;
    uint8_t startTurn;
} BluetoothControl_t;

static BluetoothParser_t s_parser;
static BluetoothControl_t s_control;
static uint8_t s_newControl;
static uint8_t s_forwardEdgePending;
static uint8_t s_turnEdgePending;
static uint8_t s_previousForwardButton;
static uint8_t s_previousTurnButton;
static uint8_t s_rearmRequired;
static uint8_t s_linkActive;
static uint8_t s_commandAllowed;
static uint8_t s_ownsMotion;
static uint16_t s_ticksSinceControl;
static uint16_t s_telemetryTicks;
static uint16_t s_txSequence;
static BluetoothDebug_Result_t s_lastResult;

static uint16_t BluetoothDebug_UpdateCRC(uint16_t crc, uint8_t data)
{
    uint8_t bit;

    crc ^= (uint16_t)data << 8;
    for (bit = 0U; bit < 8U; bit++)
    {
        crc = ((crc & 0x8000U) != 0U) ?
            (uint16_t)((crc << 1) ^ 0x1021U) :
            (uint16_t)(crc << 1);
    }
    return crc;
}

static uint16_t BluetoothDebug_ReadU16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t BluetoothDebug_ReadI16(const uint8_t *data)
{
    return (int16_t)BluetoothDebug_ReadU16(data);
}

static int32_t BluetoothDebug_ReadI32(const uint8_t *data)
{
    uint32_t value = (uint32_t)data[0] |
                     ((uint32_t)data[1] << 8) |
                     ((uint32_t)data[2] << 16) |
                     ((uint32_t)data[3] << 24);
    return (int32_t)value;
}

static void BluetoothDebug_WriteU16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)(value >> 8);
}

static void BluetoothDebug_WriteU32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)((value >> 8) & 0xFFU);
    data[2] = (uint8_t)((value >> 16) & 0xFFU);
    data[3] = (uint8_t)(value >> 24);
}

static void BluetoothDebug_WriteFloat(uint8_t *data, float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    BluetoothDebug_WriteU32(data, bits);
}

static void BluetoothDebug_ResetParser(void)
{
    s_parser.state = BLUETOOTH_PARSER_WAIT_HEAD;
    s_parser.fixedIndex = 0U;
    s_parser.payloadLength = 0U;
    s_parser.payloadIndex = 0U;
    s_parser.calculatedCRC = 0xFFFFU;
    s_parser.receivedCRC = 0U;
}

static void BluetoothDebug_RestartParser(uint8_t byte)
{
    BluetoothDebug_ResetParser();
    if (byte == BLUETOOTH_DEBUG_FRAME_HEAD)
    {
        s_parser.state = BLUETOOTH_PARSER_FIXED_FIELDS;
    }
}

static uint8_t BluetoothDebug_ButtonsAreValid(
    uint8_t startForward, uint8_t startTurn)
{
    return ((startForward <= 1U) && (startTurn <= 1U)) ? 1U : 0U;
}

static void BluetoothDebug_HandleControlPayload(const uint8_t *payload)
{
    BluetoothControl_t next;

    next.joystickXMMps = BluetoothDebug_ReadI16(&payload[0]);
    next.joystickYMMps = BluetoothDebug_ReadI16(&payload[2]);
    next.forwardSpeedMMps = BluetoothDebug_ReadI32(&payload[4]);
    next.forwardDistanceMM = BluetoothDebug_ReadI32(&payload[8]);
    next.turnAngleDeg = BluetoothDebug_ReadI16(&payload[12]);
    next.startForward = payload[14];
    next.startTurn = payload[15];

    if (BluetoothDebug_ButtonsAreValid(
            next.startForward, next.startTurn) == 0U)
    {
        s_lastResult = BLUETOOTH_DEBUG_RESULT_INVALID_PARAMETER;
        return;
    }

    /* 开始按钮只响应 0->1 上升沿，周期发送时不会重复触发动作。 */
    if ((next.startForward != 0U) &&
        (s_previousForwardButton == 0U))
    {
        s_forwardEdgePending = 1U;
    }
    if ((next.startTurn != 0U) &&
        (s_previousTurnButton == 0U))
    {
        s_turnEdgePending = 1U;
    }

    s_previousForwardButton = next.startForward;
    s_previousTurnButton = next.startTurn;
    s_ticksSinceControl = 0U;
    s_linkActive = 1U;
    s_telemetryTicks = BLUETOOTH_DEBUG_TELEMETRY_TICKS;
    s_control = next;
    s_newControl = 1U;
}

static void BluetoothDebug_HandleFrame(void)
{
    uint8_t version = s_parser.fixedFields[0];
    uint8_t messageId = s_parser.fixedFields[1];

    if ((version != BLUETOOTH_DEBUG_PROTOCOL_VERSION) ||
        (messageId != BLUETOOTH_DEBUG_MESSAGE_CONTROL) ||
        (s_parser.payloadLength !=
         BLUETOOTH_DEBUG_CONTROL_PAYLOAD_LENGTH))
    {
        return;
    }
    BluetoothDebug_HandleControlPayload(s_parser.payload);
}

static void BluetoothDebug_ProcessByte(uint8_t byte)
{
    switch (s_parser.state)
    {
        case BLUETOOTH_PARSER_WAIT_HEAD:
            if (byte == BLUETOOTH_DEBUG_FRAME_HEAD)
            {
                s_parser.state = BLUETOOTH_PARSER_FIXED_FIELDS;
                s_parser.calculatedCRC = 0xFFFFU;
                s_parser.fixedIndex = 0U;
            }
            break;

        case BLUETOOTH_PARSER_FIXED_FIELDS:
            s_parser.fixedFields[s_parser.fixedIndex++] = byte;
            s_parser.calculatedCRC = BluetoothDebug_UpdateCRC(
                s_parser.calculatedCRC, byte);
            if (s_parser.fixedIndex >= sizeof(s_parser.fixedFields))
            {
                s_parser.payloadLength = BluetoothDebug_ReadU16(
                    &s_parser.fixedFields[4]);
                s_parser.payloadIndex = 0U;
                if (s_parser.payloadLength >
                    BLUETOOTH_DEBUG_MAX_PAYLOAD_LENGTH)
                {
                    BluetoothDebug_RestartParser(byte);
                }
                else if (s_parser.payloadLength == 0U)
                {
                    s_parser.state = BLUETOOTH_PARSER_CRC_LOW;
                }
                else
                {
                    s_parser.state = BLUETOOTH_PARSER_PAYLOAD;
                }
            }
            break;

        case BLUETOOTH_PARSER_PAYLOAD:
            s_parser.payload[s_parser.payloadIndex++] = byte;
            s_parser.calculatedCRC = BluetoothDebug_UpdateCRC(
                s_parser.calculatedCRC, byte);
            if (s_parser.payloadIndex >= s_parser.payloadLength)
            {
                s_parser.state = BLUETOOTH_PARSER_CRC_LOW;
            }
            break;

        case BLUETOOTH_PARSER_CRC_LOW:
            s_parser.receivedCRC = byte;
            s_parser.state = BLUETOOTH_PARSER_CRC_HIGH;
            break;

        case BLUETOOTH_PARSER_CRC_HIGH:
            s_parser.receivedCRC |= (uint16_t)byte << 8;
            s_parser.state = BLUETOOTH_PARSER_TAIL;
            break;

        case BLUETOOTH_PARSER_TAIL:
            if ((byte == BLUETOOTH_DEBUG_FRAME_TAIL) &&
                (s_parser.receivedCRC == s_parser.calculatedCRC))
            {
                BluetoothDebug_HandleFrame();
                BluetoothDebug_ResetParser();
            }
            else
            {
                BluetoothDebug_RestartParser(byte);
            }
            break;

        default:
            BluetoothDebug_RestartParser(byte);
            break;
    }
}

static uint8_t BluetoothDebug_GetAction(void)
{
    switch (MotionManager_GetMode())
    {
        case MOTION_MANAGER_MODE_MANUAL:
            return 1U;
        case MOTION_MANAGER_MODE_STRAIGHT:
            return 2U;
        case MOTION_MANAGER_MODE_TURN:
            return 3U;
        case MOTION_MANAGER_MODE_BRAKE:
            return 4U;
        case MOTION_MANAGER_MODE_IDLE:
        case MOTION_MANAGER_MODE_LINE:
        default:
            return 0U;
    }
}

static uint8_t BluetoothDebug_AutomaticActionIsBusy(void)
{
    MotionManager_Mode_t mode = MotionManager_GetMode();

    if ((mode != MOTION_MANAGER_MODE_STRAIGHT) &&
        (mode != MOTION_MANAGER_MODE_TURN) &&
        (mode != MOTION_MANAGER_MODE_BRAKE))
    {
        return 0U;
    }
    return MotionManager_IsBusy();
}

static uint8_t BluetoothDebug_SendFrame(
    uint8_t messageId, const uint8_t *payload, uint16_t payloadLength)
{
    uint8_t frame[BLUETOOTH_DEBUG_TELEMETRY_PAYLOAD_LENGTH + 10U];
    uint16_t crc = 0xFFFFU;
    uint16_t index;
    uint16_t frameLength = (uint16_t)(payloadLength + 10U);

    if ((payload == NULL) ||
        (payloadLength > BLUETOOTH_DEBUG_TELEMETRY_PAYLOAD_LENGTH))
    {
        return 0U;
    }

    frame[0] = BLUETOOTH_DEBUG_FRAME_HEAD;
    frame[1] = BLUETOOTH_DEBUG_PROTOCOL_VERSION;
    frame[2] = messageId;
    BluetoothDebug_WriteU16(&frame[3], s_txSequence);
    BluetoothDebug_WriteU16(&frame[5], payloadLength);
    memcpy(&frame[7], payload, payloadLength);

    for (index = 1U; index < (uint16_t)(7U + payloadLength); index++)
    {
        crc = BluetoothDebug_UpdateCRC(crc, frame[index]);
    }
    BluetoothDebug_WriteU16(&frame[7U + payloadLength], crc);
    frame[9U + payloadLength] = BLUETOOTH_DEBUG_FRAME_TAIL;

    if (Serial1_QueueArray(frame, frameLength) == 0U)
    {
        return 0U;
    }
    s_txSequence++;
    return 1U;
}

static uint8_t BluetoothDebug_SendTelemetry(void)
{
    uint8_t payload[BLUETOOTH_DEBUG_TELEMETRY_PAYLOAD_LENGTH];

    BluetoothDebug_WriteFloat(&payload[0], Odometry_GetSpeedL());
    BluetoothDebug_WriteFloat(&payload[4], Odometry_GetSpeedR());
    BluetoothDebug_WriteFloat(&payload[8], Heading_GetYaw());
    BluetoothDebug_WriteU16(
        &payload[12], Servo_GetVerticalAngle());
    BluetoothDebug_WriteU16(
        &payload[14], Servo_GetHorizontalAngle());
    payload[16] = s_commandAllowed;
    payload[17] = BluetoothDebug_GetAction();
    payload[18] = BluetoothDebug_AutomaticActionIsBusy();
    payload[19] = (uint8_t)s_lastResult;
    payload[20] = (uint8_t)MotionManager_GetError();

    return BluetoothDebug_SendFrame(
        BLUETOOTH_DEBUG_MESSAGE_TELEMETRY,
        payload, sizeof(payload));
}

static float BluetoothDebug_ApplyDeadzone(float value)
{
    return (fabsf(value) <=
            BLUETOOTH_DEBUG_JOYSTICK_DEADZONE_MMPS) ? 0.0f : value;
}

static uint8_t BluetoothDebug_ControlHasMotionRequest(void)
{
    if ((s_forwardEdgePending != 0U) || (s_turnEdgePending != 0U))
    {
        return 1U;
    }
    if ((fabsf((float)s_control.joystickXMMps) >
         BLUETOOTH_DEBUG_JOYSTICK_DEADZONE_MMPS) ||
        (fabsf((float)s_control.joystickYMMps) >
         BLUETOOTH_DEBUG_JOYSTICK_DEADZONE_MMPS))
    {
        return 1U;
    }
    return 0U;
}

static void BluetoothDebug_UpdateJoystick(void)
{
    float x = BluetoothDebug_ApplyDeadzone(
        (float)s_control.joystickXMMps);
    float y = BluetoothDebug_ApplyDeadzone(
        (float)s_control.joystickYMMps);
    float left = y + x;
    float right = y - x;
    float largest = fmaxf(fabsf(left), fabsf(right));

    if (largest > BLUETOOTH_DEBUG_MANUAL_MAX_SPEED_MMPS)
    {
        float scale = BLUETOOTH_DEBUG_MANUAL_MAX_SPEED_MMPS / largest;
        left *= scale;
        right *= scale;
    }

    if (MotionManager_SetManualWheelSpeeds(left, right) !=
        MOTION_MANAGER_RESULT_OK)
    {
        s_lastResult = BLUETOOTH_DEBUG_RESULT_START_FAILED;
        s_ownsMotion = 0U;
        return;
    }

    s_ownsMotion = ((fabsf(left) > 0.001f) ||
                    (fabsf(right) > 0.001f)) ? 1U : 0U;
}

static void BluetoothDebug_StartRequestedAction(void)
{
    MotionManager_Result_t result;

    if ((s_forwardEdgePending != 0U) &&
        (s_turnEdgePending != 0U))
    {
        s_lastResult = BLUETOOTH_DEBUG_RESULT_BUTTON_CONFLICT;
        s_forwardEdgePending = 0U;
        s_turnEdgePending = 0U;
        return;
    }
    if ((s_forwardEdgePending == 0U) &&
        (s_turnEdgePending == 0U))
    {
        return;
    }
    if (BluetoothDebug_AutomaticActionIsBusy() != 0U)
    {
        s_lastResult = BLUETOOTH_DEBUG_RESULT_BUSY;
        s_forwardEdgePending = 0U;
        s_turnEdgePending = 0U;
        return;
    }

    if (s_forwardEdgePending != 0U)
    {
        if ((s_control.forwardSpeedMMps <= 0) ||
            (s_control.forwardDistanceMM <= 0))
        {
            s_lastResult = BLUETOOTH_DEBUG_RESULT_INVALID_PARAMETER;
            s_forwardEdgePending = 0U;
            return;
        }
        result = MotionManager_StartForward(
            (uint32_t)s_control.forwardDistanceMM,
            (float)s_control.forwardSpeedMMps, 0.0f);
    }
    else
    {
        if (s_control.turnAngleDeg == 0)
        {
            s_lastResult = BLUETOOTH_DEBUG_RESULT_INVALID_PARAMETER;
            s_turnEdgePending = 0U;
            return;
        }
        result = MotionManager_TurnBy(
            (float)s_control.turnAngleDeg,
            BLUETOOTH_DEBUG_TURN_SPEED_MMPS);
    }

    s_forwardEdgePending = 0U;
    s_turnEdgePending = 0U;
    if (result == MOTION_MANAGER_RESULT_OK)
    {
        s_lastResult = BLUETOOTH_DEBUG_RESULT_OK;
        s_ownsMotion = 1U;
    }
    else if (result == MOTION_MANAGER_RESULT_INVALID_ARGUMENT)
    {
        s_lastResult = BLUETOOTH_DEBUG_RESULT_INVALID_PARAMETER;
    }
    else
    {
        s_lastResult = BLUETOOTH_DEBUG_RESULT_START_FAILED;
    }

    if (result != MOTION_MANAGER_RESULT_OK)
    {
        /* 启动请求失败不保留半初始化的运动模式，由命令结果说明原因。 */
        MotionManager_Stop();
        s_ownsMotion = 0U;
    }
}

void BluetoothDebug_Init(void)
{
    memset(&s_control, 0, sizeof(s_control));
    BluetoothDebug_ResetParser();
    s_newControl = 0U;
    s_forwardEdgePending = 0U;
    s_turnEdgePending = 0U;
    s_previousForwardButton = 0U;
    s_previousTurnButton = 0U;
    s_rearmRequired = 0U;
    s_linkActive = 0U;
    s_commandAllowed = 0U;
    s_ownsMotion = 0U;
    s_ticksSinceControl = 0U;
    s_telemetryTicks = 0U;
    s_txSequence = 0U;
    s_lastResult = BLUETOOTH_DEBUG_RESULT_OK;
}

void BluetoothDebug_Update(uint8_t elapsedTicks)
{
    uint8_t byte;

    while (Serial1_ReadByte(&byte) != 0U)
    {
        BluetoothDebug_ProcessByte(byte);
    }

    if (s_linkActive != 0U)
    {
        uint32_t elapsed = (uint32_t)s_ticksSinceControl + elapsedTicks;
        s_ticksSinceControl = (elapsed > UINT16_MAX) ?
            UINT16_MAX : (uint16_t)elapsed;
        if ((s_ticksSinceControl >= BLUETOOTH_DEBUG_TIMEOUT_TICKS) &&
            (s_rearmRequired == 0U))
        {
            if (s_ownsMotion != 0U)
            {
                MotionManager_Stop();
                s_ownsMotion = 0U;
            }
            s_rearmRequired = 1U;
            s_forwardEdgePending = 0U;
            s_turnEdgePending = 0U;
            s_lastResult = BLUETOOTH_DEBUG_RESULT_TIMEOUT;
        }

        if ((uint16_t)(s_telemetryTicks + elapsedTicks) >=
            BLUETOOTH_DEBUG_TELEMETRY_TICKS)
        {
            if (BluetoothDebug_SendTelemetry() != 0U)
            {
                s_telemetryTicks = 0U;
            }
        }
        else
        {
            s_telemetryTicks = (uint16_t)(s_telemetryTicks + elapsedTicks);
        }
    }
}

void BluetoothDebug_ControlUpdate(uint8_t commandAllowed)
{
    uint8_t actionRequested;

    s_commandAllowed = (commandAllowed != 0U) ? 1U : 0U;

    if (s_commandAllowed == 0U)
    {
        /* 状态机离开等待态后，只释放蓝牙自己启动的运动。 */
        if (s_ownsMotion != 0U)
        {
            MotionManager_Stop();
            s_ownsMotion = 0U;
        }
        if ((s_newControl != 0U) &&
            (BluetoothDebug_ControlHasMotionRequest() != 0U))
        {
            s_lastResult = BLUETOOTH_DEBUG_RESULT_BUSY;
        }
        s_newControl = 0U;
        s_forwardEdgePending = 0U;
        s_turnEdgePending = 0U;
        return;
    }

    if (s_rearmRequired != 0U)
    {
        if (s_newControl != 0U)
        {
            uint8_t joystickIsCentered =
                ((fabsf((float)s_control.joystickXMMps) <=
                  BLUETOOTH_DEBUG_JOYSTICK_DEADZONE_MMPS) &&
                 (fabsf((float)s_control.joystickYMMps) <=
                  BLUETOOTH_DEBUG_JOYSTICK_DEADZONE_MMPS)) ? 1U : 0U;

            if ((joystickIsCentered != 0U) &&
                (s_control.startForward == 0U) &&
                (s_control.startTurn == 0U))
            {
                s_rearmRequired = 0U;
                s_ticksSinceControl = 0U;
                s_lastResult = BLUETOOTH_DEBUG_RESULT_OK;
            }
            else
            {
                s_lastResult = BLUETOOTH_DEBUG_RESULT_REARM_REQUIRED;
            }
        }
        s_newControl = 0U;
        MotionManager_Stop();
        return;
    }

    /* 运行期控制错误必须保留到遥测中，不能立即被摇杆模式覆盖。 */
    if (MotionManager_GetError() != MOTION_MANAGER_ERROR_NONE)
    {
        s_lastResult = BLUETOOTH_DEBUG_RESULT_START_FAILED;
        s_ownsMotion = 0U;
        s_newControl = 0U;
        return;
    }

    actionRequested = ((s_forwardEdgePending != 0U) ||
                       (s_turnEdgePending != 0U)) ? 1U : 0U;
    BluetoothDebug_StartRequestedAction();
    if (actionRequested != 0U)
    {
        s_newControl = 0U;
        return;
    }
    if (BluetoothDebug_AutomaticActionIsBusy() != 0U)
    {
        s_newControl = 0U;
        return;
    }

    if ((MotionManager_GetMode() == MOTION_MANAGER_MODE_STRAIGHT) ||
        (MotionManager_GetMode() == MOTION_MANAGER_MODE_TURN) ||
        (MotionManager_GetMode() == MOTION_MANAGER_MODE_BRAKE))
    {
        MotionManager_Stop();
        s_ownsMotion = 0U;
    }
    BluetoothDebug_UpdateJoystick();
    s_newControl = 0U;
}

uint8_t BluetoothDebug_IsControlling(void)
{
    MotionManager_Mode_t mode = MotionManager_GetMode();

    if (s_ownsMotion == 0U)
    {
        return 0U;
    }

    if ((mode == MOTION_MANAGER_MODE_MANUAL) ||
        (mode == MOTION_MANAGER_MODE_STRAIGHT) ||
        (mode == MOTION_MANAGER_MODE_TURN) ||
        (mode == MOTION_MANAGER_MODE_BRAKE))
    {
        return MotionManager_IsBusy();
    }
    return 0U;
}

BluetoothDebug_Result_t BluetoothDebug_GetLastResult(void)
{
    return s_lastResult;
}
