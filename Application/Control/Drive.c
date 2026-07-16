#include "Application/Control/Drive.h"
#include "Application/Control/DriveConfig.h"
#include "Application/Control/PID.h"
#include "Application/State/Heading.h"
#include "Application/State/Odometry.h"
#include "Hardware/Motor/Motor.h"
#include "Hardware/Motor/PWM.h"
#include <math.h>
#include <stddef.h>

static Drive_Config_t s_config;
static PID_t s_leftSpeedPID;
static PID_t s_rightSpeedPID;
static PID_t s_headingPID;

static Drive_State_t s_state = DRIVE_STATE_IDLE;
static Drive_Error_t s_error = DRIVE_ERROR_NONE;
static uint8_t s_configured;

static float s_startDistanceLMM;
static float s_startDistanceRMM;
static float s_targetDistanceMM;
static float s_remainingDistanceMM;
static float s_cruiseSpeedMMps;
static float s_profileSpeedMMps;
static float s_targetHeadingDeg;
static float s_direction;
static float s_brakeTimeRemainingS;

static float Drive_Clamp(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static float Drive_Approach(float current, float target, float maximumStep)
{
    if (current < target)
    {
        current += maximumStep;
        return (current > target) ? target : current;
    }
    if (current > target)
    {
        current -= maximumStep;
        return (current < target) ? target : current;
    }
    return current;
}

static uint8_t Drive_ConfigIsValid(const Drive_Config_t *config)
{
    if (config == NULL)
    {
        return 0U;
    }
    if ((!isfinite(config->maximumSpeedMMps)) ||
        (!isfinite(config->maximumCommandPWM)) ||
        (!isfinite(config->accelerationMMps2)) ||
        (!isfinite(config->decelerationMMps2)) ||
        (!isfinite(config->minimumApproachSpeedMMps)) ||
        (!isfinite(config->distanceToleranceMM)) ||
        (!isfinite(config->brakeDurationS)))
    {
        return 0U;
    }
    if ((config->maximumSpeedMMps <= 0.0f) ||
        (config->maximumCommandPWM <= 0.0f) ||
        (config->maximumCommandPWM > (float)PWM_MAX_DUTY) ||
        (config->accelerationMMps2 <= 0.0f) ||
        (config->decelerationMMps2 <= 0.0f) ||
        (config->minimumApproachSpeedMMps < 0.0f) ||
        (config->minimumApproachSpeedMMps > config->maximumSpeedMMps) ||
        (config->distanceToleranceMM < 0.0f) ||
        (config->brakeDurationS < 0.0f))
    {
        return 0U;
    }
    if ((!isfinite(config->speed.kp)) ||
        (!isfinite(config->speed.ki)) ||
        (!isfinite(config->speed.integralLimit)) ||
        (!isfinite(config->speed.feedforwardPWMPerMMps)) ||
        (!isfinite(config->speed.staticFrictionPWM)) ||
        (config->speed.kp < 0.0f) ||
        (config->speed.ki < 0.0f) ||
        (config->speed.integralLimit < 0.0f) ||
        ((config->speed.ki > 0.0f) &&
         (config->speed.integralLimit <= 0.0f)) ||
        (config->speed.feedforwardPWMPerMMps < 0.0f) ||
        (config->speed.staticFrictionPWM < 0.0f) ||
        (config->speed.staticFrictionPWM > config->maximumCommandPWM))
    {
        return 0U;
    }
    if ((config->speed.kp == 0.0f) &&
        (config->speed.ki == 0.0f) &&
        (config->speed.feedforwardPWMPerMMps == 0.0f))
    {
        return 0U;
    }
    if ((!isfinite(config->heading.kp)) ||
        (!isfinite(config->heading.kd)) ||
        (!isfinite(config->heading.correctionLimitPWM)) ||
        (config->heading.kp < 0.0f) ||
        (config->heading.kd < 0.0f) ||
        ((config->heading.kp == 0.0f) &&
         (config->heading.kd == 0.0f)) ||
        (config->heading.correctionLimitPWM <= 0.0f) ||
        (config->heading.correctionLimitPWM > config->maximumCommandPWM) ||
        ((config->heading.correctionSign != 1) &&
         (config->heading.correctionSign != -1)))
    {
        return 0U;
    }
    return 1U;
}

static float Drive_SpeedFeedforward(float targetSpeedMMps)
{
    float output;

    if (fabsf(targetSpeedMMps) < 0.001f)
    {
        return 0.0f;
    }

    output = targetSpeedMMps * s_config.speed.feedforwardPWMPerMMps;
    output += (targetSpeedMMps > 0.0f) ?
                  s_config.speed.staticFrictionPWM :
                  -s_config.speed.staticFrictionPWM;
    return output;
}

static int16_t Drive_ToMotorCommand(float value)
{
    value = Drive_Clamp(value,
                        -s_config.maximumCommandPWM,
                        s_config.maximumCommandPWM);
    value += (value >= 0.0f) ? 0.5f : -0.5f;
    return (int16_t)value;
}

static void Drive_ResetControllers(void)
{
    PID_Reset(&s_leftSpeedPID);
    PID_Reset(&s_rightSpeedPID);
    PID_Reset(&s_headingPID);
    s_profileSpeedMMps = 0.0f;
}

static void Drive_SetError(Drive_Error_t error)
{
    Motor_StopAll();
    s_error = error;
    s_state = DRIVE_STATE_ERROR;
}

static void Drive_BeginBraking(void)
{
    s_profileSpeedMMps = 0.0f;
    s_brakeTimeRemainingS = s_config.brakeDurationS;

    if (s_brakeTimeRemainingS > 0.0f)
    {
        Motor_Brake();
        s_state = DRIVE_STATE_BRAKING;
    }
    else
    {
        Motor_StopAll();
        s_state = DRIVE_STATE_COMPLETED;
    }
}

Drive_Result_t Drive_Init(const Drive_Config_t *config)
{
    Motor_StopAll();
    s_configured = 0U;
    s_state = DRIVE_STATE_IDLE;
    s_error = DRIVE_ERROR_NONE;

    if (Drive_ConfigIsValid(config) == 0U)
    {
        return DRIVE_RESULT_INVALID_ARGUMENT;
    }

    s_config = *config;
    PID_Init(&s_leftSpeedPID,
             s_config.speed.kp, s_config.speed.ki, 0.0f,
             s_config.maximumCommandPWM, s_config.speed.integralLimit);
    PID_Init(&s_rightSpeedPID,
             s_config.speed.kp, s_config.speed.ki, 0.0f,
             s_config.maximumCommandPWM, s_config.speed.integralLimit);
    PID_Init(&s_headingPID,
             s_config.heading.kp, 0.0f, s_config.heading.kd,
             s_config.heading.correctionLimitPWM, 0.0f);

    Drive_ResetControllers();
    s_remainingDistanceMM = 0.0f;
    s_configured = 1U;
    return DRIVE_RESULT_OK;
}

Drive_Result_t Drive_InitDefault(void)
{
    return Drive_Init(&g_driveConfig);
}

Drive_Result_t Drive_StartStraight(float distanceMM, float speedMMps)
{
    if (s_configured == 0U)
    {
        return DRIVE_RESULT_NOT_CONFIGURED;
    }
    if (Drive_IsBusy() != 0U)
    {
        return DRIVE_RESULT_BUSY;
    }
    if ((!isfinite(distanceMM)) || (!isfinite(speedMMps)) ||
        (speedMMps <= 0.0f))
    {
        return DRIVE_RESULT_INVALID_ARGUMENT;
    }
    if ((Heading_IsReady() == 0U) ||
        (Odometry_CountsPerMM <= 0.001f))
    {
        return DRIVE_RESULT_SENSOR_NOT_READY;
    }

    Motor_StopAll();
    Drive_ResetControllers();
    s_error = DRIVE_ERROR_NONE;
    s_startDistanceLMM = Odometry_GetDistanceLMM();
    s_startDistanceRMM = Odometry_GetDistanceRMM();
    s_targetDistanceMM = distanceMM;
    s_remainingDistanceMM = distanceMM;
    s_cruiseSpeedMMps = Drive_Clamp(speedMMps,
                                     0.0f,
                                     s_config.maximumSpeedMMps);
    s_targetHeadingDeg = Heading_GetYaw();
    s_direction = (distanceMM >= 0.0f) ? 1.0f : -1.0f;

    if (fabsf(distanceMM) <= s_config.distanceToleranceMM)
    {
        s_remainingDistanceMM = 0.0f;
        s_state = DRIVE_STATE_COMPLETED;
        return DRIVE_RESULT_OK;
    }

    s_state = DRIVE_STATE_RUNNING;
    return DRIVE_RESULT_OK;
}

Drive_Result_t Drive_StartForward(uint32_t distanceMM, Drive_Speed_t speed)
{
    return Drive_StartStraight((float)distanceMM, (float)speed);
}

Drive_Result_t Drive_StartBackward(uint32_t distanceMM, Drive_Speed_t speed)
{
    return Drive_StartStraight(-(float)distanceMM, (float)speed);
}

void Drive_Update(float dt)
{
    float distanceLMM;
    float distanceRMM;
    float averageDistanceMM;
    float directedRemainingMM;
    float brakingSpeedMMps;
    float targetSpeedMagnitudeMMps;
    float targetSpeedMMps;
    float profileStepMMps;
    float speedFeedforwardPWM;
    float leftSpeedCorrectionPWM;
    float rightSpeedCorrectionPWM;
    float headingCorrectionPWM;
    float leftCommandPWM;
    float rightCommandPWM;

    if ((s_state != DRIVE_STATE_RUNNING) &&
        (s_state != DRIVE_STATE_BRAKING))
    {
        return;
    }
    if ((!isfinite(dt)) || (dt <= 0.0f))
    {
        Drive_SetError(DRIVE_ERROR_UPDATE_PERIOD_INVALID);
        return;
    }
    if (s_state == DRIVE_STATE_BRAKING)
    {
        s_brakeTimeRemainingS -= dt;
        if (s_brakeTimeRemainingS <= 0.0f)
        {
            Motor_StopAll();
            s_state = DRIVE_STATE_COMPLETED;
        }
        return;
    }
    if (Heading_IsReady() == 0U)
    {
        Drive_SetError(DRIVE_ERROR_HEADING_OFFLINE);
        return;
    }
    if (Odometry_CountsPerMM <= 0.001f)
    {
        Drive_SetError(DRIVE_ERROR_ODOMETRY_INVALID);
        return;
    }

    distanceLMM = Odometry_GetDistanceLMM() - s_startDistanceLMM;
    distanceRMM = Odometry_GetDistanceRMM() - s_startDistanceRMM;
    averageDistanceMM = (distanceLMM + distanceRMM) * 0.5f;
    s_remainingDistanceMM = s_targetDistanceMM - averageDistanceMM;
    directedRemainingMM = s_direction * s_remainingDistanceMM;

    /* 距离层：进入允许误差后执行短时主动制动。 */
    if (directedRemainingMM <= s_config.distanceToleranceMM)
    {
        Drive_BeginBraking();
        return;
    }

    /* 速度规划层：按剩余制动距离限制目标速度，并限制加减速度。 */
    brakingSpeedMMps = sqrtf(2.0f * s_config.decelerationMMps2 *
                             directedRemainingMM);
    targetSpeedMagnitudeMMps = Drive_Clamp(brakingSpeedMMps,
                                           0.0f,
                                           s_cruiseSpeedMMps);
    if (targetSpeedMagnitudeMMps < s_config.minimumApproachSpeedMMps)
    {
        targetSpeedMagnitudeMMps = Drive_Clamp(
            s_config.minimumApproachSpeedMMps,
            0.0f, s_cruiseSpeedMMps);
    }
    targetSpeedMMps = s_direction * targetSpeedMagnitudeMMps;

    if (fabsf(targetSpeedMMps) > fabsf(s_profileSpeedMMps))
    {
        profileStepMMps = s_config.accelerationMMps2 * dt;
    }
    else
    {
        profileStepMMps = s_config.decelerationMMps2 * dt;
    }
    s_profileSpeedMMps = Drive_Approach(s_profileSpeedMMps,
                                         targetSpeedMMps,
                                         profileStepMMps);

    /* 速度层：左右轮分别使用编码器速度 PI，并叠加标定前馈。 */
    speedFeedforwardPWM = Drive_SpeedFeedforward(s_profileSpeedMMps);
    leftSpeedCorrectionPWM = PID_Update(
        &s_leftSpeedPID,
        s_profileSpeedMMps, Odometry_GetSpeedL(), dt);
    rightSpeedCorrectionPWM = PID_Update(
        &s_rightSpeedPID,
        s_profileSpeedMMps, Odometry_GetSpeedR(), dt);

    /* 航向层：直接使用连续累计角度，不对多圈角度做 ±180° 归一化。 */
    headingCorrectionPWM = PID_Update(
        &s_headingPID, s_targetHeadingDeg, Heading_GetYaw(), dt);
    headingCorrectionPWM *= (float)s_config.heading.correctionSign;

    /* 将公共速度输出和航向差速合成最终左右轮 PWM。 */
    leftCommandPWM = speedFeedforwardPWM + leftSpeedCorrectionPWM -
                     headingCorrectionPWM;
    rightCommandPWM = speedFeedforwardPWM + rightSpeedCorrectionPWM +
                      headingCorrectionPWM;
    Motor_SetPWM(Drive_ToMotorCommand(leftCommandPWM),
                 Drive_ToMotorCommand(rightCommandPWM));
}

void Drive_Stop(void)
{
    Motor_StopAll();
    Drive_ResetControllers();
    s_remainingDistanceMM = 0.0f;
    s_error = DRIVE_ERROR_NONE;
    s_state = DRIVE_STATE_IDLE;
}

uint8_t Drive_IsConfigured(void)
{
    return s_configured;
}

uint8_t Drive_IsBusy(void)
{
    return ((s_state == DRIVE_STATE_RUNNING) ||
            (s_state == DRIVE_STATE_BRAKING)) ? 1U : 0U;
}

uint8_t Drive_IsFinished(void)
{
    return (s_state == DRIVE_STATE_COMPLETED) ? 1U : 0U;
}

Drive_State_t Drive_GetState(void)
{
    return s_state;
}

Drive_Error_t Drive_GetError(void)
{
    return s_error;
}

float Drive_GetRemainingDistanceMM(void)
{
    return s_remainingDistanceMM;
}
