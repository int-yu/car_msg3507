#ifndef ACCOMPLISH_BRUSHLESS_MOTOR_TEST_H
#define ACCOMPLISH_BRUSHLESS_MOTOR_TEST_H

#include "Application/Mission/Mission.h"

/* 开机完成 App 初始化后自动启动；KEY2 主动停止并失能两个电机。 */
#define BRUSHLESS_MOTOR_TEST_STOP_KEY_MASK       0x02U

/* X 轴每次沿右转方向增加 180°；方向相反时将符号改为 -1.0f。 */
#define BRUSHLESS_MOTOR_TEST_X_RIGHT_SIGN        1.0f
#define BRUSHLESS_MOTOR_TEST_X_STEP_DEG          180.0f

/* Y 轴在两个多圈绝对目标之间往返。 */
#define BRUSHLESS_MOTOR_TEST_Y_LOW_DEG           0.0f
#define BRUSHLESS_MOTOR_TEST_Y_HIGH_DEG          180.0f

const Mission_GraphDefinition_t *
BrushlessMotorTest_GetMissionGraph(void);

#endif
