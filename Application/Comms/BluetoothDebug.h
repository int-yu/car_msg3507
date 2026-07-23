#ifndef APPLICATION_COMMS_BLUETOOTH_DEBUG_H
#define APPLICATION_COMMS_BLUETOOTH_DEBUG_H

#include <stdint.h>

/* 命令没有结束符时，连续空闲达到以下 100 Hz 节拍数后执行。 */
#define BLUETOOTH_COMMAND_IDLE_TICKS 3U
#define BLUETOOTH_TASK_SIGNAL_MAX    255U

/* 网页调试运动命令的参数范围与默认巡航速度。 */
#define BLUETOOTH_DEBUG_MIN_SPEED_MMPS     20
#define BLUETOOTH_DEBUG_MAX_SPEED_MMPS     800
#define BLUETOOTH_DEBUG_DEFAULT_SPEED_MMPS 200
#define BLUETOOTH_DEBUG_MAX_DISTANCE_MM    10000
#define BLUETOOTH_DEBUG_MAX_ANGLE_DEG      3600

void BluetoothDebug_Init(void);
void BluetoothDebug_Update(uint8_t elapsedTicks,
                           uint8_t manualMotorEnabled);
uint8_t BluetoothDebug_PopSignal(uint8_t *signal);
int16_t BluetoothDebug_GetLeftCommand(void);
int16_t BluetoothDebug_GetRightCommand(void);

#endif
