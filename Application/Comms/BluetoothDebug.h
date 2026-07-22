#ifndef APPLICATION_COMMS_BLUETOOTH_DEBUG_H
#define APPLICATION_COMMS_BLUETOOTH_DEBUG_H

#include <stdint.h>

/* 蓝牙调试台固定帧参数。 */
#define BLUETOOTH_DEBUG_FRAME_HEAD              0x7AU
#define BLUETOOTH_DEBUG_FRAME_TAIL              0x7BU
#define BLUETOOTH_DEBUG_PROTOCOL_VERSION        0x01U
#define BLUETOOTH_DEBUG_MESSAGE_TELEMETRY       0x01U
#define BLUETOOTH_DEBUG_MESSAGE_CONTROL         0x80U
#define BLUETOOTH_DEBUG_CONTROL_PAYLOAD_LENGTH  16U
#define BLUETOOTH_DEBUG_TELEMETRY_PAYLOAD_LENGTH 21U
#define BLUETOOTH_DEBUG_MAX_PAYLOAD_LENGTH      32U

/* 100 Hz 系统节拍下，10 拍回传一次，50 拍未收到控制帧则停车。 */
#define BLUETOOTH_DEBUG_TELEMETRY_TICKS         10U
#define BLUETOOTH_DEBUG_TIMEOUT_TICKS           50U

/* 调试运动参数，可根据实车表现调整。 */
#define BLUETOOTH_DEBUG_TURN_SPEED_MMPS          100.0f
#define BLUETOOTH_DEBUG_MANUAL_MAX_SPEED_MMPS    500.0f
#define BLUETOOTH_DEBUG_JOYSTICK_DEADZONE_MMPS   10.0f

typedef enum
{
    BLUETOOTH_DEBUG_RESULT_OK = 0,
    BLUETOOTH_DEBUG_RESULT_INVALID_PARAMETER = 1,
    BLUETOOTH_DEBUG_RESULT_BUSY = 2,
    BLUETOOTH_DEBUG_RESULT_START_FAILED = 3,
    BLUETOOTH_DEBUG_RESULT_BUTTON_CONFLICT = 4,
    BLUETOOTH_DEBUG_RESULT_TIMEOUT = 5,
    BLUETOOTH_DEBUG_RESULT_REARM_REQUIRED = 6
} BluetoothDebug_Result_t;

/* 网页调试运动命令的参数范围与默认巡航速度。 */
#define BLUETOOTH_DEBUG_MIN_SPEED_MMPS     20
#define BLUETOOTH_DEBUG_MAX_SPEED_MMPS     800
#define BLUETOOTH_DEBUG_DEFAULT_SPEED_MMPS 200
#define BLUETOOTH_DEBUG_MAX_DISTANCE_MM    10000
#define BLUETOOTH_DEBUG_MAX_ANGLE_DEG      3600

void BluetoothDebug_Init(void);
/* 读取并解析串口帧，同时处理超时和周期遥测。 */
void BluetoothDebug_Update(uint8_t elapsedTicks);
/* 仅在 Mission 等待状态允许执行命令；其他状态只接收和回传，不接管车轮。 */
void BluetoothDebug_ControlUpdate(uint8_t commandAllowed);

uint8_t BluetoothDebug_IsControlling(void);
BluetoothDebug_Result_t BluetoothDebug_GetLastResult(void);

#endif
