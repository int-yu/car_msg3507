#ifndef APPLICATION_COMMS_BLUETOOTH_DEBUG_H
#define APPLICATION_COMMS_BLUETOOTH_DEBUG_H

#include <stdint.h>

/* 命令没有结束符时，连续空闲达到以下 100 Hz 节拍数后执行。 */
#define BLUETOOTH_COMMAND_IDLE_TICKS 3U
#define BLUETOOTH_TASK_SIGNAL_MAX    255U

void BluetoothDebug_Init(void);
void BluetoothDebug_Update(uint8_t elapsedTicks,
                           uint8_t manualMotorEnabled);
uint8_t BluetoothDebug_PopSignal(uint8_t *signal);
int16_t BluetoothDebug_GetLeftCommand(void);
int16_t BluetoothDebug_GetRightCommand(void);

#endif
