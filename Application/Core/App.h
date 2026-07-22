#ifndef APPLICATION_CORE_APP_H
#define APPLICATION_CORE_APP_H

#include <stdint.h>

/* 每个有效系统节拍向上层提供真实时间和按键状态。 */
typedef struct
{
    uint8_t elapsedTicks;
    float dt;
    uint8_t pressedKeys;
    uint8_t pressedEdges;
} App_UpdateContext_t;

void App_Init(void);
uint8_t App_Update(App_UpdateContext_t *context);

#endif
