#ifndef APPLICATION_COMMS_BLUETOOTH_DEBUG_H
#define APPLICATION_COMMS_BLUETOOTH_DEBUG_H

#include <stdint.h>

/* 命令没有结束符时，连续空闲达到以下 100 Hz 节拍数后执行。 */
#define BLUETOOTH_COMMAND_IDLE_TICKS 3U

void BluetoothDebug_Init(void);
void BluetoothDebug_Update(uint8_t elapsedTicks);
int16_t BluetoothDebug_GetLeftCommand(void);
int16_t BluetoothDebug_GetRightCommand(void);

#endif
