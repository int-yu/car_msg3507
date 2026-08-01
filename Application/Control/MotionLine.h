#ifndef APPLICATION_CONTROL_MOTION_LINE_H
#define APPLICATION_CONTROL_MOTION_LINE_H

#include <stdint.h>

/* Six-channel infrared line follower. A cached bit of 1 means that channel
 * currently sees the learned black target color. CH1 is the leftmost sensor
 * when GRAYDETECT_CHANNEL1_IS_RIGHT is 0.
 */
#define MOTION_LINE_OUTER_WEIGHT       9
#define MOTION_LINE_INNER_WEIGHT       2.5f
#define MOTION_LINE_KP_MMPS_PER_WEIGHT 26.0f
#define MOTION_LINE_KI_MMPS_PER_WEIGHT 0.0f
#define MOTION_LINE_KD_MMPS_PER_WEIGHT 1.0f
#define MOTION_LINE_ACCELERATION_MMPS2 300.0f
#define MOTION_LINE_DECELERATION_MMPS2 360.0f
#define MOTION_LINE_WEIGHT_FILTER_ALPHA 0.25f
#define MOTION_LINE_LOST_CONFIRM_TICKS 160U

/* Runtime tunings for the general line controller used outside requirement 2.
 * MotionLine_Start() snapshots them; writes affect only the next start.
 */
extern float MotionLine_TuneKpMMpsPerWeight;
extern float MotionLine_TuneKiMMpsPerWeight;
extern float MotionLine_TuneKdMMpsPerWeight;
extern float MotionLine_TuneAccelerationMMps2;
extern float MotionLine_TuneDecelerationMMps2;

typedef enum
{
    MOTION_LINE_STATE_IDLE = 0,
    MOTION_LINE_STATE_RUNNING,
    MOTION_LINE_STATE_FINISHED,
    MOTION_LINE_STATE_ERROR
} MotionLine_State_t;

typedef enum
{
    MOTION_LINE_ERROR_NONE = 0,
    MOTION_LINE_ERROR_UPDATE_PERIOD_INVALID,
    MOTION_LINE_ERROR_WHEEL
} MotionLine_Error_t;

typedef enum
{
    MOTION_LINE_RESULT_OK = 0,
    MOTION_LINE_RESULT_BUSY,
    MOTION_LINE_RESULT_INVALID_ARGUMENT,
    MOTION_LINE_RESULT_NOT_CONFIGURED
} MotionLine_Result_t;

MotionLine_Result_t MotionLine_Init(void);
MotionLine_Result_t MotionLine_Start(float speedMMps);
MotionLine_Result_t MotionLine_SetSpeed(float speedMMps);
MotionLine_Result_t MotionLine_RequestStop(void);
void MotionLine_Update(float dt);
void MotionLine_Stop(void);

uint8_t MotionLine_IsConfigured(void);
uint8_t MotionLine_IsBusy(void);
uint8_t MotionLine_IsFinished(void);
MotionLine_State_t MotionLine_GetState(void);
MotionLine_Error_t MotionLine_GetError(void);
float MotionLine_GetLineError(void);
float MotionLine_GetProfileSpeedMMps(void);
float MotionLine_GetProfileAccelerationMMps2(void);

#endif
