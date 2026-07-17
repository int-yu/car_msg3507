#ifndef APPLICATION_CORE_APP_H
#define APPLICATION_CORE_APP_H

#include <stdint.h>

/* 每次有效系统更新提供给 Mission 的只读事件和时间数据。 */
typedef struct
{
    uint8_t elapsedTicks;
    float dt;
    uint8_t pressedKeys;
    uint8_t pressedEdges;
    uint8_t hasBluetoothSignal;
    uint8_t bluetoothSignal;
} App_UpdateContext_t;

void App_Init(void);
uint8_t App_Update(App_UpdateContext_t *context);

#endif
