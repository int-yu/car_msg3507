#ifndef HARDWARE_MOTOR_F32C_H
#define HARDWARE_MOTOR_F32C_H

#include <stdint.h>

/* F32C 协议参数，依据《F32C无刷电机使用手册》通信协议章节。 */
#define F32C_MIN_ADDRESS                    1U
#define F32C_MAX_SPEED_RPM                  1000
#define F32C_MAX_MULTI_TURN_ANGLE_DEG       214748300.0f
#define F32C_COMMAND_INTERVAL_MS            1U  /* 多电机指令间隔不得小于 1 ms。 */

typedef enum
{
    F32C_RESULT_OK = 0,
    F32C_RESULT_INVALID_ARGUMENT,
    F32C_RESULT_FRAME_ERROR
} F32C_Result_t;

typedef enum
{
    F32C_FEEDBACK_SPEED = 0,
    F32C_FEEDBACK_TOTAL_ANGLE = 1,
    F32C_FEEDBACK_MECHANICAL_ANGLE = 2,
    F32C_FEEDBACK_ACCELERATION = 3,
    F32C_FEEDBACK_BUS_VOLTAGE = 4
} F32C_FeedbackType_t;

typedef enum
{
    F32C_MODE_SPEED = 0,
    F32C_MODE_MULTI_TURN_T = 1,
    F32C_MODE_SINGLE_TURN_T = 2,
    F32C_MODE_MULTI_TURN_DIRECT = 3,
    F32C_MODE_SINGLE_TURN_DIRECT = 4
} F32C_Mode_t;

typedef struct
{
    uint8_t address;
    F32C_FeedbackType_t type;
    int32_t rawValue;
} F32C_Feedback_t;

void F32C_Init(void);
F32C_Result_t F32C_Enable(uint8_t address);
F32C_Result_t F32C_Disable(uint8_t address);
F32C_Result_t F32C_SetMode(uint8_t address, F32C_Mode_t mode);
F32C_Result_t F32C_SetSpeedRPM(uint8_t address, int16_t speedRPM);
F32C_Result_t F32C_SetAcceleration(
    uint8_t address, uint16_t accelerationRPMS2);
F32C_Result_t F32C_ClearMultiTurnAngle(uint8_t address);
F32C_Result_t F32C_SetMultiTurnPositionDegrees(
    uint8_t address, float targetAngleDeg);
F32C_Result_t F32C_RequestFeedback(
    uint8_t address, F32C_FeedbackType_t type);

/* 从 UART2 接收缓冲区解析一帧反馈；没有完整有效帧时返回 0。 */
uint8_t F32C_PopFeedback(F32C_Feedback_t *feedback);

#endif
