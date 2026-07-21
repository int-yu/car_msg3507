#ifndef APPLICATION_CORE_TEST_APP_H
#define APPLICATION_CORE_TEST_APP_H

#include "Application/Core/App.h"

/*
 * 测试专用运行通道：只初始化测试流程需要的快速模块，跳过 OLED、MPU6050、
 * 灰度、里程和蜂鸣器等完整整车服务。
 */
void TestApp_Init(void);
uint8_t TestApp_Update(App_UpdateContext_t *context);

#endif
